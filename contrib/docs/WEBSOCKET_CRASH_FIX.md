# WebSocket Crash Fix — Polling Mode

**Date:** 2025-11-07
**Updated:** 2026-03-12
**Issue:** Segmentation fault (SIGSEGV) at address 0x00000078
**Root Cause:** WebSocket library causing crashes in Flame OFX host environment

## Problem

The plugin crashed when monitoring workflow execution via WebSocket:

```text
Error: abnormal termination, signal = 11
SIGSEGV - Segmentation Fault
Signal was generated internally:
invalid permissions for mapped object at address 0x00000078
```

The crash occurred during `monitorExecution()`, which used ixwebsocket to receive real-time updates from ComfyUI server.

## Root Cause

WebSocket libraries are fragile in OFX plugin environments due to:

- Signal handling conflicts with the host application
- Threading issues in restricted plugin contexts
- Memory access violations in shared libraries
- Host application terminating the plugin while a WebSocket connection is active

**This is not a ComfyUI server compatibility issue.** The latest ComfyUI server supports WebSockets correctly. The problem is the Flame OFX runtime — background threads with blocking network I/O can conflict with Flame's threading model during render cancellation or plugin unload.

## Solution: HTTP Polling

Replaced real-time WebSocket monitoring with HTTP polling against `/history/{prompt_id}`:

```cpp
// Before (WebSocket mode):
_comfyClient->monitorExecution(promptId,
    [&](EventType eventType, const json& data) {
        // Handle events via WebSocket callbacks
    });

// After (HTTP polling):
const int maxAttempts = 300; // 5 minutes at 1-second intervals
for (int attempt = 0; attempt < maxAttempts; ++attempt) {
    json history = _comfyClient->getHistory(promptId);
    if (history.contains(promptId)) {
        std::string status = history[promptId]["status"]["status_str"];
        if (status == "success") { completed = true; break; }
    }
    std::this_thread::sleep_for(std::chrono::seconds(1));
}
```

## Why Not Just Fix the WebSocket?

Three alternatives were evaluated before settling on polling permanently.

### Alternative 1: Out-of-process sidecar (cleanest, safest)

Run a small standalone daemon that maintains the WebSocket connection to ComfyUI
and exposes a simple Unix socket / named pipe IPC to the plugin. The plugin makes
quick local IPC calls; the sidecar lifecycle is fully independent of Flame.

- **Pro:** Zero crash risk, real-time updates possible, reusable across plugins
- **Con:** Extra process to deploy and keep alive; adds operational complexity

### Alternative 2: Managed WebSocket on the existing async thread

The async job manager already owns a background thread. If the WebSocket were managed
on that thread — with an atomic stop flag and a proper `join()` in the destructor —
it might work safely.

- **Pro:** No extra processes; real-time progress available
- **Con:** Flame's teardown behavior during render cancel is unpredictable;
  risk of re-introducing the crash; requires extensive testing

### Alternative 3: Non-blocking WebSocket poll (no background thread)

Use a non-blocking WebSocket library (e.g. `libwebsockets` in event-driven mode)
and drive `ws.poll()` from the existing HTTP polling loop — no extra threads at all.

- **Pro:** Clean threading model; no background thread lifecycle issues
- **Con:** Most WebSocket libraries are callback/blocking-oriented; adapting them
  to a poll model is awkward and library-dependent

### Decision

HTTP polling is retained because it is proven stable, the latency trade-off is
irrelevant for workflows that take seconds to minutes, and the alternatives carry
either deployment cost (sidecar) or re-crash risk (thread management).

Revisit if real-time node-by-node progress inside Flame's UI becomes a hard requirement.

## Side Effect: `client_id` Warning on ComfyUI Server (fixed 2026-03-12)

While not using WebSockets, the plugin was still sending `client_id` in HTTP prompt
submissions. ComfyUI uses this field to route WebSocket completion notifications back
to the submitting client. With no active WebSocket session registered for that
`client_id`, ComfyUI logged:

```text
WARNING: 'NoneType' object has no attribute 'session_uuid'
```

**Fix:** `client_id` is now omitted from the HTTP prompt payload — `queuePrompt()`
call sites pass `""`, which suppresses the field. ComfyUI skips WebSocket notification
when no `client_id` is present. The `monitorExecution()` WebSocket function and
`getClientId()` remain in `comfyui_client.cpp` should WebSocket mode be revived.

## Trade-offs of Current Approach

- **Latency:** 1-second polling interval vs. real-time WebSocket updates
  — acceptable for AI workflows that take seconds to minutes
- **Network overhead:** One HTTP request per second while a workflow runs
  — minimal impact

## Benefits

- No crashes — simple HTTP polling is stable in all OFX hosts
- Graceful error handling for network issues
- Progress updates still provided to the user
- Timeout protection (5 minutes by default)
- Compatible with all OFX hosts, not just Flame

## Files Modified

- `contrib/plugins/ComfyUI/common/comfyui_base_plugin.cpp` — polling implementation
- `contrib/plugins/ComfyUI/common/async_job_manager.cpp` — removed `client_id` from submissions
- `contrib/plugins/ComfyUI/common/comfyui_client.cpp` — `queuePrompt` omits `client_id` when empty

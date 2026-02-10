# DaVinci Resolve — Browser Open Fix

**Date:** 2026-02-03
**Affects:** "New Workflow" and "Edit Workflow" buttons in AnyComfy
**Hosts affected:** DaVinci Resolve only. Flame/Flare unaffected.
**Log source:** `/Volumes/silo2/002_COMFYUI/ofx/plugins/logs/comfyui_plugin_20260203.log`

---

## Two bugs, same symptom

Both buttons are supposed to open a browser to ComfyUI. Both work in Flame/Flare.
Neither works in Resolve. The root causes are different and independent.

---

## Bug 1 — "New Workflow" fails before reaching the browser

### Symptom

Every press of "New Workflow" in Resolve logged the same sequence (lines 5547–5591,
repeated 8+ times during the session):

```log
[info]  Create New Workflow button pressed
[info]  Creating template workflow
[info]  Generated unique workflow name: workflow_1770127937
[info]  Creating workflow subdirectory: /Volumes/silo2/002_COMFYUI/in/workflows/workflow_1770127937
[info]  User selected 1 input(s) for new workflow
[warn]  Plugin path does not contain .ofx.bundle:           ← empty string
[error] Template file not found at:                          ← empty string
[error] Failed to create template workflow: Template workflow file not found in bundle resources: workflows/template.json
```

The template file is never loaded, so `createTemplateWorkflow()` throws. The browser
open call is never reached.

### Root cause

`getBundleResourcePath()` (`comfyui_base_plugin.cpp:1734`) derives the bundle's
`Resources/` directory from `kOfxPluginPropFilePath`. Resolve does not populate this
property — it returns an empty string. Confirmed by the environment dump that fires
on every instance creation:

```log
# Flame (works):
[info] Plugin File Path: '/Library/OFX/Plugins/AnyComfy.ofx.bundle'

# Resolve (fails):
[info] Plugin File Path: ''
```

This is a Resolve host behaviour, not something the plugin can force.

### Fix

`comfyui_base_plugin.cpp` — when `kOfxPluginPropFilePath` is empty, fall back to
`dladdr()` with a symbol from the plugin's own shared library. `dladdr` asks the
dynamic linker directly for the path of the dylib that contains the given address,
which returns the `.ofx` binary inside the bundle regardless of what the host reports.

```cpp
if (pluginPath.empty()) {
    Dl_info info;
    if (dladdr(reinterpret_cast<const void*>(&BasePlugin::getBundleResourcePath), &info) && info.dli_fname) {
        pluginPath = info.dli_fname;   // e.g. /Library/OFX/Plugins/AnyComfy.ofx.bundle/Contents/MacOS/AnyComfy.ofx
    }
}
// existing .ofx.bundle search continues unchanged
```

### Why not other approaches

| Approach | Problem |
|---|---|
| Force-populate `kOfxPluginPropFilePath` | Host-owned property; plugin cannot write it |
| Alternative OFX property | No standard alternative exists |
| `_NSGetExecutablePath` | Returns the **host app** path (e.g. `/Applications/DaVinci Resolve.app/...`), not the plugin dylib. Would hit the same `.ofx.bundle` check and fail. |
| Hardcode `/Library/OFX/Plugins/AnyComfy.ofx.bundle` | Breaks if installed to `~/Library/OFX/Plugins/` or any custom path |

---

## Bug 2 — "Edit Workflow" reaches the browser call but nothing opens

### Symptom

The "Edit Workflow" button does not need the template file, so it gets all the way
through to `openComfyUIInBrowser`. The URL is constructed and logged cleanly (lines
9445–9448). No error is ever logged. The browser simply does not open.

```log
[info] Opening ComfyUI in browser with workflow: AnyComfy_1770127540
[info] Opening URL with auto-load: http://192.168.1.211:8188?load_local_json=workflows/AnyComfy_1770127540/AnyComfy_1770127540.json
[info] Workflow will be loaded from and saved to: /Volumes/silo2/002_COMFYUI/in/workflows/AnyComfy_1770127540/AnyComfy_1770127540.json
[info] Note: Requires OFX.AutoLoader extension in ComfyUI/web/extensions/
# ← nothing. no error. no browser.
```

The code that ran was:
```cpp
std::string command = "open \"" + url + "\"";
int result = std::system(command.c_str());   // returns 0, but browser never opens
```

### Root cause

Resolve sandboxes plugin subprocesses. `std::system()` forks the current process;
the child inherits the sandbox and the `open` command runs but has no access to the
user's display session to actually launch a browser. It exits 0 (no error from its
own perspective), so the plugin never logs a failure.

### Fix

`open_url.mm` — a thin Objective-C++ wrapper that calls
`[[NSWorkspace sharedWorkspace] openURL:configuration:completionHandler:]`.
`anycomfy_plugin.cpp` calls it via an `extern "C"` declaration (`ofx_open_url`).
`AppKit` framework added to the link step in `anycomfy/CMakeLists.txt`.

```objc
// open_url.mm
[[NSWorkspace sharedWorkspace] openURL:url
                          configuration:[NSWorkspaceOpenConfiguration new]
                         completionHandler:^(NSRunningApplication* app, NSError* error) { … }];
```

```cpp
// anycomfy_plugin.cpp
extern "C" int ofx_open_url(const char* url_cstr);   // implemented in open_url.mm
// …
ofx_open_url(url.c_str());
```

### Why not other approaches

| Approach | Problem |
|---|---|
| `system("open ...")` | Subprocess inherits sandbox; silently fails in Resolve |
| `posix_spawn` | Same subprocess sandboxing issue |
| `LSOpenURLsWithApplication` | Both the header and the symbol were removed from CoreServices in macOS 26. Not available at all on this SDK. |
| `AuthorizationExecuteWithPrivileges` | Deprecated, requires user password prompt, overkill |

`NSWorkspace` via `.mm` was the only viable path. The wrapper is a single file
(`open_url.mm`) with one function, so the Obj-C surface is minimal.

---

## Files changed

| File | Change |
|---|---|
| `contrib/plugins/ComfyUI/common/comfyui_base_plugin.cpp` | `#include <dlfcn.h>`; file-scope `_dladdr_anchor`; `dladdr` fallback in `getBundleResourcePath` |
| `contrib/plugins/ComfyUI/anycomfy/open_url.mm` | **New.** `NSWorkspace` wrapper exposing `ofx_open_url()` as `extern "C"` |
| `contrib/plugins/ComfyUI/anycomfy/anycomfy_plugin.cpp` | Calls `ofx_open_url()` instead of `system("open")` |
| `contrib/plugins/ComfyUI/anycomfy/CMakeLists.txt` | `open_url.mm` added to sources; `-framework AppKit` added to macOS link step |

---

## What to test

1. **"New Workflow" in Resolve** — press the button. A new workflow directory should
   appear under `/Volumes/silo2/002_COMFYUI/in/workflows/` and a browser tab should
   open to ComfyUI with the template loaded. Check the log for `dladdr path:` to
   confirm the fallback fired.

2. **"Edit Workflow" in Resolve** — select an existing workflow, press the button.
   Browser should open to ComfyUI with that workflow loaded. Check the log — there
   should be no `LSOpenURLsWithApplication failed` warning.

3. **Both buttons in Flame/Flare** — behaviour should be unchanged. The `dladdr`
   fallback only fires when `kOfxPluginPropFilePath` is empty; Flame populates it
   normally so that path is never taken. `LSOpenURLsWithApplication` works identically
   to `open` in non-sandboxed contexts.

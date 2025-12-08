# OFX Async Rendering Investigation

**Date**: 2025-10-09
**Status**: ⚠️ **OFX Does Not Support True Async Rendering**

---

## Summary

After investigating the OFX specification and APIs, **OFX does not provide a mechanism for plugins to trigger automatic re-renders after async work completes**.

## What I Found

### ✅ Available APIs

1. **Progress Reporting** (Support library):

   ```cpp
   void progressStart(const std::string &message);
   bool progressUpdate(double progress);  // Returns false if user cancels
   void progressEnd();
   ```

   - Shows progress bar to user
   - Allows cancellation
   - **But render() must still block**

2. **Instance Changed Actions**:
   - `kOfxActionBeginInstanceChanged` / `kOfxActionEndInstanceChanged`
   - `kOfxActionInstanceChanged`
   - Only for parameter changes, not for triggering re-renders

3. **Cache Invalidation**:
   - `kOfxParamInvalidateValueChange` / `kOfxParamInvalidateAll`
   - Only works for parameter changes
   - **Cannot be triggered by plugin code after async work**

### ❌ NOT Available

1. **No "request re-render" API** - Plugins cannot tell the host "please re-render frame X"
2. **No async render support** - OFX expects `render()` to complete synchronously
3. **No callback mechanism** - No way to notify host when background work finishes

## Why This Limitation Exists

OFX was designed for **real-time or near-real-time** effects:

- Color correction
- Keying
- Compositing operations
- GPU accelerated effects

**Not designed for**:

- Network I/O
- AI inference on remote servers
- Long-running background tasks (15-30+ seconds)

## Our Options

### Option 1: Synchronous with Progress (Recommended) ✅

**Accept that render() blocks**, but show progress:

```cpp
void BasePlugin::render(const RenderArguments &args) {
    progressStart("Processing with ComfyUI...");

    try {
        // Step 1: Write input
        progressUpdate(0.1);  // 10%
        writeInputImage(src.get(), frame);

        // Step 2: Queue workflow
        progressUpdate(0.2);  // 20%
        std::string promptId = _comfyClient->queuePrompt(workflow, clientId);

        // Step 3: Monitor execution (this blocks, but updates progress)
        _comfyClient->monitorExecution(promptId, [this](EventType type, const json& data) {
            if (type == EventType::Progress && data.contains("value") && data.contains("max")) {
                double progress = 0.2 + 0.6 * (data["value"].get<double>() / data["max"].get<double>());
                progressUpdate(progress);  // 20-80%
            }
        });

        // Step 4: Load result
        progressUpdate(0.9);  // 90%
        loadResult(outputPath, dst);

        progressUpdate(1.0);  // 100%
    } catch (...) {
        progressEnd();
        throw;
    }

    progressEnd();
}
```

**Pros**:

- ✅ Works within OFX limitations
- ✅ User sees progress
- ✅ Simple implementation
- ✅ Reliable

**Cons**:

- ❌ UI blocks (but shows progress, so acceptable)
- ❌ User cannot interact while processing

### Option 2: Cache-Based with Manual Re-render ⚠️

**What I implemented** - requires user to manually re-render:

```cpp
void BasePlugin::render(const RenderArguments &args) {
    // Check cache
    if (cached) {
        loadFromCache();
        return;
    }

    // Launch background task
    std::async([this, args]() {
        executeWorkflow(args);
        // Cache result, but NO WAY to trigger automatic re-render
    });

    // Tell user to try again later
    throw std::runtime_error("Processing... Re-render in 15-30 seconds");
}
```

**Pros**:

- ✅ UI doesn't block

**Cons**:

- ❌ **Terrible UX** - user must guess when to re-render
- ❌ No automatic update
- ❌ User must manually retry multiple times

### Option 3: Pre-render / Cache Workflow 💡

**Process frames ahead of time**, then render is instant:

```cpp
// User workflow:
// 1. Click "Pre-process Frames 1-100" button (custom parameter action)
// 2. Wait (with progress)
// 3. Render timeline - all frames instant (load from cache)

void BasePlugin::preProcessFrames(int startFrame, int endFrame) {
    progressStart("Pre-processing frames for ComfyUI...");

    for (int frame = startFrame; frame <= endFrame; frame++) {
        // Process in background
        executeWorkflowForFrame(frame);

        double progress = (frame - startFrame + 1.0) / (endFrame - startFrame + 1.0);
        if (!progressUpdate(progress)) {
            break;  // User cancelled
        }
    }

    progressEnd();
}

void BasePlugin::render(const RenderArguments &args) {
    // Just load from cache (instant)
    int frame = static_cast<int>(args.time);
    if (hasCachedResult(frame)) {
        loadFromCache(frame);
    } else {
        throw std::runtime_error("Frame not pre-processed. Use 'Pre-process' button first.");
    }
}
```

**Pros**:

- ✅ Playback is real-time (loads from cache)
- ✅ User can work while pre-processing
- ✅ Matches typical VFX workflow (pre-render, then review)

**Cons**:

- ⚠️ Requires custom UI button
- ⚠️ Two-step workflow

## Recommendation

**Implement Option 1: Synchronous with Progress** ✅

### Why?

1. **Works within OFX design** - No fighting the framework
2. **Good UX** - User sees what's happening
3. **Simple** - Less code, fewer edge cases
4. **Reliable** - No race conditions or cache issues
5. **Matches expectations** - Long-running effects block (users understand this)

### Similar to Python PyBox

The Python version also blocks:

```python
# PyBox also blocks during processing
submit_workflow(workflow_data)
update_workflow_execution()  # Blocks until complete
return result
```

Flame continues to work because **PyBox runs in a separate process**. But within the PyBox process, it's synchronous.

Similarly, our OFX plugin blocks its render thread, but:

- Flame/Nuke/Resolve have **thread pools** for rendering
- Other frames can render on other threads
- UI thread stays responsive (as long as we show progress)

## Implementation Plan

1. **Revert async changes** - Remove background threads, futures, cache maps
2. **Add progress reporting** - Use OFX progress APIs
3. **Update WebSocket monitoring** - Call `progressUpdate()` on events
4. **Test with live server** - Verify progress shows correctly

## Code Changes Needed

### Revert render() to synchronous

```cpp
void BasePlugin::render(const OFX::RenderArguments &args) {
    progressStart("ComfyUI Processing");

    try {
        executeWorkflow(args);  // Blocks, but shows progress
    } catch (const std::exception& e) {
        progressEnd();
        throw;
    }

    progressEnd();
}
```

### Add progress to executeWorkflow()

```cpp
void BasePlugin::executeWorkflow(const OFX::RenderArguments &args) {
    // Step 1: Write input (10%)
    progressUpdate(0.1);
    std::string inputPath = writeInputImage(src.get(), frame);

    // Step 2: Queue (20%)
    progressUpdate(0.2);
    std::string promptId = _comfyClient->queuePrompt(workflow, clientId);

    // Step 3: Monitor (20-80%)
    _comfyClient->monitorExecution(promptId, [this](EventType type, const json& data) {
        if (type == EventType::Progress) {
            double serverProgress = data["value"].get<double>() / data["max"].get<double>();
            progressUpdate(0.2 + 0.6 * serverProgress);
        }
    });

    // Step 4: Load (90%)
    progressUpdate(0.9);
    ImageData result = ImageIO::readEXR(outputPath);

    // Step 5: Copy to output (100%)
    ImageIO::toOFXBuffer(result, dst->getPixelData(), ...);
    progressUpdate(1.0);
}
```

## Conclusion

**OFX fundamentally does not support async rendering with automatic re-render**.

The best approach is **synchronous rendering with progress reporting**, which:

- Works within OFX limitations
- Provides good user experience
- Is simple and reliable
- Matches how other VFX software handles long operations

---

**Status**: Ready to implement synchronous + progress approach
**Next**: Revert async changes and add progress reporting

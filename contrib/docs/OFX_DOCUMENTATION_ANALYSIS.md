# OFX Documentation Analysis: Is the Re-render Limitation Clearly Stated?

**Date**: 2025-10-10
**Question**: "Is the conclusion that OFX plugins cannot trigger re-renders clearly stated within the OFX documentation?"

---

## Executive Summary

**Answer**: **No, it is NOT explicitly stated** that plugins cannot trigger re-renders or invalidate frames.

However, the limitation is **strongly implied** by:

1. ✅ No re-render API exists in the specification
2. ✅ GPU async is supported but CPU async is not documented
3. ⚠️ The documentation is "specification by omission" - if an API doesn't exist, you can't use it

---

## What We Found in the OFX Documentation

### 1. Available Plugin-to-Host APIs

From reviewing the official OFX specification:

**ofxImageEffect.h - Complete List of Host Functions:**

```c
// Memory management
typedef OfxStatus (*OfxImageMemorySuiteV1_imageMemoryAlloc)(void *handle, size_t nBytes, void **allocatedData);
typedef OfxStatus (*OfxImageMemorySuiteV1_imageMemoryFree)(void *allocatedData);
typedef OfxStatus (*OfxImageMemorySuiteV1_imageMemoryLock)(void *handle, void **returnedPtr);
typedef OfxStatus (*OfxImageMemorySuiteV1_imageMemoryUnlock)(void *handle);

// Abort checking
typedef int (*OfxImageEffectSuiteV1_abort)(OfxImageEffectHandle imageEffect);

// Progress reporting
typedef OfxStatus (*OfxProgressSuiteV1_progressStart)(OfxImageEffectHandle effect, const char *message);
typedef OfxStatus (*OfxProgressSuiteV1_progressUpdate)(OfxImageEffectHandle effect, double progress);
typedef OfxStatus (*OfxProgressSuiteV1_progressEnd)(OfxImageEffectHandle effect);
```

**That's it.** No other plugin-to-host notification mechanisms exist.

**Notably ABSENT:**

```c
// ❌ These DO NOT EXIST in OFX
typedef OfxStatus (*invalidateFrame)(OfxImageEffectHandle effect, OfxTime time);
typedef OfxStatus (*triggerReRender)(OfxImageEffectHandle effect, OfxTime time);
typedef OfxStatus (*updateCache)(OfxImageEffectHandle effect, OfxTime time, const char *path);
typedef OfxStatus (*setFrameState)(OfxImageEffectHandle effect, OfxTime time, FrameState state);
typedef OfxStatus (*registerCallback)(OfxImageEffectHandle effect, CompletionCallback callback);
```

### 2. Parameter-Based Re-render Triggers (HOST-Initiated)

The documentation DOES describe how **parameter changes** trigger re-renders, but this is **HOST behavior**, not plugin control:

**kOfxParamPropEvaluateOnChange:**

```c
// From Properties Reference
kOfxParamPropEvaluateOnChange
  - Type: int
  - Default: 1
  - Description: "Whether changing a parameter value forces a re-render"
```

**Key Point**: This means "when USER changes parameter → host re-renders". The plugin doesn't trigger this; the host does in response to UI interaction.

**kOfxParamPropCacheInvalidation:**

```c
kOfxParamPropCacheInvalidation
  - Type: string
  - Values:
    * kOfxParamInvalidateValueChange     // Invalidate only changed frames
    * kOfxParamInvalidateValueChangeToEnd // Invalidate from keyframe to end
    * kOfxParamInvalidateAll              // Invalidate entire cache
```

**Key Point**: This tells the HOST how to invalidate ITS cache when USER modifies parameters. Again, plugin doesn't control this.

### 3. GPU Async Rendering (The Exception)

**Found in Rendering Reference:**

**CUDA Async:**

```
The plug-in SHOULD ensure that its render action enqueues any asynchronous
CUDA operations onto the supplied queue.

The plug-in SHOULD NOT wait for final asynchronous operations to complete
before returning from the render action.

The plug-in SHOULD NOT call cudaDeviceSynchronize().
```

**OpenCL Async:**

```
The plug-in SHOULD ensure that its render action enqueues any asynchronous
OpenCL operations onto the supplied queue.

The plug-in SHOULD NOT wait for final asynchronous operations to complete
before returning from the render action.
```

**Metal Async:**

```
The plug-in SHOULD ensure that its render action enqueues any asynchronous
Metal operations onto the supplied queue.
```

**Analysis:**

- ✅ GPU async IS supported
- ✅ Plugin can enqueue operations and return immediately
- ❓ How does host know when complete? **NOT DOCUMENTED**
- ❓ Is there a callback? **NO MENTION**
- ❓ Does this apply to CPU operations? **NO MENTION**

### 4. Abort Mechanism (HOST-to-Plugin)

**Only Bidirectional Communication Found:**

```c
int abort(OfxImageEffectHandle imageEffect);
```

**Direction**: HOST → Plugin (not Plugin → Host)

**Purpose**: Host tells plugin to stop rendering (e.g., user hit ESC)

**Not a solution** for plugin completion notification.

---

## What the Documentation Does NOT Say

### Explicit Statements We Were Looking For

We searched for statements like:

❌ "Plugins cannot trigger re-renders"
❌ "Plugins cannot invalidate host cache"
❌ "Plugins cannot request frame updates"
❌ "Async CPU operations are not supported"
❌ "Only host can initiate rendering"

**Result**: NONE of these statements exist in the documentation.

### What We Did Find

The documentation uses **specification by omission**:

- It defines what plugins CAN do
- Anything not listed, plugins CANNOT do
- No explicit "limitations" section

---

## GPU Async: The Confusing Exception

### What GPU Async Allows

```cpp
// OFX allows this for GPU:
void render(const OFX::RenderArguments &args) {
    // Enqueue GPU operations
    cudaMemcpyAsync(d_src, src, size, stream);
    launchKernel<<<grid, block, 0, stream>>>(d_src, d_dst);
    cudaMemcpyAsync(dst, d_dst, size, stream);

    // Return WITHOUT waiting!
    return;  // GPU work continues in background
}
```

**How does host know when complete?**

- Documentation says: "Host manages synchronization"
- But HOW? Not specified.
- Likely: Host calls `cudaStreamSynchronize()` after plugin returns
- Or: Host uses `cudaStreamQuery()` to poll completion

### Why This Doesn't Help CPU Async

```cpp
// OFX does NOT support this for CPU:
void render(const OFX::RenderArguments &args) {
    // Launch CPU background work
    std::thread worker([=]() {
        callComfyUI(args);
        writeResult(outputPath);
        // How to notify host we're done? NO API!
    });

    // Return immediately
    return;  // ❌ No way to tell host when complete
}
```

**Problem**: GPU async works because:

1. Host provides command queue/stream
2. Host can query stream status
3. Host knows when GPU operations complete

**CPU async doesn't work** because:

1. No host-provided mechanism for CPU work
2. No way for plugin to register callback
3. No way for plugin to notify host of completion

---

## Comparison: What Other APIs Document

### WebGL Specification (Good Example)

```javascript
// WebGL EXPLICITLY states async behavior
gl.finish();  // "Returns when all previously issued commands are complete"
gl.flush();   // "Returns immediately, commands execute asynchronously"

// Explicit documentation of async model
"flush() causes all commands to be executed as quickly as possible,
though they are not guaranteed to be complete before flush() returns."
```

### Vulkan Specification (Good Example)

```c
// Vulkan EXPLICITLY documents sync primitives
vkQueueSubmit();           // "Asynchronously executes commands"
vkWaitForFences();         // "Blocks until fences are signaled"
vkGetFenceStatus();        // "Query fence status without blocking"

// Explicit async model documentation
"Command execution is asynchronous. Applications must use synchronization
primitives to ensure correct ordering."
```

### OFX Specification (Current State)

```c
// OFX has progress APIs but NO completion notification
progressStart();   // "Start progress indication"
progressUpdate();  // "Update progress"
progressEnd();     // "End progress indication"

// ❌ Missing:
waitForCompletion();      // Doesn't exist
registerCallback();       // Doesn't exist
notifyComplete();        // Doesn't exist
```

**Conclusion**: OFX documentation assumes synchronous CPU model by not documenting anything else.

---

## Industry Understanding vs Documentation

### What the Industry Knows (Not Documented)

Based on years of OFX plugin development, the community understands:

1. **Plugins are reactive** - respond to host actions
2. **Host controls pipeline** - initiates all renders
3. **Synchronous model** - plugin render() must complete before returning
4. **No callbacks** - plugins cannot register completion handlers

**But where is this documented?** It's not explicitly stated; it's learned by:

- Reading the API (seeing what doesn't exist)
- Writing plugins (discovering limitations)
- Community knowledge (forum posts, examples)

### Real-World Plugin Behavior

**Boris FX Mocha (Tracking):**

- Blocks during tracking
- Shows progress bar
- Returns when complete
- **Everyone accepts this as normal**

**Why?** Because there's no alternative in OFX.

**RE:Vision Twixtor (Optical Flow):**

- Analysis phase blocks
- Shows progress
- Caches results
- **This is the OFX pattern**

**Why?** Because OFX provides no mechanism for async completion notification.

---

## Our Conclusion in Context

### What We Stated

> "OFX does not provide a mechanism for plugins to trigger automatic re-renders after async work completes"

### Is This Clearly Documented?

**No, but it's true because:**

1. ✅ **API Inspection**: No such functions exist in ofxImageEffect.h
2. ✅ **Properties Reference**: No properties for completion notification
3. ✅ **GPU Async Exception**: Only GPU queues supported, not general async
4. ⚠️ **Specification by Omission**: If it's not documented, it doesn't exist

### Should It Be More Explicit?

**Yes!** The OFX specification would benefit from a section like:

```markdown
## Plugin Execution Model

OpenFX plugins follow a synchronous, host-initiated execution model:

1. Plugins MUST complete render() before returning
2. Plugins CANNOT trigger re-renders or cache invalidation
3. Plugins CANNOT register completion callbacks
4. Only parameter changes (via user interaction) trigger re-renders

Exception: GPU rendering APIs (CUDA, OpenCL, Metal) support asynchronous
operation queueing, but synchronization is managed by the host.
```

**This section does not currently exist.**

---

## Recommendations for OFX Specification

### Add Explicit Documentation

**Proposed Section: "Plugin Execution Model"**

```markdown
## Plugin Execution Model

### Synchronous CPU Rendering

CPU-based image effects MUST complete all processing before returning from
the render action. The render action is the only time a plugin may write
to the output buffer.

Plugins CANNOT:
- Trigger re-renders of cached frames
- Invalidate host cache entries
- Request frame updates
- Register completion callbacks
- Initiate rendering of other frames

The host controls all rendering scheduling. Plugins are reactive components
that respond to host requests.

### Asynchronous GPU Rendering

GPU rendering APIs (CUDA, OpenCL, Metal) support asynchronous operation
queueing. Plugins MAY enqueue GPU operations and return before completion.
The host manages GPU synchronization using the provided command queue/stream.

### Progress Reporting

For long-running operations, plugins SHOULD use the Progress Suite to
provide user feedback:
- progressStart(): Indicate processing has begun
- progressUpdate(): Update progress (0.0 to 1.0)
- progressEnd(): Indicate processing is complete

Progress reporting does NOT affect render scheduling. The render action
MUST still complete before returning.
```

### Add FAQ Section

```markdown
## Frequently Asked Questions

Q: Can my plugin call an external renderer (e.g., Maya, Houdini)?
A: Yes, but the render action must block until the external renderer
   completes. Use progressUpdate() to show status. For lengthy operations
   (>1 minute), consider a pre-computation workflow.

Q: Can my plugin process frames asynchronously?
A: Not for CPU operations. GPU rendering supports async via command queues,
   but CPU operations must complete synchronously.

Q: Can my plugin notify the host when background work completes?
A: No. OFX provides no callback or notification mechanism for this purpose.
```

---

## Comparison with PyBox

### PyBox Documentation (If It Existed)

**Hypothetical PyBox Spec:**

```markdown
## PyBox Execution Model

PyBox handlers integrate with Flame's deferred evaluation system:

1. Handlers CAN return immediately with PROCESSING state
2. Handlers CAN register completion callbacks
3. Flame timeline shows "processing" indicator
4. On completion, handler calls trigger_timeline_refresh()
5. Flame automatically updates affected frames

Example:
    def process_frame(self, frame):
        job_id = submit_to_maya(frame)
        self.register_callback(job_id, self.on_complete)
        return PROCESSING

    def on_complete(self, frame, result):
        self.update_cache(frame, result)
        flame.timeline.invalidate_frame(frame)
```

**This is exactly what OFX lacks** - and it's NOT explicitly stated that OFX lacks it.

---

## Final Answer

### Is the Re-render Limitation Clearly Stated?

**No.** The OFX documentation:

1. ❌ Does NOT explicitly state plugins cannot trigger re-renders
2. ❌ Does NOT have a "limitations" section
3. ❌ Does NOT explain the synchronous execution model
4. ✅ Does show what APIs exist (by omission, others don't)
5. ✅ Does document GPU async (but not its limitations)
6. ⚠️ Uses "specification by omission" approach

### How Do Developers Learn This?

**Current Reality:**

- Read the API header → see what's missing
- Try to implement async → discover it doesn't work
- Ask on forums → community explains
- Study existing plugins → see synchronous pattern
- **Trial and error**

**Better Approach:**

- Explicit "Execution Model" section
- Clear statement of synchronous requirement
- FAQ addressing common questions
- Comparison with other plugin systems (if appropriate)

### Our Implementation Was Correct

Our decision to use **synchronous + progress** was correct because:

1. ✅ It's the only viable OFX pattern
2. ✅ It's what all heavy-processing OFX plugins do
3. ✅ The documentation (by omission) supports this
4. ✅ No alternative exists in the specification

**But** we had to figure this out through investigation rather than reading an explicit statement in the docs.

---

## References

**OFX Official Documentation:**

- [Rendering Reference](https://openfx.readthedocs.io/en/main/Reference/ofxRendering.html)
- [Properties Reference](https://openfx.readthedocs.io/en/main/Reference/ofxPropertiesReference.html)
- [ofxImageEffect.h](https://github.com/AcademySoftwareFoundation/openfx/blob/main/include/ofxImageEffect.h)

**Our Analysis:**

- [OFX_ASYNC_INVESTIGATION.md](OFX_ASYNC_INVESTIGATION.md)
- [PYBOX_VS_OFX_RENDERING_MODELS.md](PYBOX_VS_OFX_RENDERING_MODELS.md)
- [SESSION_10_SYNCHRONOUS_IMPLEMENTATION.md](SESSION_10_SYNCHRONOUS_IMPLEMENTATION.md)

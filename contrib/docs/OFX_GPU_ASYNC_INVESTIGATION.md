# OFX GPU Async Investigation: Evidence from Support Examples

**Date**: 2025-10-10
**Question**: "Can we do async with OFX?" - Investigation of GPUGain example plugin

---

## Executive Summary

**Finding**: OFX **DOES** support async for **GPU operations only**, but in a very specific way:

1. ✅ Plugin enqueues GPU operations to host-provided stream/queue
2. ✅ Plugin returns immediately **without** waiting for GPU completion
3. ✅ **Host** manages synchronization (not plugin)
4. ❌ Plugin has **NO notification** when GPU work completes
5. ❌ Plugin **CANNOT** trigger re-render after completion
6. ❌ **Does NOT apply** to CPU/network operations

**Conclusion**: GPU async is not a solution for ComfyUI network calls.

---

## Evidence from GPUGain Example

### 1. Plugin Code Analysis

**File**: `/Users/julien/src/openfx/Support/Plugins/GPUGain/GPUGain.cpp`

**The render() Method**:
```cpp
void GPUGain::render(const OFX::RenderArguments& p_Args)
{
    // Line 219-227
    GainExample imageScaler(*this);
    setupAndProcess(imageScaler, p_Args);  // Synchronous call
}
```

**The setupAndProcess() Method**:
```cpp
void GPUGain::setupAndProcess(GainExample& p_GainExample,
                               const OFX::RenderArguments& p_Args)
{
    // Lines 286-332

    // Get source and destination images
    std::unique_ptr<OFX::Image> dst(m_DstClip->fetchImage(p_Args.time));
    std::unique_ptr<OFX::Image> src(m_SrcClip->fetchImage(p_Args.time));

    // Setup GPU render arguments - THIS IS THE KEY
    p_GainExample.setGPURenderArgs(p_Args);  // Line 322

    // Call process - this is where GPU magic happens
    p_GainExample.process();  // Line 331
}
```

### 2. GPU Stream/Queue Handling

**File**: `/Users/julien/src/openfx/Support/include/ofxsProcessing.h`

**ImageProcessor stores host-provided GPU handles**:
```cpp
class ImageProcessor : public OFX::MultiThread::Processor
{
protected:
    void*            _pOpenCLCmdQ;           // OpenCL Command Queue Handle
    void*            _pCudaStream;           // CUDA Stream Handle
    void*            _pMetalCmdQ;            // Metal Command Queue Handle

    void setGPURenderArgs(const OFX::RenderArguments& args)
    {
        if (args.isEnabledOpenCLRender)
        {
            _pOpenCLCmdQ = args.pOpenCLCmdQ;  // HOST provides queue
        }
        if (args.isEnabledCudaRender)
        {
            _pCudaStream = args.pCudaStream;  // HOST provides stream
        }
        if (args.isEnabledMetalRender)
        {
            _pMetalCmdQ = args.pMetalCmdQ;    // HOST provides queue
        }
    }
```

**Key Point**: Host provides the GPU queue/stream, not the plugin!

### 3. The process() Method

**File**: `/Users/julien/src/openfx/Support/include/ofxsProcessing.h` (lines 172-222)

```cpp
virtual void process(void)
{
    preProcess();

    if (_isEnabledOpenCLRender)
    {
        processImagesOpenCL();     // Plugin implements this
    }
    else if (_isEnabledCudaRender)
    {
        processImagesCuda();       // Plugin implements this
    }
    else if (_isEnabledMetalRender)
    {
        processImagesMetal();      // Plugin implements this
    }
    else  // CPU
    {
        multiThread(nCPUs);        // Multi-threaded CPU processing
    }

    postProcess();

    // ⚠️ NO GPU SYNCHRONIZATION HERE!
}
```

**Critical Observation**: `process()` returns immediately after calling `processImagesCuda()` - **no synchronization**!

### 4. CUDA Kernel Execution

**File**: `/Users/julien/src/openfx/Support/Plugins/GPUGain/CudaKernel.cu`

```cpp
void RunCudaKernel(void* p_Stream, int p_Width, int p_Height,
                   float* p_Gain, const float* p_Input, float* p_Output)
{
    dim3 threads(128, 1, 1);
    dim3 blocks(((p_Width + threads.x - 1) / threads.x), p_Height, 1);

    // Cast void* to CUDA stream
    cudaStream_t stream = static_cast<cudaStream_t>(p_Stream);

    // Launch kernel on HOST-PROVIDED stream
    GainAdjustKernel<<<blocks, threads, 0, stream>>>(
        p_Width, p_Height,
        p_Gain[0], p_Gain[1], p_Gain[2], p_Gain[3],
        p_Input, p_Output
    );

    // ⚠️ NO cudaStreamSynchronize()!
    // ⚠️ NO cudaDeviceSynchronize()!
    // ⚠️ Function returns immediately!
}
```

**Called from**:
```cpp
void GainExample::processImagesCuda()
{
    // Lines 53-65
    float* input = static_cast<float*>(_srcImg->getPixelData());
    float* output = static_cast<float*>(_dstImg->getPixelData());

    RunCudaKernel(_pCudaStream, width, height, _scales, input, output);

    // ⚠️ Returns immediately after enqueueing!
    // ⚠️ GPU work continues in background!
}
```

### 5. No Synchronization in Plugin Code

**Search Results**:
```bash
$ grep -r "cudaStreamSynchronize\|cudaDeviceSynchronize\|clFinish" GPUGain/
# NO RESULTS
```

**Conclusion**: Plugin **never** waits for GPU operations to complete!

---

## How GPU Async Actually Works

### The Execution Flow:

```
1. Host calls plugin->render()
   ↓
2. Plugin calls setupAndProcess()
   ↓
3. Plugin fetches src/dst images (GPU memory)
   ↓
4. Plugin calls process()
   ↓
5. process() calls processImagesCuda()
   ↓
6. processImagesCuda() calls RunCudaKernel()
   ↓
7. RunCudaKernel() ENQUEUES kernel to host stream
   ↓
8. RunCudaKernel() RETURNS IMMEDIATELY ⚡
   ↓
9. processImagesCuda() RETURNS IMMEDIATELY ⚡
   ↓
10. process() RETURNS IMMEDIATELY ⚡
    ↓
11. render() RETURNS IMMEDIATELY ⚡
    ↓
12. 🎯 HOST now has control again
    ↓
13. HOST calls cudaStreamSynchronize(_pCudaStream) ✅
    ↓
14. HOST waits for GPU completion
    ↓
15. HOST reads output buffer when ready
```

### Key Insight:

**Plugin enqueues work and returns → Host synchronizes**

This is fundamentally different from:
```cpp
// What we CANNOT do:
void render() {
    submitToComfyUI();        // Network call
    return IMMEDIATELY;       // ❌ Who synchronizes?
    // Host has no "ComfyUI stream" to wait on!
}
```

---

## Why This Doesn't Help ComfyUI

### GPU Async Model:

| Aspect | GPU Operations | ComfyUI Network Calls |
|--------|---------------|----------------------|
| **Queue** | Host-provided CUDA stream | ❌ No equivalent |
| **Enqueue** | `kernel<<<..., stream>>>()` | ❌ HTTP POST (blocking) |
| **Async** | Kernel runs in background | ❌ Network I/O blocks |
| **Sync** | Host calls `cudaStreamSync()` | ❌ No host API |
| **Query** | Host can query stream status | ❌ No query mechanism |
| **Completion** | Host knows when done | ❌ Plugin knows, host doesn't |

### The Fundamental Difference:

**GPU Async:**
```cpp
// Plugin
void processImagesCuda() {
    kernel<<<..., hostStream>>>(...);  // Enqueue to HOST stream
    return;                            // Host will sync
}

// Host (after plugin returns)
cudaStreamSynchronize(hostStream);     // Host waits
// Now output buffer is ready
```

**ComfyUI Async (DOESN'T WORK):**
```cpp
// Plugin
void render() {
    std::thread([=]() {
        callComfyUI(...);              // Network call
        writeOutput(...);              // Write to storage
        // ❌ How to notify host?
    }).detach();
    return;                            // ❌ Host has no way to sync
}

// Host (after plugin returns)
// ??? What to wait on?
// ??? How to know when ready?
// ❌ No equivalent to cudaStreamSynchronize()
```

---

## Critical Limitations

### What GPU Async Provides:

1. ✅ Host provides GPU queue/stream
2. ✅ Plugin enqueues operations
3. ✅ Plugin returns immediately
4. ✅ **Host** manages synchronization
5. ✅ Host can query completion status

### What GPU Async Does NOT Provide:

1. ❌ Plugin notification when complete
2. ❌ Plugin callback mechanism
3. ❌ Plugin ability to trigger re-render
4. ❌ Plugin control over synchronization
5. ❌ Applies to CPU/network operations

### Why This Matters:

**GPU async works because**:
- Host provides the queue
- Host knows how to sync the queue
- Host can query queue status
- Output buffer is in GPU memory host manages

**CPU/Network async doesn't work because**:
- No host-provided "network queue"
- Host doesn't know about ComfyUI server
- Host can't sync network operations
- Output is in filesystem host doesn't monitor

---

## Documentation Confirmation

### OFX Rendering Reference States:

> **CUDA:** "The plug-in SHOULD ensure that its render action enqueues any asynchronous CUDA operations onto the supplied queue. The plug-in SHOULD NOT wait for final asynchronous operations to complete before returning from the render action."

> **OpenCL:** "The plug-in SHOULD ensure that its render action enqueues any asynchronous OpenCL operations onto the supplied queue. The plug-in SHOULD NOT wait for final asynchronous operations to complete before returning from the render action."

**Key Words**: "onto the **supplied queue**" - Host provides the queue!

### What the Documentation Doesn't Say:

❌ "Plugins can create their own async operations"
❌ "Plugins can register completion callbacks"
❌ "Plugins can notify host of async completion"
❌ "Host will wait for plugin-initiated async work"

---

## Comparison: What We Need vs What Exists

### What ComfyUI Needs:

```cpp
void render() {
    // Launch network operation
    auto future = std::async([=]() {
        callComfyUI(...);
        writeOutput(...);
    });

    // Option A: Wait here (blocks UI) ⚠️
    future.wait();
    loadOutput();

    // Option B: Return immediately ❌
    // But how does host know when done?
    // How does plugin trigger re-render?
    return;
}
```

### What GPU Async Provides:

```cpp
void render() {
    // Enqueue to HOST-PROVIDED queue
    kernel<<<..., hostProvidedStream>>>(...);

    // Return immediately ✅
    // Host will call cudaStreamSynchronize()
    // Host knows when GPU work is done
    return;
}
```

**The gap**: ComfyUI needs plugin-initiated async, but OFX only provides host-managed async.

---

## Real-World Example: How Hosts Use GPU Async

### Nuke's Behavior (Hypothetical):

```cpp
// Nuke (host) code
void renderFrame(int frame) {
    // Create CUDA stream
    cudaStream_t stream;
    cudaStreamCreate(&stream);

    // Call plugin
    RenderArguments args;
    args.pCudaStream = stream;
    args.isEnabledCudaRender = true;

    plugin->render(args);  // Plugin enqueues work and returns

    // ✅ Nuke synchronizes the stream it provided
    cudaStreamSynchronize(stream);

    // Now output buffer is ready
    displayFrame(outputBuffer);

    cudaStreamDestroy(stream);
}
```

**Key**: Host controls the entire lifecycle!

### What We'd Need for ComfyUI (DOESN'T EXIST):

```cpp
// Hypothetical host code
void renderFrame(int frame) {
    // Create "async work tracker"
    AsyncTracker tracker;

    RenderArguments args;
    args.pAsyncTracker = &tracker;  // ❌ Doesn't exist

    plugin->render(args);  // Plugin launches network work

    // ✅ Host waits on tracker
    tracker.wait();  // ❌ No such API

    displayFrame(outputBuffer);
}
```

**The missing piece**: No generic "async work tracker" in OFX.

---

## Conclusion

### Can We Do Async with OFX?

**For GPU operations**: ✅ **Yes**
- Plugin enqueues to host-provided stream
- Plugin returns immediately
- Host synchronizes

**For CPU/Network operations**: ❌ **No**
- No host-provided mechanism
- No way to notify host of completion
- No way to trigger re-render

### Why GPUGain Doesn't Help ComfyUI:

1. **GPU async requires host-provided queue** - ComfyUI server is external
2. **Host manages GPU synchronization** - Host doesn't know about ComfyUI
3. **Works because output is in GPU memory** - ComfyUI output is in filesystem
4. **Host controls the stream** - Plugin can't control network timing

### Our Implementation is Correct:

**Synchronous + Progress** is the right choice because:
1. ✅ It's the only viable CPU-based OFX pattern
2. ✅ Real-time progress via WebSocket is event-driven
3. ✅ Standard pattern for heavy CPU/network operations
4. ✅ Works across all OFX hosts
5. ❌ GPU async pattern **does not apply** to network operations

---

## References

**OFX Examples Analyzed:**
- `/Users/julien/src/openfx/Support/Plugins/GPUGain/GPUGain.cpp`
- `/Users/julien/src/openfx/Support/Plugins/GPUGain/CudaKernel.cu`
- `/Users/julien/src/openfx/Support/include/ofxsProcessing.h`

**OFX Documentation:**
- [Rendering Reference](https://openfx.readthedocs.io/en/main/Reference/ofxRendering.html)

**Our Analysis:**
- [OFX_ASYNC_INVESTIGATION.md](OFX_ASYNC_INVESTIGATION.md)
- [OFX_DOCUMENTATION_ANALYSIS.md](OFX_DOCUMENTATION_ANALYSIS.md)
- [PYBOX_VS_OFX_RENDERING_MODELS.md](PYBOX_VS_OFX_RENDERING_MODELS.md)

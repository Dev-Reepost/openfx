# Non-Blocking ComfyUI Plugin Implementation Plan

## Executive Summary

Transform ComfyUI OFX plugins from blocking (UI-freezing) to non-blocking (progressive rendering) architecture while maintaining robustness and simplicity.

**Goal:** Users can scrub timeline freely while ComfyUI renders in background, with results appearing progressively as they complete.

---

## Architecture Overview

### Current (Blocking) Flow

```
render() called → Submit workflow → WAIT for ComfyUI → Return result
                                    ↑
                                    └─ BLOCKS UI (5-60+ seconds)
```

### New (Non-Blocking) Flow

```
render() called → Check cache
                  ├─ Hit? → Return immediately ✓
                  └─ Miss? → Check if job queued
                             ├─ Yes? → Return placeholder ✓
                             └─ No? → Queue job async + Return placeholder ✓

Background Thread: Monitor jobs → On completion → Invalidate cache → Host re-renders
```

---

## Core Components

### 1. AsyncJobManager (New)

**File:** `contrib/plugins/ComfyUI/common/async_job_manager.h/cpp`

**Responsibilities:**

- Maintain job queue (frame → job mapping)
- Submit jobs to ComfyUI asynchronously
- Monitor job status via background thread
- Notify plugin when jobs complete
- Handle failures and cleanup

**Key Classes:**

```cpp
enum class JobStatus {
    QUEUED,      // Submitted to ComfyUI
    PROCESSING,  // ComfyUI is working on it
    COMPLETED,   // Success - file written to disk
    FAILED       // Error occurred
};

struct AsyncJob {
    std::string promptId;        // ComfyUI prompt ID
    int frame;                   // Frame number
    std::string outputPath;      // Expected output file path
    std::chrono::time_point submittedTime;
    JobStatus status;
    std::string errorMessage;    // If failed
};

class AsyncJobManager {
public:
    AsyncJobManager(Client* comfyClient, spdlog::logger* logger);
    ~AsyncJobManager();

    // Job lifecycle
    bool isJobPending(int frame);
    void submitJob(int frame, const json& workflow, const std::string& outputPath);
    JobStatus getJobStatus(int frame);
    void cancelJob(int frame);
    void cancelAllJobs();

    // Completion callbacks
    using CompletionCallback = std::function<void(int frame, bool success)>;
    void setCompletionCallback(CompletionCallback callback);

    // Monitoring
    int getPendingJobCount();
    std::vector<int> getPendingFrames();

private:
    void monitorThreadFunc();    // Background monitoring loop
    bool checkJobCompletion(AsyncJob& job);
    void cleanupOldJobs();

    std::map<int, AsyncJob> _jobs;
    std::mutex _jobsMutex;
    std::thread _monitorThread;
    std::atomic<bool> _shutdown;

    Client* _comfyClient;
    spdlog::logger* _logger;
    CompletionCallback _completionCallback;
};
```

### 2. Modified BasePlugin

**Files:** `contrib/plugins/ComfyUI/common/comfyui_base_plugin.h/cpp`

**Changes:**

#### Header Changes

```cpp
class BasePlugin : public OFX::ImageEffect {
protected:
    // NEW: Async job management
    std::unique_ptr<AsyncJobManager> _jobManager;

    // NEW: Async mode parameter
    OFX::ChoiceParam* _asyncMode;

    // NEW: Placeholder mode parameter
    OFX::ChoiceParam* _placeholderMode;

    // NEW: Hidden refresh trigger for cache invalidation
    OFX::DoubleParam* _refreshTrigger;

    // Modified render logic
    virtual void render(const OFX::RenderArguments &args) override;

    // NEW: Async helper methods
    void renderBlocking(const OFX::RenderArguments &args);
    void renderAsync(const OFX::RenderArguments &args);
    void returnPlaceholder(const OFX::RenderArguments &args);
    void onJobComplete(int frame, bool success);

    // Existing methods...
};
```

#### render() Implementation

```cpp
void BasePlugin::render(const OFX::RenderArguments &args) {
    int frame = static_cast<int>(args.time);

    // Check async mode
    int asyncModeValue;
    _asyncMode->getValue(asyncModeValue);

    if (asyncModeValue == 0) {  // Blocking mode
        renderBlocking(args);
        return;
    }

    // === NON-BLOCKING MODE ===

    // STEP 1: Check cache (instant return)
    std::string cachedPath = constructExpectedOutputPath(frame);
    std::ifstream cacheTest(cachedPath);
    if (cacheTest.good()) {
        cacheTest.close();
        if (_logger) _logger->info("Frame {}: Cache HIT", frame);

        // Load from cache and return
        loadCachedResult(args, cachedPath);
        return;  // ✓ INSTANT
    }

    // STEP 2: Check if job already pending
    if (_jobManager->isJobPending(frame)) {
        JobStatus status = _jobManager->getJobStatus(frame);
        if (_logger) _logger->info("Frame {}: Job already pending ({})",
                                   frame, statusToString(status));

        // Return placeholder
        returnPlaceholder(args);
        return;  // ✓ NON-BLOCKING
    }

    // STEP 3: Submit new async job
    if (_logger) _logger->info("Frame {}: Cache MISS - submitting async job", frame);

    try {
        // Write input image to shared storage
        std::unique_ptr<OFX::Image> src(_srcClip->fetchImage(args.time));
        std::string inputPath = writeInputImage(src.get(), frame);

        // Build workflow
        json workflow = buildWorkflow(frame, inputPath);

        // Submit asynchronously
        _jobManager->submitJob(frame, workflow, cachedPath);

        if (_logger) _logger->info("Frame {}: Async job submitted successfully", frame);
    } catch (const std::exception& e) {
        if (_logger) _logger->error("Frame {}: Failed to submit job: {}",
                                    frame, e.what());
    }

    // Return placeholder immediately
    returnPlaceholder(args);  // ✓ NON-BLOCKING
}
```

### 3. Placeholder Rendering

**Modes:**

1. **Source Passthrough** - Show original input (default)
2. **Checkerboard** - Visual indicator that processing is pending
3. **Solid Color** - Configurable color (e.g., gray)
4. **Last Valid Frame** - Show most recent completed result

```cpp
void BasePlugin::returnPlaceholder(const OFX::RenderArguments &args) {
    int placeholderMode;
    _placeholderMode->getValue(placeholderMode);

    std::unique_ptr<OFX::Image> dst(_dstClip->fetchImage(args.time));

    switch (placeholderMode) {
        case 0:  // Source passthrough
            if (_srcClip && _srcClip->isConnected()) {
                std::unique_ptr<OFX::Image> src(_srcClip->fetchImage(args.time));
                if (src.get() && dst.get()) {
                    copyPixelData(src.get(), dst.get());
                }
            }
            break;

        case 1:  // Checkerboard pattern
            renderCheckerboard(dst.get());
            break;

        case 2:  // Solid color
            renderSolidColor(dst.get(), 0.5, 0.5, 0.5);
            break;

        case 3:  // Last valid frame
            int lastValidFrame = findLastValidFrame(args.time);
            if (lastValidFrame >= 0) {
                loadCachedResult(args, constructExpectedOutputPath(lastValidFrame));
            } else {
                // Fallback to source
                std::unique_ptr<OFX::Image> src(_srcClip->fetchImage(args.time));
                if (src.get()) copyPixelData(src.get(), dst.get());
            }
            break;
    }
}
```

### 4. Cache Invalidation Strategy

**Problem:** After job completes, how do we tell the host to re-render the frame?

**Solutions (in order of preference):**

#### Option A: Hidden Parameter Trigger (Most Compatible)

```cpp
void BasePlugin::onJobComplete(int frame, bool success) {
    if (!success) return;

    if (_logger) _logger->info("Frame {}: Job completed - invalidating cache", frame);

    // Modify hidden parameter to trigger re-render
    double currentValue;
    _refreshTrigger->getValueAtTime(frame, currentValue);
    _refreshTrigger->setValueAtTime(frame, currentValue + 0.001);

    // This causes host to invalidate its cache and call render() again
}
```

#### Option B: Clip Invalidation (If available)

```cpp
// Check if host supports clip invalidation
if (hasClipInvalidationAPI()) {
    _dstClip->invalidateCache(frame);
}
```

#### Option C: Periodic UI Refresh (Fallback)

```cpp
// User can manually refresh by scrubbing away and back
// Or we add a "Refresh" button parameter
```

---

## Parameter Additions

### 1. Async Mode

```cpp
OFX::ChoiceParamDescriptor *asyncMode = desc.defineChoiceParam("asyncMode");
asyncMode->setLabel("Rendering Mode");
asyncMode->setHint("Blocking: Traditional behavior (UI freezes during render). "
                   "Non-Blocking: Progressive rendering (returns placeholder immediately)");
asyncMode->appendOption("Blocking (Wait for Result)");
asyncMode->appendOption("Non-Blocking (Progressive)");
asyncMode->setDefault(1);  // Non-blocking by default
asyncMode->setAnimates(false);
```

### 2. Placeholder Mode

```cpp
OFX::ChoiceParamDescriptor *placeholder = desc.defineChoiceParam("placeholderMode");
placeholder->setLabel("Placeholder Display");
placeholder->setHint("What to show while ComfyUI is processing");
placeholder->appendOption("Source Passthrough");
placeholder->appendOption("Checkerboard Pattern");
placeholder->appendOption("Gray Frame");
placeholder->appendOption("Last Valid Result");
placeholder->setDefault(0);
placeholder->setAnimates(false);
```

### 3. Refresh Trigger (Hidden)

```cpp
OFX::DoubleParamDescriptor *refresh = desc.defineDoubleParam("refreshTrigger");
refresh->setLabel("Refresh");
refresh->setIsSecret(true);  // Hidden from UI
refresh->setAnimates(true);  // Keyframable per-frame
refresh->setDefault(0.0);
```

### 4. Job Status Display (Read-Only)

```cpp
OFX::StringParamDescriptor *jobStatus = desc.defineStringParam("jobStatus");
jobStatus->setLabel("Job Status");
jobStatus->setHint("Current async job queue status");
jobStatus->setStringType(OFX::eStringTypeLabel);  // Read-only label
jobStatus->setDefault("No jobs pending");
jobStatus->setEnabled(false);
```

---

## Implementation Phases

### Phase 1: Core Infrastructure ✅

**Deliverables:**

- [ ] AsyncJobManager.h with complete class definition
- [ ] AsyncJobManager.cpp with job queue and monitoring
- [ ] Unit tests for AsyncJobManager

**Acceptance Criteria:**

- Can submit jobs asynchronously
- Background thread monitors completion
- Thread-safe job status queries
- Proper cleanup on destruction

---

### Phase 2: BasePlugin Integration ✅

**Deliverables:**

- [ ] Modified comfyui_base_plugin.h with AsyncJobManager
- [ ] New render() with 3-step logic
- [ ] Placeholder rendering functions
- [ ] Cache invalidation callback

**Acceptance Criteria:**

- Blocking mode still works (backward compatible)
- Non-blocking mode returns immediately
- Jobs tracked across render() calls
- No memory leaks or race conditions

---

### Phase 3: Cache Invalidation ✅

**Deliverables:**

- [ ] Host notification mechanism
- [ ] Test with Flame's cache behavior
- [ ] Fallback strategies if primary method fails

**Acceptance Criteria:**

- Completed frames automatically appear
- No manual refresh needed (ideally)
- Works reliably in Flame

---

### Phase 4: User Experience Polish ✅

**Deliverables:**

- [ ] Visual feedback parameters
- [ ] Job status display in UI
- [ ] Progress indicators (if possible)
- [ ] Manual refresh button (fallback)

**Acceptance Criteria:**

- Users understand what's happening
- Clear indication of pending jobs
- Easy way to see progress

---

### Phase 5: Testing & Documentation ✅

**Deliverables:**

- [ ] Integration tests with real ComfyUI server
- [ ] Performance benchmarks
- [ ] User documentation
- [ ] Update CLAUDE.md with async architecture

**Acceptance Criteria:**

- No crashes under heavy load
- Proper cleanup on plugin unload
- Clear documentation for users

---

## Technical Considerations

### Thread Safety

- All job map access protected by mutex
- ComfyUI client already has mutex protection
- Callback from monitor thread must be thread-safe

### Memory Management

- Jobs cleaned up after completion (configurable retention period)
- Old completed jobs removed to prevent unbounded growth
- Proper shutdown of monitor thread in destructor

### Error Handling

- Network failures: Mark job as failed, allow retry
- ComfyUI errors: Capture error message, show in UI
- Missing output files: Detect and mark as failed

### Performance

- Monitor thread polls every 1 second (configurable)
- Efficient file existence checks for cache
- Minimal overhead for cache hits

### Compatibility

- Blocking mode available for critical renders
- Graceful degradation if async not desired
- Works with existing workflow code (no changes needed)

---

## Success Metrics

### Before (Blocking)

- **Timeline scrubbing:** Freezes for 5-60 seconds per frame
- **User experience:** Frustrating, can't preview without waiting
- **Workflow:** Must wait for each frame to render

### After (Non-Blocking)

- **Timeline scrubbing:** Smooth, instant response
- **User experience:** Can freely explore timeline while rendering progresses
- **Workflow:** Submit batch of frames, come back later to see results

---

## Risk Mitigation

### Risk 1: Host cache invalidation doesn't work

**Mitigation:**

- Implement multiple strategies (parameter trigger, clip invalidation)
- Add manual refresh button as fallback
- Test thoroughly with Flame before release

### Risk 2: Race conditions in job queue

**Mitigation:**

- Comprehensive mutex protection
- Unit tests for concurrent access
- Code review focused on thread safety

### Risk 3: Memory leaks from accumulated jobs

**Mitigation:**

- Automatic cleanup of old jobs
- Configurable retention policy
- Memory usage monitoring in tests

### Risk 4: ComfyUI server overload

**Mitigation:**

- Configurable max concurrent jobs
- Queue depth limits
- Throttling during fast scrubbing

---

## Files to Create

### New Files

```
contrib/plugins/ComfyUI/common/async_job_manager.h
contrib/plugins/ComfyUI/common/async_job_manager.cpp
contrib/plugins/ComfyUI/tests/test_async_job_manager.cpp
```

### Modified Files

```
contrib/plugins/ComfyUI/common/comfyui_base_plugin.h
contrib/plugins/ComfyUI/common/comfyui_base_plugin.cpp
contrib/plugins/ComfyUI/CMakeLists.txt
CLAUDE.md (documentation update)
```

---

## Testing Strategy

### Unit Tests

- AsyncJobManager job queue operations
- Thread safety under concurrent access
- Cleanup and shutdown behavior

### Integration Tests

- Submit job → Monitor → Complete → Invalidate cycle
- Multiple frames in sequence
- Error handling paths

### Manual Testing

- Timeline scrubbing in Flame
- Fast scrubbing (stress test)
- Job completion notification
- Switching between blocking/non-blocking modes

---

## Future Enhancements (Out of Scope)

### Smart Pre-fetching

- When frame N renders, queue N+1 automatically
- Configurable look-ahead distance

### Batch Optimization

- Detect sequence renders (BeginSequenceRender)
- Submit all frames at once to ComfyUI queue

### WebSocket Progress Updates

- Real-time progress percentage
- Show in plugin UI parameter

### Priority Queue

- Current frame = high priority
- Background frames = low priority

---

## Timeline Estimate

- **Phase 1:** 4-6 hours (AsyncJobManager)
- **Phase 2:** 4-6 hours (BasePlugin integration)
- **Phase 3:** 2-4 hours (Cache invalidation testing)
- **Phase 4:** 2-3 hours (UX polish)
- **Phase 5:** 2-4 hours (Testing & docs)

**Total:** 14-23 hours of development

---

## Approval & Sign-off

**Approved by:** [User]
**Date:** 2025-12-08
**Version:** 1.0

Ready to proceed with implementation? ✓

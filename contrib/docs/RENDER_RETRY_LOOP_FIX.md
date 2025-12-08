# Render Retry Loop Fix - Project Name Validation

## Date: 2025-11-21

## Problem

When the plugin was enabled without setting the "Project Name" parameter, Flame would render the same frame **10 times** in rapid succession, all failing with the same error.

### Log Evidence

```
[2025-11-21 13:36:00.596] [info] [comfyui_base_plugin.cpp:493] ComfyUI processing ENABLED
[2025-11-21 13:36:00.596] [warning] [comfyui_base_plugin.cpp:501] Project Name is required but not set...
[2025-11-21 13:36:02.246] [info] [comfyui_base_plugin.cpp:493] ComfyUI processing ENABLED
[2025-11-21 13:36:02.246] [warning] [comfyui_base_plugin.cpp:501] Project Name is required but not set...
... (8 more identical retries)
```

All 10 render attempts were for:
- Same frame: Frame 24
- Same timestamp: Time 1.00083
- Same failure: "Project Name is required but not set"

## Root Cause

The validation code was **throwing a `std::runtime_error` exception** when the project name was empty:

```cpp
if (projectName.empty()) {
    std::string warningMsg = "Project Name is required but not set...";
    if (_logger) _logger->warn(warningMsg);
    setPersistentMessage(OFX::Message::eMessageWarning, "", warningMsg.c_str());
    throw std::runtime_error(warningMsg);  // ← CAUSES RETRY LOOP
}
```

### Why This Caused Retries

When an OFX plugin throws an exception during `render()`:
1. **Host application interprets it as a temporary render failure** (like a GPU hiccup or memory allocation issue)
2. **Host automatically retries** the render, hoping it will succeed on the next attempt
3. **Retry loop continues** until host's maximum retry count is reached (Flame: 10 attempts)

This is correct behavior for **transient failures** (e.g., temporary network issues, GPU busy), but wrong for **configuration errors** (missing required parameter).

## Solution

**Return early with passthrough** instead of throwing an exception when project name is missing.

### Fixed Code

```cpp
if (projectName.empty()) {
    std::string warningMsg = "Project Name is required but not set. Please set the Project Name parameter in the plugin settings.";
    if (_logger) _logger->warn(warningMsg);
    setPersistentMessage(OFX::Message::eMessageWarning, "", warningMsg.c_str());

    // Return passthrough instead of throwing - this is a configuration issue, not a render failure
    // Throwing causes Flame to retry the render multiple times thinking it's a temporary error
    if (_srcClip && _srcClip->isConnected()) {
        std::unique_ptr<OFX::Image> src(_srcClip->fetchImage(args.time));
        std::unique_ptr<OFX::Image> dst(_dstClip->fetchImage(args.time));

        if (src.get() && dst.get()) {
            copyPixelData(src.get(), dst.get());
        }
    }
    return;  // Don't throw exception
}
```

## Expected Behavior After Fix

When plugin is enabled without setting project name:

1. **Single render attempt** (not 10)
2. **Warning message displayed** in Flame UI
3. **Passthrough mode activated** (input copied directly to output)
4. **User can set project name** and processing will resume on next render
5. **No retry loop**

## Code Location

**File**: [comfyui_base_plugin.cpp](common/comfyui_base_plugin.cpp)
**Lines**: 499-515
**Function**: `BasePlugin::render()`

## Testing

### Before Fix
```bash
# Enable plugin without setting project name
# Result: 10 render attempts, log fills with identical warnings
```

### After Fix
```bash
# Enable plugin without setting project name
# Result: 1 render attempt, single warning, passthrough output
```

## Related Issues

This same pattern should be applied to **all configuration validation** in the plugin:

- ✅ **Project Name** - Fixed (uses passthrough)
- ⚠️ **Server Address** - Still throws exception (line ~550)
- ⚠️ **Mount Path** - Still throws exception (line ~560)
- ⚠️ **Workflow Name** - Still throws exception (line ~570)

## Best Practice: Configuration Errors vs Render Failures

### Throw Exceptions For:
- **Transient failures**: Network timeout, GPU busy, temporary memory allocation failure
- **Recoverable errors**: File locked, disk full (might succeed on retry)
- **Critical failures**: Plugin corruption, invalid memory access

### Return Passthrough For:
- **Configuration errors**: Missing required parameters, invalid settings
- **User errors**: Empty text fields, out-of-range values
- **Permanent failures**: Model file not found, incompatible workflow

## Related Documentation

- [COMPREHENSIVE_LOGGING_ADDED.md](COMPREHENSIVE_LOGGING_ADDED.md) - OFX property logging
- [INSTANCE_ID_INVESTIGATION.md](INSTANCE_ID_INVESTIGATION.md) - Project name parameter analysis
- [NON_BLOCKING_UI_FIX.md](NON_BLOCKING_UI_FIX.md) - Async execution patterns

## Build Status

✅ **Fixed and compiled successfully**
✅ **Plugin rebuilt and installed**
✅ **Ready for testing in Flame**

## Future Work

Consider applying the same fix to other configuration validations to prevent retry loops for other missing parameters.

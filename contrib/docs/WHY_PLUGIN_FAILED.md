# Why "Plugin rendering failed" Occurred

## What Happened

You applied the SAMSegmentation plugin in Flame and tried to render, but got repeated "Plugin rendering failed" messages. **Nothing was sent to ComfyUI** - the plugin crashed BEFORE it could even connect.

## Root Cause

The plugin requires **ALL six Storage Configuration parameters** to be filled in, but they have **NO default values**. If any parameter is empty, the plugin crashes immediately when trying to build file paths.

## The Code Flow

**Step-by-step what happens during render():**

1. **Line 111** (`comfyui_base_plugin.cpp`): Plugin tries to fetch source image
   ```cpp
   std::unique_ptr<OFX::Image> src(_srcClip->fetchImage(args.time));
   ```

2. **Line 112**: Plugin calls `writeInputImage()` to write input EXR
   ```cpp
   std::string inputPath = writeInputImage(src.get(), frame);
   ```

3. **Lines 234-239** (`writeInputImage`): Gets parameter values
   ```cpp
   std::string mountPath, flameProject, workflow, basename, layer, version;
   _sharedMountPath->getValue(mountPath);          // ← Empty if not set!
   _flameProjectName->getValue(flameProject);      // ← Empty if not set!
   _workflowName->getValue(workflow);              // ← Empty if not set!
   _basename->getValue(basename);                   // ← Empty if not set!
   _layerName->getValue(layer);                     // ← Empty if not set!
   _outputVersion->getValue(version);               // ← Empty if not set!
   ```

4. **Lines 244-247**: Builds filepath (if params empty, creates invalid path)
   ```cpp
   filename << mountPath << "/in/" << flameProject << "/" << workflow << "/"
            << basename << "_" << layer << "_"
            << std::setw(4) << std::setfill('0') << frame << "_"
            << version << "_.exr";
   // Example if ALL empty: "/in////___0001__.exr"
   ```

5. **Line 276**: Tries to write EXR to invalid path
   ```cpp
   ImageIO::writeEXR(filename.str(), imageData);
   // CRASHES - directory doesn't exist!
   ```

## Why Nothing Was Sent to ComfyUI

The workflow is only queued to ComfyUI at **line 120** (`executeWorkflow`):

```cpp
std::string promptId = _comfyClient->queuePrompt(workflow, _comfyClient->getClientId());
```

But the plugin crashes at line 276 (writeInputImage) which happens at line 112 - **BEFORE** reaching line 120!

## The Missing Parameters

These parameters are **required** but have **no defaults**:

```
Storage Configuration:
  ✗ Shared Mount Path: (empty)
  ✗ Project Name: (empty)
  ✗ Workflow Name: (empty)
  ✗ Basename: (empty)
  ✗ Layer Name: (empty)
  ✗ Output Version: (empty)
```

If ANY of these are empty, you get:
- Invalid file path like: `/in////___0001__.exr`
- Directory creation fails
- EXR write fails
- Plugin crashes with exception
- Flame shows: "Plugin rendering failed"

## Why Changing Server Address Didn't Help

You tried:
1. Server Address: `localhost` → Failed
2. Server Address: `192.168.1.211` → Still failed

Both failed because **the plugin never reached the server connection code**. It crashed earlier when trying to write the input file.

## The Fix

You MUST fill in ALL six storage parameters:

```
Storage Configuration:
  Shared Mount Path: /Z                    ← Required!
  Project Name: TEST_SAM                   ← Required!
  Workflow Name: segmentation              ← Required!
  Basename: shot01                         ← Required!
  Layer Name: beauty                       ← Required!
  Output Version: v001                     ← Required!
```

## How to Verify Parameters Are Set

Before rendering, check the plugin UI and ensure you see:

```
Storage Configuration Group (expanded):
  Shared Mount Path: /Z
  Project Name: TEST_SAM
  Workflow Name: segmentation
  Basename: shot01
  Layer Name: beauty
  Output Version: v001
```

If ANY field shows empty or has a default placeholder, the plugin will crash.

## Better Error Handling (Future Improvement)

The plugin should validate parameters BEFORE trying to use them:

```cpp
std::string BasePlugin::writeInputImage(OFX::Image* img, int frame)
{
    // Get path components
    std::string mountPath, flameProject, workflow, basename, layer, version;
    _sharedMountPath->getValue(mountPath);
    _flameProjectName->getValue(flameProject);
    _workflowName->getValue(workflow);
    _basename->getValue(basename);
    _layerName->getValue(layer);
    _outputVersion->getValue(version);

    // VALIDATION (should add this!)
    if (mountPath.empty()) {
        throw std::runtime_error("Shared Mount Path parameter is required");
    }
    if (flameProject.empty()) {
        throw std::runtime_error("Project Name parameter is required");
    }
    // ... etc for all parameters

    // Now safe to build path...
}
```

This would give you a clear error message instead of a cryptic crash.

## Testing Checklist

Before testing again:

- [ ] Verify `/Z` symlink exists: `ls /Z/in`
- [ ] Verify Z: drive mapped on Windows: Check Windows server
- [ ] Fill in ALL six Storage Configuration parameters
- [ ] Server Address: `192.168.1.211`
- [ ] Server Port: `8188`
- [ ] Apply plugin to a clip
- [ ] Render ONE frame first
- [ ] Watch for files in `/Z/in/TEST_SAM/segmentation/`
- [ ] Check ComfyUI console for workflow submission

## Success Indicators

**If parameters are correct, you'll see:**
1. Input file created: `/Z/in/TEST_SAM/segmentation/shot01_beauty_0001_v001_.exr`
2. ComfyUI console shows workflow received
3. Processing happens on ComfyUI server
4. Output file created: `/Z/out/TEST_SAM/segmentation/v001/image_0001.exr`
5. Flame displays result

**If it still fails:**
- Check the exact error in Flame
- Look for created files to see how far it got
- Check ComfyUI console for errors
- Verify models are installed

## Summary

**The plugin didn't fail because of network/server issues.**
**It failed because required parameters were not filled in.**

Fill in all six Storage Configuration parameters and try again!

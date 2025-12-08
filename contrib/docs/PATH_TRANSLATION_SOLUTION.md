# Cross-Platform Path Translation Solution

## The Problem

**Mac (Flame/OFX Plugin):**
```
/Volumes/silo2/002_COMFYUI/in/...
```

**Windows (ComfyUI Server):**
```
\\192.168.1.110\silo2\002_COMFYUI\in\...
```

The OFX plugin runs on Mac and writes Unix-style paths, but ComfyUI's LoadEXR node on Windows can't understand them.

## Solution Options

### Option 1: Add Path Translation Parameter (Recommended)

Add a new parameter to the plugin that translates Mac paths to Windows UNC paths before sending to ComfyUI.

**New Parameter:**
```cpp
// In comfyui_base_plugin.h
OFX::StringParam* _windowsPathPrefix;

// In describeCommonParameters()
OFX::StringParamDescriptor *windowsPrefix = desc.defineStringParam("windowsPathPrefix");
windowsPrefix->setLabel("Windows Path Prefix");
windowsPrefix->setHint("For Windows ComfyUI servers: \\\\192.168.1.110\\silo2");
windowsPrefix->setDefault("");
```

**Path Translation Function:**
```cpp
std::string BasePlugin::translatePathForComfyUI(const std::string& macPath) {
    std::string windowsPrefix;
    _windowsPathPrefix->getValue(windowsPrefix);

    if (windowsPrefix.empty()) {
        // No translation needed (ComfyUI on same OS)
        return macPath;
    }

    // Find the mount point in the mac path
    // Example: /Volumes/silo2 -> \\192.168.1.110\silo2
    std::string mountPath;
    _sharedMountPath->getValue(mountPath);

    if (macPath.find(mountPath) == 0) {
        // Replace Mac mount with Windows UNC path
        std::string relativePath = macPath.substr(mountPath.length());
        // Convert forward slashes to backslashes
        std::replace(relativePath.begin(), relativePath.end(), '/', '\\');
        return windowsPrefix + relativePath;
    }

    return macPath; // Fallback
}
```

**Usage in buildWorkflow():**
```cpp
// Build input path (Mac style)
std::string macInputPath = mountPath + "/in/" + flameProject + "/" + workflow_name + "/...";

// Translate for ComfyUI (Windows style if needed)
std::string comfyInputPath = translatePathForComfyUI(macInputPath);

// Use in workflow JSON
workflow["prompt"]["1"]["inputs"]["filepath"] = comfyInputPath;
```

**User Configuration in Flame:**
```
Shared Mount Path: /Volumes/silo2/002_COMFYUI
Windows Path Prefix: \\192.168.1.110\silo2\002_COMFYUI
```

### Option 2: Environment Variable (Simple but less flexible)

Set an environment variable before starting Flame:
```bash
export COMFYUI_WINDOWS_PATH_PREFIX="\\\\192.168.1.110\\silo2\\002_COMFYUI"
```

Then read it in the plugin.

### Option 3: Configuration File (Most flexible)

Create a JSON config file:
```json
{
  "path_mappings": [
    {
      "mac": "/Volumes/silo2/002_COMFYUI",
      "windows": "\\\\192.168.1.110\\silo2\\002_COMFYUI"
    }
  ]
}
```

### Option 4: Map Drive on Windows (Simplest, no code change)

On your Windows ComfyUI server, map the share to a drive letter:
```powershell
# Map as Z: drive
net use Z: \\192.168.1.110\silo2\002_COMFYUI /persistent:yes
```

Then modify the Mac mount to match:
```bash
# Symlink to make paths align
sudo mkdir -p /Z
sudo mount_smbfs //reepost@192.168.1.110/silo2/002_COMFYUI /Z
```

Then use:
```
Shared Mount Path: /Z
```

Both Mac and Windows will use compatible paths.

## Recommended Implementation

**Quick Test (No Code Changes):**
1. On Windows: `net use Z: \\192.168.1.110\silo2\002_COMFYUI /persistent:yes`
2. On Mac: Create symlink: `sudo ln -s /Volumes/silo2/002_COMFYUI /Z`
3. In Flame: Use `Shared Mount Path: /Z`

**Proper Solution (Add to plugin):**
Implement Option 1 (Path Translation Parameter) so users can easily configure cross-platform setups.

## Testing

**On Windows ComfyUI server:**
```powershell
# Test access
dir \\192.168.1.110\silo2\002_COMFYUI\in

# Or with mapped drive
net use Z: \\192.168.1.110\silo2\002_COMFYUI
dir Z:\in
```

**Expected workflow JSON path (for Windows ComfyUI):**
```json
{
  "1": {
    "inputs": {
      "filepath": "\\\\192.168.1.110\\silo2\\002_COMFYUI\\in\\TEST_SAM\\segmentation\\shot01_beauty_0001_v001_.exr"
    }
  }
}
```

Or with mapped drive:
```json
{
  "1": {
    "inputs": {
      "filepath": "Z:\\002_COMFYUI\\in\\TEST_SAM\\segmentation\\shot01_beauty_0001_v001_.exr"
    }
  }
}
```

## Next Steps

1. **Test Windows access first:**
   ```powershell
   dir \\192.168.1.110\silo2\002_COMFYUI
   ```

2. **If that works, map as Z: drive:**
   ```powershell
   net use Z: \\192.168.1.110\silo2\002_COMFYUI /persistent:yes
   ```

3. **On Mac, create matching /Z path:**
   ```bash
   # Option A: Symlink (simple)
   sudo ln -s /Volumes/silo2/002_COMFYUI /Z

   # Option B: Direct mount (cleaner)
   sudo mkdir -p /Z
   sudo mount_smbfs //reepost@192.168.1.110/silo2/002_COMFYUI /Z
   ```

4. **Test in Flame with:**
   ```
   Shared Mount Path: /Z
   ```

This way both Mac and Windows see the same path structure, and no code changes are needed!

# Path Conversion Changes for Cross-Platform Support

## Problem

The plugin runs on macOS and writes Unix-style paths (`/Z/in/...`) to the workflow JSON, but sends this JSON to a Windows ComfyUI server which cannot understand Unix paths.

## Solution Implemented

Added automatic path conversion that translates Unix paths to Windows paths before sending to ComfyUI.

## Changes Made

### 1. Added Path Conversion Function (comfyui_base_plugin.h/cpp)

**Header (comfyui_base_plugin.h line 74):**
```cpp
std::string convertPathForComfyUI(const std::string& localPath);
```

**Implementation (comfyui_base_plugin.cpp lines 346-368):**
```cpp
std::string BasePlugin::convertPathForComfyUI(const std::string& localPath)
{
    // Convert Unix/Mac path to Windows path for remote ComfyUI server
    // Example: /Z/in/test.exr -> Z:\in\test.exr

    std::string windowsPath = localPath;

    // Check if path starts with /Z/ (our symlink convention)
    if (windowsPath.find("/Z/") == 0) {
        // Remove leading slash: /Z/ -> Z/
        windowsPath = windowsPath.substr(1);
        // Replace first / with :: Z/ -> Z:
        size_t firstSlash = windowsPath.find('/');
        if (firstSlash != std::string::npos) {
            windowsPath[firstSlash] = ':';
        }
    }

    // Replace all remaining forward slashes with backslashes
    std::replace(windowsPath.begin(), windowsPath.end(), '/', '\\');

    return windowsPath;
}
```

### 2. Updated SAM Plugin to Use Conversion (sam_segmentation_plugin.cpp)

**Before (lines 100-113):**
```cpp
std::ostringstream inputPath, outputPrefix;
inputPath << mountPath << "/in/" << flameProject << "/" << workflow_name
          << "/image_" << std::setw(4) << std::setfill('0') << frame << ".exr";
outputPrefix << mountPath << "/out/" << flameProject << "/" << workflow_name
             << "/" << version << "/image";

json workflow = {
    {"prompt", {
        {"1", {
            {"inputs", {
                {"filepath", inputPath.str()},  // Unix path sent to Windows!
```

**After (lines 106-117):**
```cpp
std::ostringstream inputPath, outputPrefix;
inputPath << mountPath << "/in/" << flameProject << "/" << workflow_name
          << "/image_" << std::setw(4) << std::setfill('0') << frame << ".exr";
outputPrefix << mountPath << "/out/" << flameProject << "/" << workflow_name
             << "/" << version << "/image";

// Convert paths for ComfyUI (Windows server)
std::string comfyInputPath = convertPathForComfyUI(inputPath.str());
std::string comfyOutputPrefix = convertPathForComfyUI(outputPrefix.str());

json workflow = {
    {"prompt", {
        {"1", {
            {"inputs", {
                {"filepath", comfyInputPath},  // Windows path!
```

## Path Conversion Examples

| Mac Plugin Path | Workflow JSON Path (Windows) |
|----------------|------------------------------|
| `/Z/in/TEST_SAM/segmentation/shot01_beauty_0001_v001_.exr` | `Z:\in\TEST_SAM\segmentation\shot01_beauty_0001_v001_.exr` |
| `/Z/out/TEST_SAM/segmentation/v001/image` | `Z:\out\TEST_SAM\segmentation\v001\image` |

## Configuration Required

### On Windows ComfyUI Server (192.168.1.211):
```powershell
net use Z: \\192.168.1.110\silo2\002_COMFYUI /user:reepost /persistent:yes
```

### On Mac Flame Machine:
```bash
sudo ln -s /Volumes/silo2/002_COMFYUI /Z
```

### In Flame Plugin Parameters:
```
Shared Mount Path: /Z
```

## How It Works

1. **Plugin writes file locally:**
   - Uses Mac path: `/Z/in/TEST_SAM/segmentation/shot01_beauty_0001_v001_.exr`
   - File is written to `/Z/in/...` (which is a symlink to `/Volumes/silo2/002_COMFYUI/in/...`)

2. **Plugin builds workflow JSON:**
   - Converts Mac path to Windows path
   - Sends: `Z:\in\TEST_SAM\segmentation\shot01_beauty_0001_v001_.exr`

3. **ComfyUI (Windows) reads file:**
   - Receives Windows path: `Z:\in\...`
   - Reads from `Z:` drive (mapped to `\\192.168.1.110\silo2\002_COMFYUI`)
   - Successfully loads the EXR file

4. **ComfyUI writes output:**
   - Writes to Windows path: `Z:\out\TEST_SAM\segmentation\v001\image_0001.exr`

5. **Plugin reads result:**
   - Converts output path back to Mac path
   - Reads from: `/Z/out/TEST_SAM/segmentation/v001/image_0001.exr`

## Benefits

- **No manual path configuration needed** - Automatic conversion
- **Works with `/Z` symlink convention** - Simple setup
- **Extensible** - Can be enhanced to support other mount points
- **Backwards compatible** - If paths don't start with `/Z/`, they pass through unchanged

## Testing

After rebuild and reinstall:

1. Verify path conversion with simple test:
   ```cpp
   std::string test = "/Z/in/test.exr";
   std::string result = convertPathForComfyUI(test);
   // Expected: "Z:\\in\\test.exr"
   ```

2. Test in Flame:
   - Configure parameters with `/Z` as mount path
   - Render a frame
   - Check ComfyUI console for path in workflow JSON
   - Should see `Z:\in\...` not `/Z/in/...`

## Future Enhancements

Could add a parameter to make conversion configurable:
- "ComfyUI OS Type": Choice (Unix/Windows/Auto)
- Only convert when set to Windows
- Auto-detect based on server response

Or support multiple mount points:
- `/Volumes/silo2` → `\\\\192.168.1.110\\silo2`
- Configurable path mapping table

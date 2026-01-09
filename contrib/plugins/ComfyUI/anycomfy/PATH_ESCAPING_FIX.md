# Path Escaping Fix - Double Backslash Issue

## Problem

When using AnyComfy with real ComfyUI workflows, the plugin was generating paths with **double-escaped backslashes**, causing errors like:

```
FileNotFoundError: [WinError 3] The system cannot find the path specified:
'Z:\\\\out\\\\ANYCOMFY,_TEST\\\\any_segmentation\\\\v001'
```

Notice the quadruple backslashes `\\\\` in the error message. The path should be:
```
Z:\out\ANYCOMFY_TEST\any_segmentation\v001
```

But ComfyUI was receiving:
```
Z:\\out\\ANYCOMFY_TEST\\any_segmentation\\v001
```

(Double backslashes instead of single)

## Root Cause

### The Double-Escaping Bug

The issue was caused by **double JSON escaping**:

1. **First Escape**: The `convertPathForComfyUI()` function manually escaped backslashes:
   ```cpp
   // Manual escaping
   std::string jsonPath;
   for (char c : windowsPath) {
       if (c == '\\') {
           jsonPath += "\\\\";  // Z:\out → Z:\\out
       } else {
           jsonPath += c;
       }
   }
   ```

2. **Second Escape**: `nlohmann_json` **automatically** escapes backslashes when serializing to JSON:
   ```cpp
   nodeData["inputs"]["filepath"] = comfyInputPath;  // Z:\\out → "Z:\\\\out" in JSON
   ```

3. **Result**: Path gets double-escaped:
   ```
   Original:      Z:\out\project\v001
   After manual:  Z:\\out\\project\\v001
   In JSON:       "Z:\\\\out\\\\project\\\\v001"
   Parsed by UI:  Z:\\out\\project\\v001  (WRONG!)
   ```

### Why This Happened

The original `convertPathForComfyUI()` was designed for **template-based workflows** where paths were inserted via string replacement:

```cpp
// String-based workflow (old approach)
std::string workflowStr = workflow.dump();
workflowStr.replace(pos, placeholder.length(), comfyInputPath);
json finalWorkflow = json::parse(workflowStr);
```

In this case, manual escaping was necessary because we were replacing into a JSON **string**.

But the new **smart injection** directly modifies JSON **objects**:

```cpp
// Object-based workflow (new approach)
nodeData["inputs"]["filepath"] = comfyInputPath;  // nlohmann_json handles escaping!
```

Here, `nlohmann_json` automatically handles escaping, so manual escaping causes double-escaping.

## Solution

### Remove Manual Escaping in Smart Injection

Changed `injectPathsIntoWorkflow()` to do path conversion **without** manual JSON escaping:

**Before (Wrong - Double Escaping):**
```cpp
std::string comfyInputPath = convertPathForComfyUI(inputPath);  // Manually escapes
nodeData["inputs"]["filepath"] = comfyInputPath;  // nlohmann_json escapes again!
```

**After (Correct - Single Escaping):**
```cpp
// Convert path WITHOUT manual JSON escaping
std::string comfyInputPath = inputPath;
if (comfyInputPath.find(clientMount) == 0) {
    comfyInputPath.replace(0, clientMount.length(), serverMount);
}
std::replace(comfyInputPath.begin(), comfyInputPath.end(), '/', '\\');

// nlohmann_json handles escaping automatically
nodeData["inputs"]["filepath"] = comfyInputPath;  // Single escaping ✓
```

### Path Conversion Flow

**Correct Flow:**
```
Client path:   /mnt/share/out/project/v001
Mount replace: Z:/out/project/v001
Slash convert: Z:\out\project\v001
In JSON:       "Z:\\out\\project\\v001"  (nlohmann_json escapes)
Parsed by UI:  Z:\out\project\v001  ✓ CORRECT
```

## Code Changes

### File: anycomfy_plugin.cpp

#### Before

```cpp
json AnyComfyPlugin::injectPathsIntoWorkflow(const json& workflow, int frame,
                                              const std::string& inputPath,
                                              const std::string& outputPrefix)
{
    // Convert paths to ComfyUI format (Windows paths if needed)
    std::string comfyInputPath = convertPathForComfyUI(inputPath);      // ❌ Double escaping
    std::string comfyOutputPrefix = convertPathForComfyUI(outputPrefix); // ❌ Double escaping

    // ...inject into workflow...
}
```

#### After

```cpp
json AnyComfyPlugin::injectPathsIntoWorkflow(const json& workflow, int frame,
                                              const std::string& inputPath,
                                              const std::string& outputPrefix)
{
    // Convert paths to ComfyUI format (Windows paths if needed)
    // IMPORTANT: We need the RAW Windows path WITHOUT manual JSON escaping
    // because nlohmann_json will automatically escape backslashes when serializing
    std::string clientMount, serverMount;
    _sharedMountPath->getValue(clientMount);
    _serverMountPoint->getValue(serverMount);

    // Convert input path (replace mount + forward slashes → backslashes)
    std::string comfyInputPath = inputPath;
    if (comfyInputPath.find(clientMount) == 0) {
        comfyInputPath.replace(0, clientMount.length(), serverMount);
    }
    std::replace(comfyInputPath.begin(), comfyInputPath.end(), '/', '\\');  // ✓ Raw path

    // Convert output prefix (same process)
    std::string comfyOutputPrefix = outputPrefix;
    if (comfyOutputPrefix.find(clientMount) == 0) {
        comfyOutputPrefix.replace(0, clientMount.length(), serverMount);
    }
    std::replace(comfyOutputPrefix.begin(), comfyOutputPrefix.end(), '/', '\\');  // ✓ Raw path

    // ...inject into workflow...
    // nlohmann_json will automatically escape backslashes when serializing ✓
}
```

## Testing

### Before Fix

**Plugin Output (logs):**
```
Injecting input path: Z:\\in\\project\\input.exr
Injecting output prefix: Z:\\out\\project\\v001\\output
```

**JSON Sent to ComfyUI:**
```json
{
  "3": {
    "inputs": {
      "filepath": "Z:\\\\in\\\\project\\\\input.exr"
    }
  }
}
```

**Error:**
```
FileNotFoundError: Path not found: Z:\\in\\project\\input.exr
```

### After Fix

**Plugin Output (logs):**
```
Injecting input path (raw Windows): Z:\in\project\input.exr
Injecting output prefix (raw Windows): Z:\out\project\v001\output
(nlohmann_json will auto-escape backslashes in JSON output)
```

**JSON Sent to ComfyUI:**
```json
{
  "3": {
    "inputs": {
      "filepath": "Z:\\in\\project\\input.exr"
    }
  }
}
```

**Parsed by ComfyUI:**
```python
filepath = "Z:\in\project\input.exr"  # ✓ Correct path
```

**Success!** ✓

## Important Notes

### When to Manually Escape

**DO manually escape** when doing string-based replacement:
```cpp
std::string workflowStr = workflow.dump();  // Convert to string
workflowStr.replace(pos, "${INPUT_PATH}", escapedPath);  // Need escaping
json final = json::parse(workflowStr);
```

**DON'T manually escape** when setting JSON object properties:
```cpp
json workflow;
workflow["inputs"]["filepath"] = rawPath;  // DON'T escape - nlohmann does it
```

### Impact on Template-Based Workflows

The `convertPathForComfyUI()` function is still used for **template-based workflows** in the base `customizeWorkflow()` method, which does string-based replacement. This is correct and unchanged.

The fix only affects **smart injection** in AnyComfy, which uses object-based modification.

## Files Modified

1. **anycomfy_plugin.cpp**
   - Line 310-336: Modified `injectPathsIntoWorkflow()` to use raw paths without manual escaping

## Version History

- **v1.0.0**: Initial release with smart injection
- **v1.0.1**: Smart injection added
- **v1.0.2**: Path escaping fix (this fix)

## Related Issues

- **Smart Injection**: [SMART_INJECTION_FIX.md](SMART_INJECTION_FIX.md)
- **Directory Setup**: [DIRECTORY_SETUP_GUIDE.md](DIRECTORY_SETUP_GUIDE.md)

---

**Fixed**: December 26, 2025
**Plugin Version**: 1.0.2
**Issue**: Double JSON escaping causing invalid Windows paths

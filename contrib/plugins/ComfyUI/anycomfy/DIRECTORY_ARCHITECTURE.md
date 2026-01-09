# Directory Creation Architecture - No Code Duplication

## Summary

**INPUT directories**: ✅ Automatically created by existing code in `comfyui_image_io.cpp`
**OUTPUT directories**: ⚠️ Must be pre-created on server (limitation of ComfyUI's SaveEXR node)

**No code duplication** - All directory creation uses the same `createDirectoryRecursive()` function.

## Architecture

### Central Directory Creation Function

**Location**: `contrib/plugins/ComfyUI/common/comfyui_image_io.cpp`

```cpp
bool createDirectoryRecursive(const std::string& path) {
    // Check if directory already exists
    struct stat st;
    if (stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) {
        return true; // Already exists
    }

    // Find parent directory (RECURSIVE)
    size_t pos = path.find_last_of("/\\");
    if (pos != std::string::npos) {
        std::string parent = path.substr(0, pos);
        if (!createDirectoryRecursive(parent)) {  // ← Recursion creates full tree
            return false;
        }
    }

    // Create this directory
    if (mkdir(path.c_str(), 0755) != 0 && errno != EEXIST) {
        return false;
    }

    return true;
}
```

**Key Feature**: Recursively creates the full directory tree (like `mkdir -p`)

### Call Chain for INPUT Directories (CLIENT-SIDE)

```
BasePlugin::render()
  → BasePlugin::writeInputImage()  [line 1119]
      → ImageIO::writeEXR()  [comfyui_image_io.cpp:82]
          → createDirectoryRecursive()  [comfyui_image_io.cpp:93]
              ✅ Creates full path: /mnt/share/in/project/workflow/v001/
```

**Code Location**:
- `comfyui_base_plugin.cpp:1119` - Calls `ImageIO::writeEXR()`
- `comfyui_image_io.cpp:93` - Creates directory before writing

**Example**:
```
Input file:  /mnt/share/in/TEST/segmentation/v001/shot01_0001.exr
Creates:     /mnt/share/in/TEST/segmentation/v001/  ← Full tree created ✓
```

### OUTPUT Directories (SERVER-SIDE) - Why We Can't Create Them

**The Problem**:
```
OFX Plugin (Client)          ComfyUI Server
-------------------          ---------------
1. Build workflow JSON
2. Send to server    ───────►
                             3. Execute workflow
                             4. SaveEXR node tries to write
                             5. os.mkdir(dirname)  ← Only creates ONE level!
                                                    ← FAILS if parent doesn't exist!
```

**Why ComfyUI's SaveEXR Fails**:

```python
# ComfyUI's SaveEXR node (simplified)
def save_images(self, filename_prefix, ...):
    basepath = f"{output_dir}/{filename_prefix}.{frame:04d}.exr"

    # PROBLEM: Only creates immediate parent directory
    os.mkdir(os.path.dirname(basepath))  # ← Only ONE level!

    # If basepath = "Z:\out\PROJECT\workflow\v001\file.exr"
    # This tries: os.mkdir("Z:\out\PROJECT\workflow\v001")
    # But FAILS if "Z:\out\PROJECT\workflow" doesn't exist!
```

**What it should do**:
```python
os.makedirs(os.path.dirname(basepath), exist_ok=True)  # Creates full tree
```

**But we can't modify ComfyUI's code** - it's a third-party server.

### Why We Can't Fix This in the Plugin

| Approach | Why It Won't Work |
|----------|-------------------|
| Create via SSH | We don't have SSH credentials to server |
| Create via HTTP API | ComfyUI has no "create directory" endpoint |
| Create via workflow | Would need to add custom ComfyUI node |
| Create from client | Output path is on remote server, not accessible |

### The Correct Solution

**Pre-create output directories on the ComfyUI server** before running workflows.

This is a **ONE-TIME SETUP** per project, not something that needs to happen per-frame.

## No Code Duplication

### Existing Code (Used by ALL plugins)

1. **`createDirectoryRecursive()`** - Single implementation in `comfyui_image_io.cpp`
2. **`ImageIO::writeEXR()`** - Calls `createDirectoryRecursive()` for input files
3. **`BasePlugin::writeInputImage()`** - Uses `ImageIO::writeEXR()`

**Used by**:
- ✅ SAM Segmentation plugin
- ✅ AnyComfy plugin
- ✅ Any future ComfyUI plugins

**No duplication** - All plugins share the same base code.

### What AnyComfy Adds

**Nothing** related to directory creation! AnyComfy only adds:
- Smart workflow injection (modifying JSON nodes)
- Path conversion (mount point replacement)
- Template workflow generation

**Directory creation** is handled by the existing base code.

## User Setup Requirements

### Automatic (Handled by Plugin)

✅ **INPUT directory creation** - Plugin creates full tree:
```
/mnt/share/in/PROJECT/workflow/v001/
```

### Manual (User Must Do)

⚠️ **OUTPUT directory creation** - User creates on server:
```bash
# On ComfyUI server
mkdir -p Z:\out\PROJECT\workflow\v001
# or
python directory_creator.py PROJECT workflow
```

## Comparison with SAM Plugin

### SAM Plugin

**Input directories**: Created by `BasePlugin::writeInputImage()` ✓
**Output directories**: Must be pre-created by user ⚠️

### AnyComfy Plugin

**Input directories**: Created by `BasePlugin::writeInputImage()` ✓ (same code)
**Output directories**: Must be pre-created by user ⚠️ (same requirement)

**No difference** - Both use the same base code.

## Future Enhancement (Optional)

If we wanted to automatically create output directories, we would need to:

### Option 1: Custom ComfyUI Node

Create a custom node that runs BEFORE SaveEXR:

```python
class DirectoryCreator:
    def create_directory(self, path):
        import os
        os.makedirs(os.path.dirname(path), exist_ok=True)
        return ()
```

Add to workflow:
```json
{
  "99": {
    "inputs": {
      "path": "${OUTPUT_PREFIX}_0001.exr"
    },
    "class_type": "DirectoryCreator"
  },
  "100": {
    "inputs": {
      "filename_prefix": "${OUTPUT_PREFIX}",
      "images": ["...", 0]
    },
    "class_type": "SaveEXR"
  }
}
```

### Option 2: HTTP API Endpoint

Add to ComfyUI server:

```python
@app.route('/create_directory', methods=['POST'])
def create_directory():
    path = request.json['path']
    os.makedirs(path, exist_ok=True)
    return {'status': 'success'}
```

Then in plugin:
```cpp
_comfyClient->createDirectory(outputDirectory);  // Before workflow submission
```

### Option 3: Modify SaveEXR Node

Change SaveEXR to use `os.makedirs()` instead of `os.mkdir()`:

```python
# In ComfyUI custom node
os.makedirs(os.path.dirname(basepath), exist_ok=True)  # Full tree
```

**But**: Requires modifying third-party code.

## Conclusion

**Current Architecture**: ✅ No code duplication
- INPUT directories: Handled by existing `createDirectoryRecursive()` in base code
- OUTPUT directories: User responsibility (ComfyUI server limitation)

**AnyComfy**: Uses the same base code as SAM plugin
- No special directory handling
- No code duplication
- Consistent behavior across all ComfyUI plugins

**User Action**: Pre-create output directories on server (one-time setup)

---

**Architecture**: Centralized in `comfyui_image_io.cpp`
**Code Sharing**: All ComfyUI plugins use the same base code
**Duplication**: None ✓

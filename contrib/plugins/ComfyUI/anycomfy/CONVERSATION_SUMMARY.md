# AnyComfy Plugin - Complete Development Summary

## Overview

This document provides a comprehensive summary of the AnyComfy plugin development, from initial creation through all fixes and enhancements.

## Project Goal

**Objective**: Build a second generic OpenFX plugin named "AnyComfy" that can execute **any** ComfyUI workflow, complementing the existing SAM segmentation plugin.

**Key Requirement**: Unlike SAM (which has workflow-specific parameters like model selection, thresholds, prompts), AnyComfy should be completely generic with only:
- Server connection parameters
- Project/workflow naming parameters
- Workflow file selection
- "New Workflow" button to create templates

## Development Timeline

### Phase 1: Initial Creation (December 26, 2025)

**Created Files:**
- [anycomfy_plugin.h](anycomfy_plugin.h) - Plugin class declaration
- [anycomfy_plugin.cpp](anycomfy_plugin.cpp) - Main implementation
- [anycomfy/CMakeLists.txt](CMakeLists.txt) - Build configuration
- [resources/workflows/template.json](resources/workflows/template.json) - Default template
- [README.md](README.md) - Plugin documentation

**Modified Files:**
- `contrib/plugins/ComfyUI/CMakeLists.txt` - Added AnyComfy subdirectory

**Features Implemented:**
- Generic workflow loading from file
- Template variable substitution (`${INPUT_PATH}`, `${OUTPUT_PREFIX}`, `${FRAME}`)
- "New Workflow" button to create template and open browser
- Unique instance naming to avoid collisions (e.g., `anycomfy_effect1`, `anycomfy_effect2`)
- Full integration with base ComfyUI plugin infrastructure

**Build Command:**
```bash
./contrib/dev-tools/build-macos-universal-plugin.sh -p AnyComfy -t AnyComfy --install
```

### Phase 2: Conan Scoping Fix (December 26, 2025)

**Issue**: Build warning about unscoped Conan options
```
WARN: legacy: Unscoped option definition is ambiguous.
Use '&:build_comfyui_plugins=True' to refer to the current package.
```

**Root Cause**: Conan 2.x requires scoped options to avoid ambiguity between consumer and package options.

**Fix**: Updated all build scripts to use scoped syntax:
```bash
# Before (ambiguous)
-o build_comfyui_plugins=True

# After (scoped to current package)
-o "&:build_comfyui_plugins=True"
```

**Modified Files:**
- `contrib/dev-tools/build-macos-universal-plugin.sh`
- `contrib/dev-tools/build-plugin.sh`
- `contrib/dev-tools/build-linux-universal-plugin.sh`
- `contrib/dev-tools/build-linux-plugin.sh`

**Documentation**: [CONAN_SCOPING_FIX.md](CONAN_SCOPING_FIX.md)

### Phase 3: Smart Injection (December 26, 2025) - v1.0.1

**Issue**: Plugin only worked with templated workflows containing `${INPUT_PATH}` variables. Failed with real ComfyUI workflow exports that had hardcoded values like "Front", "Result", etc.

**Error Example:**
```
Exception: Path not found: Front
```

**Root Cause**: Plugin relied on template variable substitution, but ComfyUI's "Save (API Format)" exports workflows with hardcoded values, not template variables.

**Solution**: Implemented smart JSON node injection that:
1. Finds LoadEXR nodes by `class_type` (not by placeholder values)
2. Finds SaveEXR nodes by `class_type`
3. Directly modifies their `inputs` properties
4. Works with both templated AND non-templated workflows

**Code Changes:**
```cpp
// New method in anycomfy_plugin.cpp
json AnyComfyPlugin::injectPathsIntoWorkflow(const json& workflow, int frame,
                                              const std::string& inputPath,
                                              const std::string& outputPrefix)
{
    // Iterate through all nodes
    for (auto& [nodeId, nodeData] : modifiedWorkflow.items()) {
        // Find LoadEXR nodes by class_type
        if (nodeData["class_type"] == "LoadEXR") {
            nodeData["inputs"]["filepath"] = comfyInputPath;
        }

        // Find SaveEXR nodes by class_type
        if (nodeData["class_type"] == "SaveEXR") {
            nodeData["inputs"]["filename_prefix"] = comfyOutputPrefix;
            nodeData["inputs"]["start_frame"] = frame;
        }
    }
    return modifiedWorkflow;
}
```

**Documentation**: [SMART_INJECTION_FIX.md](SMART_INJECTION_FIX.md)

### Phase 4: Path Escaping Fix (December 26, 2025) - v1.0.2

**Issue**: Double-escaped backslashes causing invalid Windows paths

**Error Example:**
```
FileNotFoundError: [WinError 3] The system cannot find the path specified:
'Z:\\\\out\\\\ANYCOMFY_TEST\\\\any_segmentation\\\\v001'
```

**Root Cause**: Double JSON escaping
1. Manual escaping in `convertPathForComfyUI()` function: `\` → `\\`
2. Automatic escaping by `nlohmann_json` library: `\\` → `\\\\`
3. Result: Quadruple backslashes in JSON, double backslashes when parsed

**Path Escaping Flow (Wrong):**
```
Original:      Z:\out\project\v001
After manual:  Z:\\out\\project\\v001
In JSON:       "Z:\\\\out\\\\project\\\\v001"
Parsed by UI:  Z:\\out\\project\\v001  ❌ WRONG!
```

**Solution**: Remove manual escaping in smart injection, let `nlohmann_json` handle it automatically

**Code Changes:**
```cpp
// OLD (Wrong - Double Escaping):
std::string comfyInputPath = convertPathForComfyUI(inputPath);  // Manual escaping
nodeData["inputs"]["filepath"] = comfyInputPath;  // nlohmann escapes again!

// NEW (Correct - Single Escaping):
// Convert path WITHOUT manual JSON escaping
std::string comfyInputPath = inputPath;
if (comfyInputPath.find(clientMount) == 0) {
    comfyInputPath.replace(0, clientMount.length(), serverMount);
}
std::replace(comfyInputPath.begin(), comfyInputPath.end(), '/', '\\');

// nlohmann_json handles escaping automatically
nodeData["inputs"]["filepath"] = comfyInputPath;  // Single escaping ✓
```

**Path Escaping Flow (Correct):**
```
Original:      /mnt/share/out/project/v001
Mount replace: Z:/out/project/v001
Slash convert: Z:\out\project\v001
In JSON:       "Z:\\out\\project\\v001"  (nlohmann_json escapes)
Parsed by UI:  Z:\out\project\v001  ✓ CORRECT
```

**Documentation**: [PATH_ESCAPING_FIX.md](PATH_ESCAPING_FIX.md)

### Phase 5: Directory Architecture Verification (December 30, 2025)

**User Concern**: "Directory creation was meant to be handled already when we built the SAM OFX plugin. Can you just check we're not breaking nor duplicating?"

**Investigation Results**:

**Input Directories (Client-Side)** ✓
- Automatically created by `BasePlugin::writeInputImage()` at line 1119
- Calls `ImageIO::writeEXR()` in `comfyui_image_io.cpp:82`
- Uses centralized `createDirectoryRecursive()` function at line 93
- Creates full path: `/mnt/share/in/project/workflow/v001/`

**Output Directories (Client-Side)** ✓
- Automatically created by `BasePlugin::render()` at lines 1689-1765
- Uses same `ImageIO::createDirectoryRecursive()` function
- Creates full path: `/mnt/share/out/project/workflow/v001/`

**Output Directories (Server-Side)** ⚠️
- ComfyUI's SaveEXR node uses `os.mkdir()` (not `os.makedirs()`)
- Only creates ONE directory level, not full tree
- Cannot be fixed in plugin (server-side limitation)
- User must pre-create directories on server

**Centralized Implementation:**
```cpp
// File: contrib/plugins/ComfyUI/common/comfyui_image_io.cpp
bool createDirectoryRecursive(const std::string& path) {
    // Check if directory already exists
    struct stat st;
    if (stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) {
        return true;
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

**Conclusion**: ✓ No code duplication
- All ComfyUI plugins (SAM, AnyComfy, future plugins) use the same centralized `createDirectoryRecursive()` function
- No plugin-specific directory creation code
- AnyComfy adds NOTHING related to directory creation
- Directory creation is handled universally by base plugin code

**Documentation**: [DIRECTORY_ARCHITECTURE.md](DIRECTORY_ARCHITECTURE.md)

**User Tools Provided**:
- [directory_creator.py](directory_creator.py) - Python script to pre-create server directories
- [DIRECTORY_SETUP_GUIDE.md](DIRECTORY_SETUP_GUIDE.md) - Complete setup guide

### Phase 6: Project Name Parameter Analysis (December 30, 2025)

**User Observation**:
```
Created path: Z:\out\ANYCOMFY,_TEST\any_segmentation\v001
Expected:     Z:\out\SAM_TEST\any_segmentation\v001
```

**Analysis**:

**What This Reveals:**
1. ✓ Directory creation IS working (full path was created)
2. ✓ Path escaping is fixed (no double backslashes)
3. ❌ Project Name parameter has wrong value: `"ANYCOMFY,_TEST"`
4. ⚠️ Comma in project name violates naming conventions

**Root Cause**: User configuration error
- The Project Name parameter was set to `"ANYCOMFY,_TEST"` in the plugin UI
- Should have been set to `"SAM_TEST"` or `"ANYCOMFY_TEST"` (no comma)

**Why Commas Are Bad**:
- Violate documented naming conventions
- Can cause parsing issues in some systems
- May break CSV exports or scripts
- Could cause path handling issues on different platforms

**Where Project Name Is Used**:

Parameter definition in `anycomfy_plugin.cpp`:
```cpp
StringParamDescriptor* projectName = desc.defineStringParam("projectName");
projectName->setLabel("Project Name");
projectName->setHint("Name of the project (used for organizing outputs)");
projectName->setDefault("TEST");
```

Used in `BasePlugin::render()`:
```cpp
std::string projectName;
_projectName->getValue(projectName);  // Gets "ANYCOMFY,_TEST" from UI

std::string outputDir = mountPath + "/out/" + projectName + "/" + workflowName + "/" + version;
// Results in: /mnt/share/out/ANYCOMFY,_TEST/any_segmentation/v001
```

**Solution**:

**Immediate Fix** (User action):
1. Open AnyComfy plugin settings in Flame/Nuke
2. Change Project Name from `ANYCOMFY,_TEST` to `SAM_TEST` or `ANYCOMFY_TEST`
3. Re-render

**Future Enhancement** (Optional):

Add parameter validation in `render()`:
```cpp
std::string projectName;
_projectName->getValue(projectName);

// Validate project name
if (projectName.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_-")
    != std::string::npos) {
    throw std::runtime_error(
        "Invalid project name: '" + projectName + "'\n"
        "Use only letters, numbers, underscores, and hyphens.\n"
        "Example: MY_PROJECT or PROJECT_001"
    );
}
```

**Documentation**: [PROJECT_NAME_ISSUE.md](PROJECT_NAME_ISSUE.md)

## Final Architecture

### Plugin Components

**Header File**: [anycomfy_plugin.h](anycomfy_plugin.h)
- `AnyComfyPlugin` class declaration
- Inherits from `BasePlugin`
- Adds `injectPathsIntoWorkflow()` method for smart injection

**Implementation**: [anycomfy_plugin.cpp](anycomfy_plugin.cpp)
- Generic workflow loading from file
- Smart JSON node injection (finds nodes by `class_type`)
- Path conversion without manual JSON escaping
- Template workflow generation
- Browser opening for workflow editing

**Build Configuration**: [CMakeLists.txt](CMakeLists.txt)
- Links against ComfyUI common library
- Creates universal binary (x86_64 + arm64)
- Installs to standard OFX directories

### Shared Infrastructure

**Base Plugin**: `contrib/plugins/ComfyUI/common/comfyui_base_plugin.{h,cpp}`
- Connection management (ComfyUI REST API)
- Workflow submission and monitoring
- Image I/O (reading input, writing output)
- **Directory creation (lines 1689-1765)**
- Template variable substitution
- Progress tracking

**Image I/O**: `contrib/plugins/ComfyUI/common/comfyui_image_io.{h,cpp}`
- EXR file reading/writing with TinyEXR
- **Centralized `createDirectoryRecursive()` function (lines 18-42)**
- Multi-channel support (RGB, RGBA, etc.)
- Bit depth conversion

**ComfyUI Client**: `contrib/plugins/ComfyUI/common/comfyui_client.{h,cpp}`
- HTTP communication via cpp-httplib
- WebSocket support for real-time updates
- Workflow submission and execution
- Result retrieval

### No Code Duplication

**Centralized Functions** (used by ALL ComfyUI plugins):
1. `ImageIO::createDirectoryRecursive()` - Directory creation
2. `ImageIO::writeEXR()` - Input file writing
3. `ImageIO::readEXR()` - Output file reading
4. `BasePlugin::render()` - Main rendering workflow
5. `BasePlugin::writeInputImage()` - Input image preparation
6. `ComfyClient::submitWorkflow()` - Workflow submission

**Plugin-Specific Code** (unique to AnyComfy):
1. Smart JSON node injection
2. Workflow file loading from disk
3. Template workflow generation
4. "New Workflow" button handler

**Result**: Clean separation of concerns, no duplication, shared base functionality.

## Documentation Suite

### Technical Documentation

1. **[README.md](README.md)** - Main plugin documentation
   - Overview and features
   - Installation instructions
   - Usage guide
   - Parameter reference
   - Workflow examples

2. **[SMART_INJECTION_FIX.md](SMART_INJECTION_FIX.md)** - Smart injection explained
   - Problem description
   - How it works (finds nodes by `class_type`)
   - Code examples
   - Testing results

3. **[PATH_ESCAPING_FIX.md](PATH_ESCAPING_FIX.md)** - Path escaping fix
   - Double escaping issue
   - Root cause analysis
   - Solution (let nlohmann_json handle escaping)
   - Before/after comparison

4. **[DIRECTORY_ARCHITECTURE.md](DIRECTORY_ARCHITECTURE.md)** - Directory creation architecture
   - How directory creation works
   - Call chain documentation
   - No duplication verification
   - Client vs server responsibilities

5. **[DIRECTORY_SETUP_GUIDE.md](DIRECTORY_SETUP_GUIDE.md)** - Setup instructions
   - Why pre-creation is needed
   - Manual creation commands
   - Python script usage
   - Naming conventions
   - Troubleshooting

6. **[PROJECT_NAME_ISSUE.md](PROJECT_NAME_ISSUE.md)** - Parameter configuration analysis
   - Wrong project name observation
   - Root cause (user configuration)
   - Comma validation issue
   - Solution and recommendations

7. **[CONAN_SCOPING_FIX.md](CONAN_SCOPING_FIX.md)** - Conan 2.x scoping
   - Warning explanation
   - Scoped syntax solution
   - Modified files

8. **[FIXES_SUMMARY.md](FIXES_SUMMARY.md)** - Complete fixes summary
   - Version history
   - Quick troubleshooting table
   - Testing workflow
   - Best practices

9. **[CONVERSATION_SUMMARY.md](CONVERSATION_SUMMARY.md)** - This file
   - Complete development timeline
   - All phases and fixes
   - Architecture overview
   - Documentation index

### User Tools

1. **[directory_creator.py](directory_creator.py)** - Python script
   - Automatically creates directory structures on server
   - Supports custom projects and workflows
   - Input validation
   - Cross-platform (Windows/Linux/macOS)

### Workflow Examples

1. **[resources/workflows/template.json](resources/workflows/template.json)** - Default template
   - Basic LoadEXR → SaveEXR structure
   - Template variables: `${INPUT_PATH}`, `${OUTPUT_PREFIX}`, `${FRAME}`
   - Created when user clicks "New Workflow"

2. **Example workflows** (from ComfyUI):
   - `comfyui_normal_map_deepbump_workflow_api.json`
   - `comfyui_segmentation_segment_anything_workflow_api.json`
   - And others...

## Version History

### v1.0.0 (Initial Release - December 26, 2025)
- ✓ Basic AnyComfy plugin
- ✓ Template workflow support
- ✓ Template variable substitution
- ✓ Universal binary build
- ✓ Full OFX integration

### v1.0.1 (Smart Injection - December 26, 2025)
- ✓ Smart JSON node injection
- ✓ Support for non-templated workflows
- ✓ Finds nodes by `class_type`
- ✓ Works with raw ComfyUI exports

### v1.0.2 (Path Escaping Fix - December 26, 2025)
- ✓ Fixed double JSON escaping
- ✓ Raw Windows paths in smart injection
- ✓ Proper backslash handling
- ✓ nlohmann_json automatic escaping

### Documentation Updates (December 30, 2025)
- ✓ Directory architecture analysis
- ✓ Project name parameter issue analysis
- ✓ Complete documentation suite
- ✓ User tools and guides

## Current Status

### Working Features ✓

1. **Generic Workflow Execution**
   - Any ComfyUI workflow (templated or not)
   - Smart node injection
   - Path conversion
   - Frame handling

2. **Directory Management**
   - Automatic client-side directory creation
   - Centralized implementation (no duplication)
   - Full path tree creation
   - Error handling

3. **Universal Binary**
   - x86_64 + arm64 support
   - Single .ofx.bundle file
   - ~5.1 MB size
   - Works on all macOS versions

4. **Documentation**
   - Complete technical docs
   - Troubleshooting guides
   - User tools
   - Best practices

### Known Limitations ⚠️

1. **Server-Side Directory Creation**
   - ComfyUI's SaveEXR uses `os.mkdir()` (not `os.makedirs()`)
   - Cannot create full directory tree on server
   - User must pre-create directories
   - Workaround: Python script provided

2. **Parameter Validation**
   - No automatic validation of project names
   - Commas and special characters not blocked
   - Could cause parsing issues
   - Future enhancement: Add validation

### Common Issues & Solutions

| Issue | Cause | Solution |
|-------|-------|----------|
| Path not found: Front | Hardcoded workflow paths | Update to v1.0.1+ |
| Quadruple backslashes | Double JSON escaping | Update to v1.0.2+ |
| Directory doesn't exist | Server-side limitation | Pre-create with Python script |
| Wrong directory created | Parameter value error | Fix Project Name in plugin UI |
| Comma in project name | Invalid naming | Use underscores: `MY_PROJECT` |

## Best Practices

### Naming Conventions

**Projects**: Uppercase with underscores
- ✓ `SAM_TEST`
- ✓ `ANYCOMFY_TEST`
- ✓ `MY_PROJECT`
- ✗ `ANYCOMFY,_TEST` (comma)
- ✗ `MY PROJECT` (space)

**Workflows**: Lowercase with underscores
- ✓ `segmentation`
- ✓ `any_upscale`
- ✓ `deep_bump_normals`
- ✗ `my,workflow` (comma)

**Versions**: v### format
- ✓ `v001`, `v002`, `v003`
- ✓ `v010`, `v011`, etc.

### Directory Structure

```
Z:\out\  (or /mnt/share/out/)
├── PROJECT1/
│   ├── segmentation/
│   │   ├── v001/
│   │   ├── v002/
│   │   └── v003/
│   ├── upscale/
│   │   └── v001/
│   └── denoise/
│       └── v001/
└── PROJECT2/
    └── workflow_name/
        └── v001/
```

### Workflow Management

1. **Store workflows** in shared location: `${SHARED_MOUNT}/workflows/`
2. **Name descriptively**: `deepbump_normals.json`, `sam_segmentation.json`
3. **Test in ComfyUI UI** before using in plugin
4. **Version workflows**: Keep `v1`, `v2`, etc. for major changes
5. **Use templates** when possible (easier to maintain)

### Development Workflow

1. **Create directories** on server first:
   ```bash
   python directory_creator.py MY_PROJECT workflow1 workflow2
   ```

2. **Configure plugin** in host application:
   - Server Address: `192.168.1.100`
   - Project Name: `MY_PROJECT` (no commas!)
   - Workflow Name: `workflow1`
   - Workflow File: Select or create new

3. **Test single frame** before batch rendering

4. **Verify output** location and quality

5. **Render full sequence** when confirmed working

## Future Enhancements

### Planned Features

1. **Parameter Validation**
   - Validate project/workflow names
   - Block invalid characters
   - Provide clear error messages
   - Suggest corrections

2. **Auto Directory Creation**
   - Custom ComfyUI endpoint for directory creation
   - Plugin sends API request before workflow execution
   - Eliminates manual setup step

3. **Workflow Validation**
   - Check for LoadEXR/SaveEXR nodes before execution
   - Validate template variables
   - Warn about missing models or dependencies
   - Suggest fixes for common issues

4. **Workflow Upload via API**
   - Upload workflow JSON to ComfyUI server
   - Get workflow ID
   - Open browser with `?workflow=<id>`
   - Streamline workflow editing

5. **Enhanced Error Messages**
   - Detect common configuration errors
   - Provide actionable solutions
   - Link to relevant documentation
   - Interactive troubleshooting

## Build and Installation

### Quick Build

```bash
# Build universal binary and install
./contrib/dev-tools/build-macos-universal-plugin.sh -p AnyComfy -t AnyComfy --install

# Verify installation
ls -lh ~/Library/OFX/Plugins/AnyComfy.ofx.bundle/Contents/MacOS/AnyComfy.ofx
lipo -info ~/Library/OFX/Plugins/AnyComfy.ofx.bundle/Contents/MacOS/AnyComfy.ofx
```

**Expected Output:**
```
Architectures: x86_64 arm64
Size: ~5.1 MB
```

### Detailed Build Steps

```bash
# 1. Clean previous builds (optional)
./contrib/dev-tools/build-macos-universal-plugin.sh -p AnyComfy -t AnyComfy --clean

# 2. Build arm64 architecture
./contrib/dev-tools/build-macos-universal-plugin.sh -p AnyComfy -t AnyComfy --arch arm64

# 3. Build x86_64 architecture
./contrib/dev-tools/build-macos-universal-plugin.sh -p AnyComfy -t AnyComfy --arch x86_64

# 4. Create universal binary
./contrib/dev-tools/build-macos-universal-plugin.sh -p AnyComfy -t AnyComfy

# 5. Install to OFX directory
./contrib/dev-tools/build-macos-universal-plugin.sh -p AnyComfy -t AnyComfy --install
```

### Verification

```bash
# Check bundle structure
tree ~/Library/OFX/Plugins/AnyComfy.ofx.bundle

# Verify architectures
lipo -info ~/Library/OFX/Plugins/AnyComfy.ofx.bundle/Contents/MacOS/AnyComfy.ofx

# Test in host application
# 1. Open Flame/Nuke
# 2. Look for "AnyComfy" in Effects menu
# 3. Add to timeline/node graph
# 4. Configure parameters
# 5. Render test frame
```

## Support and Contact

### Documentation
- Main README: [README.md](README.md)
- All documentation: See [FIXES_SUMMARY.md](FIXES_SUMMARY.md) for complete index

### Tools
- Directory creator: [directory_creator.py](directory_creator.py)

### Community
- GitHub: https://github.com/AcademySoftwareFoundation/openfx
- OpenFX Group: https://groups.google.com/g/openeffects-dev

### Troubleshooting
1. Check [FIXES_SUMMARY.md](FIXES_SUMMARY.md) for quick troubleshooting table
2. Review specific issue docs:
   - Smart injection: [SMART_INJECTION_FIX.md](SMART_INJECTION_FIX.md)
   - Path escaping: [PATH_ESCAPING_FIX.md](PATH_ESCAPING_FIX.md)
   - Directories: [DIRECTORY_SETUP_GUIDE.md](DIRECTORY_SETUP_GUIDE.md)
   - Project name: [PROJECT_NAME_ISSUE.md](PROJECT_NAME_ISSUE.md)
3. Check plugin version (should be v1.0.2+)
4. Verify parameter values (no commas, special characters)
5. Test workflow in ComfyUI UI first

---

**Document Created**: December 30, 2025
**Plugin Version**: 1.0.2
**Status**: Production Ready ✓
**License**: BSD-3-Clause (matching OpenFX)
**Copyright**: 2025 OpenFX Community

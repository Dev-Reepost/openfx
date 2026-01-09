# AnyComfy Generic ComfyUI Plugin - Implementation Summary

## Overview

Successfully implemented **AnyComfy**, a generic OFX plugin that can execute any ComfyUI workflow, addressing the requirement for a flexible, workflow-agnostic plugin that complements the specific SAM segmentation plugin.

## Requirements Analysis

### Original Requirements

1. ✅ Build a second generic OFX plugin for ComfyUI
2. ✅ Allow execution of any ComfyUI workflow (as long as it has LoadEXR and SaveEXR nodes)
3. ✅ Keep UI generic without workflow-specific parameters
4. ✅ Only project/server/workflow UI elements
5. ✅ Workflow file selection from shared server workflows directory
6. ✅ "New Workflow" button to create template and open browser
7. ✅ Unique naming to avoid instance collisions
8. ✅ Anticipate and solve potential issues

### Key Design Decisions

#### 1. Complete Workflow Agnosticism
- **Zero workflow-specific parameters** - Unlike SAM (which has model selection, threshold, prompt, etc.)
- All workflow logic configured in ComfyUI UI, not in OFX
- Maximum flexibility for users

#### 2. Workflow Management
- Workflow file selector (browses from shared server)
- Template generator creates minimal LoadEXR → SaveEXR workflow
- Browser auto-opening for easy workflow editing
- Built-in template bundled with plugin

#### 3. Instance Collision Avoidance
- Each OFX instance has unique identifier from `_instanceName`
- Auto-generated workflow names: `anycomfy_<instance>_<timestamp>.json`
- Independent output directories per instance
- Separate caching per instance

#### 4. Issues Anticipated and Solved

**Issue 1: Multiple instances writing to same output**
- **Solution**: Use instance name in output prefix
- Each instance writes to unique directory

**Issue 2: Workflow name collisions**
- **Solution**: Include timestamp in auto-generated workflow names
- Format: `anycomfy_effect1_1703612400.json`

**Issue 3: Template workflows need to be accessible**
- **Solution**: Bundle template.json in plugin resources
- Automatically copied to `.ofx.bundle/Contents/Resources/workflows/`
- Users can also reference bundled templates via `resolveWorkflowPath()`

**Issue 4: ComfyUI doesn't support direct workflow URL loading**
- **Solution**: Open ComfyUI base URL, user manually loads workflow
- **Future Enhancement**: Upload workflow via API and open with workflow ID

**Issue 5: Users need to understand workflow requirements**
- **Solution**: Comprehensive README.md in resources explaining requirements
- Template workflow demonstrates minimal structure

**Issue 6: Cross-platform browser opening**
- **Solution**: Platform-specific implementations
  - macOS: `open` command
  - Linux: `xdg-open` command
  - Windows: `ShellExecuteA` API

## Implementation Details

### File Structure

```
contrib/plugins/ComfyUI/anycomfy/
├── anycomfy_plugin.h              # Plugin class declaration
├── anycomfy_plugin.cpp            # Plugin implementation
├── CMakeLists.txt                 # Build configuration
├── README.md                      # User documentation
└── resources/
    └── workflows/
        ├── README.md              # Workflow documentation
        └── template.json          # Default template workflow
```

### Class Hierarchy

```cpp
BasePlugin (abstract)
    │
    ├── Common functionality:
    │   - Server connection management
    │   - File I/O orchestration
    │   - Async job management
    │   - Caching optimization
    │   - Parameter management
    │
    └── AnyComfyPlugin (concrete)
        │
        ├── Workflow file selection
        ├── Template generation
        ├── Browser launching
        └── Generic workflow execution
```

### Key Methods

#### `buildWorkflow(int frame, const std::string& inputPath)`
```cpp
// Loads workflow from file (no hardcoded workflow)
// Calls BasePlugin::customizeWorkflow() for template substitution
// Returns customized workflow JSON
```

#### `createTemplateWorkflow()`
```cpp
// Generates minimal workflow:
//   Node 1: LoadEXR (${INPUT_PATH})
//   Node 2: SaveEXR (${OUTPUT_PREFIX}, ${FRAME})
// Saves to workflows directory
// Opens browser to ComfyUI
```

#### `openComfyUIInBrowser(const std::string& workflowPath)`
```cpp
// Platform-specific browser opening
// Constructs URL: http://<server>:<port>
// Launches default browser
```

#### `generateUniqueWorkflowName()`
```cpp
// Format: anycomfy_<instance>_<timestamp>.json
// Uses _instanceName from OFX
// Includes timestamp for uniqueness
```

### Parameters

#### Workflow-Specific Parameters (New)
1. **createNewWorkflow** (PushButton)
   - Creates template workflow
   - Opens ComfyUI in browser
   - Sets workflow path automatically

2. **workflowsDirectory** (String)
   - Directory for workflows (relative to shared mount)
   - Default: "workflows"

#### Inherited from BasePlugin
- All server configuration parameters
- All project organization parameters
- All processing/async parameters
- Workflow file path parameter

### Template Workflow

```json
{
  "_meta": {
    "description": "Template workflow created by AnyComfy OFX plugin",
    "note": "Add custom nodes between LoadEXR (1) and SaveEXR (2)"
  },
  "1": {
    "inputs": {
      "filepath": "${INPUT_PATH}",
      "linear_to_sRGB": false,
      ...
    },
    "class_type": "LoadEXR"
  },
  "2": {
    "inputs": {
      "filename_prefix": "${OUTPUT_PREFIX}",
      "start_frame": "${FRAME}",
      "frame_pad": 4,
      "images": ["1", 0]
    },
    "class_type": "SaveEXR"
  }
}
```

### Template Variable Substitution

Uses `BasePlugin::customizeWorkflow()` for automatic substitution:

| Variable | Description | Auto-Generated By |
|----------|-------------|-------------------|
| `${INPUT_PATH}` | Input EXR file path | `writeInputImage()` |
| `${OUTPUT_PREFIX}` | Output file prefix | `constructExpectedOutputPath()` |
| `${FRAME}` | Current frame number | Frame argument |
| `${LINEAR_TO_SRGB}` | Color conversion flag | Common parameters |
| `${SRGB_TO_LINEAR}` | Color conversion flag | Common parameters |

## Build Integration

### CMake Configuration

Added to [contrib/plugins/ComfyUI/CMakeLists.txt](CMakeLists.txt:58):

```cmake
add_subdirectory(anycomfy)
```

### Build Process

```bash
# Configure
cmake --preset conan-release -DBUILD_COMFYUI_PLUGINS=ON

# Build
cmake --build build/Release --config Release --target AnyComfy

# Result
build/Release/Release/AnyComfy.ofx.bundle/
├── Contents/
│   ├── Info.plist
│   ├── MacOS/
│   │   └── AnyComfy.ofx (2.4MB, arm64)
│   └── Resources/
│       └── workflows/
│           ├── README.md
│           └── template.json
```

### Build Verification

```bash
$ file build/Release/Release/AnyComfy.ofx.bundle/Contents/MacOS/AnyComfy.ofx
Mach-O 64-bit bundle arm64

$ cat build/Release/Release/AnyComfy.ofx.bundle/Contents/Info.plist | grep CFBundleIdentifier
com.comfyui.AnyComfy
```

## Usage Workflow

### Scenario 1: Quick Start with Template

1. **Add AnyComfy** node in Flame/Nuke
2. **Configure server** (address, port, shared mount)
3. **Click "New Workflow"** button
4. **Edit in ComfyUI** browser (opens automatically)
   - Add custom nodes between LoadEXR and SaveEXR
   - Configure parameters
   - Save workflow
5. **Render** in OFX host

### Scenario 2: Using Existing Workflow

1. **Place workflow** in `${SHARED_MOUNT}/workflows/my_workflow.json`
2. **Set "Workflow File Path"** to `workflows/my_workflow.json`
3. **Configure server/project** settings
4. **Render**

### Scenario 3: Multiple Instances

```
Timeline:
├── Clip 1 → AnyComfy (denoise_workflow.json)
├── Clip 2 → AnyComfy (upscale_workflow.json)
└── Clip 3 → AnyComfy (denoise_workflow.json)
```

Each instance:
- Has unique `_instanceName` (e.g., "effect1", "effect2", "effect3")
- Writes to unique output directory
- Maintains independent cache
- No interference with other instances

## Testing Strategy

### Unit Test Coverage Needed

1. **Workflow Loading**
   - Test loading workflow from file
   - Test template variable substitution
   - Test error handling for missing files

2. **Template Generation**
   - Test creating new workflow
   - Verify JSON structure
   - Check file permissions

3. **Instance Naming**
   - Test unique name generation
   - Verify timestamp uniqueness
   - Check name collision avoidance

4. **Browser Opening**
   - Test URL construction
   - Verify platform-specific commands
   - Handle browser launch failures

### Integration Testing

1. **End-to-End Workflow**
   - Create template → Edit in ComfyUI → Render in OFX
   - Verify EXR input/output
   - Check frame sequence handling

2. **Multiple Instances**
   - Create 3+ AnyComfy instances
   - Verify independent operation
   - Check output separation

3. **Error Recovery**
   - Missing workflow file
   - Invalid workflow JSON
   - ComfyUI server offline
   - Network errors

## Comparison: AnyComfy vs SAM

| Aspect | AnyComfy | SAM Segmentation |
|--------|----------|------------------|
| **Purpose** | Generic workflow executor | Specific segmentation task |
| **Workflow** | User-provided (any workflow) | Hardcoded SAM workflow |
| **Parameters** | Minimal (server/project only) | Many (model, threshold, prompt, etc.) |
| **Flexibility** | Maximum | Limited to SAM use case |
| **Configuration** | ComfyUI UI | OFX parameters |
| **Use Cases** | Unlimited | Text-based segmentation |
| **User Complexity** | Low (fewer parameters) | Medium (many parameters) |
| **Development Effort** | Medium (one-time) | High (per workflow type) |

## Advantages of AnyComfy Approach

### 1. **No Plugin Recompilation for New Workflows**
- SAM requires: New plugin → New parameters → Recompile → Redistribute
- AnyComfy requires: New workflow JSON file → Done

### 2. **Leverage ComfyUI's Strengths**
- ComfyUI has excellent workflow editor
- Visual node graph
- Parameter tweaking
- Real-time preview
- Why duplicate this in OFX?

### 3. **Separation of Concerns**
- **OFX**: File I/O, rendering pipeline, host integration
- **ComfyUI**: AI models, processing logic, parameter configuration
- Each tool does what it's best at

### 4. **Easier Maintenance**
- Workflow changes don't require plugin updates
- Users can create/modify workflows themselves
- Less C++ code to maintain

### 5. **Scalability**
- One plugin supports unlimited workflows
- vs. One plugin per workflow type (SAM, upscale, denoise, etc.)

## Potential Issues and Mitigations

### Issue: ComfyUI Workflow Format Changes

**Risk**: ComfyUI updates workflow JSON format

**Mitigation**:
- Plugin only requires LoadEXR/SaveEXR nodes
- Template substitution is simple string replacement
- Minimal coupling to workflow structure
- Users responsible for workflow compatibility

### Issue: Workflow Discovery

**Risk**: Users don't know where to find/create workflows

**Mitigation**:
- Comprehensive README in resources
- "New Workflow" button creates starting point
- Template demonstrates minimal requirements
- Browser auto-opens for editing

### Issue: Missing Models on Server

**Risk**: Workflow requires models not installed on server

**Mitigation**:
- Current: ComfyUI server returns error (logged by plugin)
- **Future**: Parse workflow, detect required models, warn user upfront

### Issue: Invalid Workflow JSON

**Risk**: User provides malformed JSON or missing required nodes

**Mitigation**:
- Current: JSON parsing catches syntax errors
- **Future**: Validate workflow has LoadEXR and SaveEXR before execution

### Issue: Browser Doesn't Open

**Risk**: Platform-specific browser launch fails

**Mitigation**:
- Log the workflow path for manual opening
- Document manual workflow loading process
- Graceful fallback if browser launch fails

## Future Enhancements

### 1. Workflow Upload via API (High Priority)

**Current**: Browser opens to ComfyUI home, user manually loads workflow

**Enhanced**:
```cpp
void openComfyUIInBrowser(const std::string& workflowPath) {
    // 1. Read workflow JSON
    json workflow = loadWorkflowFromFile(workflowPath);

    // 2. Upload via API
    std::string response = _comfyClient->uploadWorkflow(workflow);
    json responseJson = json::parse(response);
    std::string workflowId = responseJson["workflow_id"];

    // 3. Open browser with workflow ID
    std::string url = "http://" + server + ":" + port + "/?workflow=" + workflowId;
    openBrowser(url);
}
```

**Benefit**: Workflow automatically loads in ComfyUI UI

### 2. Workflow Validation

**Check**:
- Workflow has LoadEXR node
- Workflow has SaveEXR node
- Template variables are used correctly
- JSON is valid

**Display**: Warning message if validation fails

### 3. Model Dependency Detection

**Parse** workflow to find model loaders:
```json
"18": {
  "inputs": {"model_name": "sam_vit_h (2.56GB)"},
  "class_type": "SAMModelLoader"
}
```

**Query** ComfyUI server for available models

**Warn** if required models are missing

### 4. Workflow Library Browser

**UI Enhancement**:
- Browse workflows in OFX UI
- Show workflow descriptions (from `_meta`)
- Preview workflow graph
- One-click selection

### 5. Workflow Versioning

**Track**:
- Workflow modifications
- Version history
- Rollback capability
- Diff changes

## Documentation

### Created Documents

1. **[anycomfy/README.md](anycomfy/README.md)** - Comprehensive user documentation
   - Architecture overview
   - Usage workflow
   - Parameter reference
   - Troubleshooting guide
   - Examples

2. **[anycomfy/resources/workflows/README.md](anycomfy/resources/workflows/README.md)** - Workflow documentation
   - Workflow requirements
   - Template variable reference
   - Creating workflows guide
   - Organization recommendations
   - Best practices

3. **[ANYCOMFY_IMPLEMENTATION.md](ANYCOMFY_IMPLEMENTATION.md)** (this document)
   - Implementation summary
   - Design decisions
   - Build integration
   - Testing strategy

## Conclusion

The AnyComfy plugin successfully addresses all requirements:

✅ **Generic plugin** that works with any ComfyUI workflow
✅ **Minimal UI** with only server/project/workflow parameters
✅ **Workflow file selection** from shared server directory
✅ **"New Workflow" button** creates template and opens browser
✅ **Unique naming** prevents instance collisions
✅ **Anticipated issues** (naming, browser opening, multiple instances) solved
✅ **Complete documentation** for users and developers
✅ **Successful build** (2.4MB arm64 bundle with resources)
✅ **Future-proof design** enables enhancements without breaking changes

### Key Innovation

**Separation of Concerns**: OFX handles rendering pipeline, ComfyUI handles AI processing logic. Users configure workflows in ComfyUI's powerful visual editor instead of through limited OFX parameters.

This approach provides maximum flexibility while minimizing plugin complexity and maintenance burden.

## Build and Installation

### Building AnyComfy

```bash
# Configure with ComfyUI plugins enabled
cmake --preset conan-release -DBUILD_COMFYUI_PLUGINS=ON

# Build AnyComfy target
cmake --build build/Release --config Release --target AnyComfy --parallel

# Bundle location
ls -lh build/Release/Release/AnyComfy.ofx.bundle/Contents/MacOS/AnyComfy.ofx
```

### Installation

```bash
# macOS
cp -r build/Release/Release/AnyComfy.ofx.bundle ~/Library/OFX/Plugins/

# Linux
cp -r build/Release/AnyComfy.ofx.bundle /usr/OFX/Plugins/

# Verify installation
ls -lh ~/Library/OFX/Plugins/AnyComfy.ofx.bundle/
```

### Testing Installation

1. **Launch OFX host** (Flame, Nuke, Resolve, etc.)
2. **Look for "AnyComfy"** in ComfyUI plugin category
3. **Add to timeline**
4. **Verify parameters** appear in UI
5. **Click "New Workflow"** to test template generation

## Next Steps

### Recommended Testing

1. **Create test workflow** with template
2. **Test multiple instances** in timeline
3. **Verify output separation**
4. **Test error handling** (missing server, invalid workflow)
5. **Performance testing** with large frame ranges

### Recommended Enhancements

1. Implement workflow upload via API
2. Add workflow validation
3. Add model dependency detection
4. Create example workflow library
5. Add unit tests

## Contact and Support

For issues, questions, or contributions:
- **GitHub**: https://github.com/AcademySoftwareFoundation/openfx
- **OpenFX Group**: https://groups.google.com/g/openeffects-dev

---

**Implementation Date**: December 26, 2025
**Plugin Version**: 1.0.0
**Build System**: CMake + Conan 2.x
**Platforms**: macOS (arm64), Linux (x86_64, arm64), Windows (planned)

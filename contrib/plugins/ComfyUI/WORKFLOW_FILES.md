# ComfyUI Workflow File Support

This document describes the workflow file resource system for ComfyUI OFX plugins.

## Overview

ComfyUI plugins now support loading workflows from external JSON files, with clean integration into the OFX bundle structure. This provides several benefits:

1. **Separation of Concerns**: Workflow logic is separated from plugin code
2. **Easy Customization**: Users can modify workflows without recompiling
3. **Version Control**: Workflow JSON files can be tracked separately
4. **Fallback Support**: Plugins maintain hardcoded workflows as fallback

## Architecture

### Base Plugin Features

The `BasePlugin` class provides workflow file management:

- **`getBundleResourcePath()`** - Locates resources within the plugin bundle
- **`resolveWorkflowPath()`** - Resolves bundle-relative or absolute paths
- **`loadWorkflowFromFile()`** - Loads and parses JSON workflow files
- **`customizeWorkflow()`** - Replaces common placeholders (paths, frame number)

### Plugin Implementation

Derived plugins (e.g., `SAMSegmentationPlugin`) implement:

1. **`buildWorkflow()`** - Tries file loading first, falls back to hardcoded
2. **`buildHardcodedWorkflow()`** - Original hardcoded workflow logic
3. **`customizeWorkflowWithParams()`** - Plugin-specific parameter replacement

## Bundle Structure

```
Plugin.ofx.bundle/
├── Contents/
│   ├── Info.plist
│   ├── MacOS/
│   │   └── Plugin.ofx
│   └── Resources/              ← Resources directory
│       └── workflows/
│           ├── README.md
│           └── workflow.json   ← Workflow templates
```

On Linux:
```
Plugin.ofx.bundle/
├── Contents/
│   └── Linux-{arch}/
│       └── Plugin.ofx
└── Resources/                  ← At bundle root
    └── workflows/
        └── workflow.json
```

## Placeholder System

### Common Placeholders (BasePlugin)

- `${INPUT_PATH}` - Input EXR file path (converted to server format)
- `${OUTPUT_PREFIX}` - Output file prefix (converted to server format)
- `${FRAME}` - Current frame number

### Plugin-Specific Placeholders

Each plugin can define additional placeholders. For SAM Segmentation:

- `${PROMPT}` - Segmentation text prompt
- `${THRESHOLD}` - Detection threshold (0.0-1.0)
- `${RESOLUTION}` - Preprocessing resolution
- `${SAM_MODEL}` - SAM model name
- `${DINO_MODEL}` - Grounding DINO model name
- `${LINEAR_TO_SRGB}` - Color space flag ("true"/"false")
- `${SRGB_TO_LINEAR}` - Color space flag ("true"/"false")

## Usage Examples

### Using Bundled Workflows

Set the "Workflow File" parameter to:
```
resources/workflows/sam_segmentation.json
```

The plugin automatically resolves this to the bundle resource.

### Using Custom Workflows

1. Create your workflow JSON with placeholders
2. Save it anywhere on disk
3. Set "Workflow File" parameter to absolute path:
```
/Users/myuser/custom_workflows/my_workflow.json
```

### Fallback Behavior

Leave "Workflow File" empty to use the hardcoded workflow.

## Template Workflow Example

```json
{
  "_comment": "This is a ComfyUI workflow template",
  "_placeholders": {
    "INPUT_PATH": "Input file path",
    "OUTPUT_PREFIX": "Output file prefix",
    "FRAME": "Frame number",
    "PROMPT": "Segmentation prompt"
  },

  "1": {
    "inputs": {
      "filepath": "${INPUT_PATH}",
      "linear_to_sRGB": "${LINEAR_TO_SRGB}"
    },
    "class_type": "LoadEXR"
  },

  "2": {
    "inputs": {
      "prompt": "${PROMPT}",
      "threshold": ${THRESHOLD},
      "image": ["1", 0]
    },
    "class_type": "GroundingDinoSAMSegment (segment anything)"
  },

  "3": {
    "inputs": {
      "filename_prefix": "${OUTPUT_PREFIX}",
      "start_frame": ${FRAME},
      "images": ["2", 0]
    },
    "class_type": "SaveEXR"
  }
}
```

## CMake Integration

The CMakeLists.txt automatically copies resources into the bundle:

```cmake
if(APPLE)
    set(RESOURCES_DIR "${CMAKE_BINARY_DIR}/${CMAKE_BUILD_TYPE}/Plugin.ofx.bundle/Contents/Resources")

    add_custom_command(TARGET Plugin POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${RESOURCES_DIR}"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/resources"
            "${RESOURCES_DIR}"
        COMMENT "Bundling resources"
    )
endif()
```

## Creating New Plugins with Workflow Files

1. Create `resources/workflows/` directory in plugin folder
2. Add workflow JSON template with placeholders
3. Update CMakeLists.txt to copy resources (see above)
4. Implement `customizeWorkflowWithParams()` for plugin-specific placeholders
5. Call base class `customizeWorkflow()` first for common placeholders

## Platform Support

- ✅ **macOS**: Full support with CoreFoundation bundle API
- ✅ **Linux**: Full support with string-based path resolution
- ⚠️ **Windows**: Not yet implemented (uses same logic as Linux)

## Best Practices

1. **Always provide fallback**: Keep hardcoded workflow for robustness
2. **Document placeholders**: Add `_placeholders` section to JSON for clarity
3. **Validate workflow**: Test both bundled and custom workflow paths
4. **Handle errors gracefully**: Log failures and fall back to hardcoded workflow
5. **Use relative paths**: Default to bundle resources, allow absolute overrides

## Logging

All workflow operations are logged:

```
INFO: Resolving workflow path: resources/workflows/sam_segmentation.json
DEBUG: Plugin file path: /Library/OFX/Plugins/SAMSegmentation.ofx.bundle/Contents/MacOS/SAMSegmentation.ofx
DEBUG: Resolved bundle resource path: /Library/OFX/Plugins/SAMSegmentation.ofx.bundle/Contents/Resources/workflows/sam_segmentation.json
INFO: Loading workflow from file: /Library/OFX/Plugins/SAMSegmentation.ofx.bundle/Contents/Resources/workflows/sam_segmentation.json
INFO: Workflow loaded successfully: 2847 bytes
DEBUG: Customizing workflow for frame 1
DEBUG: SAM parameter customization complete
INFO: Successfully loaded and customized workflow from file
```

## Future Enhancements

Potential improvements:

- **Workflow validation**: Schema validation for loaded JSON
- **UI file browser**: Native file picker for workflow selection
- **Hot reload**: Detect workflow file changes and reload
- **Workflow library**: Built-in collection of common workflows
- **Visual editor**: Generate workflow JSON from GUI

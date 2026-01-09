# AnyComfy Workflows

This directory contains workflow templates and examples for the AnyComfy OFX plugin.

## Overview

AnyComfy is a generic ComfyUI OFX plugin that can execute any ComfyUI workflow, as long as the workflow implements:
- **LoadEXR** node (for input)
- **SaveEXR** node (for output)

Unlike specific plugins (like SAM), AnyComfy has no workflow-specific parameters. All workflow configuration is done in the ComfyUI UI.

## Workflow Requirements

### Required Nodes

1. **LoadEXR** (usually node "1")
   - Must use `${INPUT_PATH}` as the filepath
   - Receives the input image from OFX

2. **SaveEXR** (usually last node)
   - Must use `${OUTPUT_PREFIX}` as the filename_prefix
   - Must use `${FRAME}` as the start_frame
   - Outputs the result back to OFX

### Template Variables

The plugin automatically substitutes these variables in your workflow:

| Variable | Description | Example |
|----------|-------------|---------|
| `${INPUT_PATH}` | Full path to input EXR file | `/mnt/share/project1/inputs/frame_0001.exr` |
| `${OUTPUT_PREFIX}` | Output file prefix (path + basename) | `/mnt/share/project1/outputs/anycomfy_instance1` |
| `${FRAME}` | Current frame number | `1` |
| `${LINEAR_TO_SRGB}` | Linear to sRGB conversion flag | `false` |
| `${SRGB_TO_LINEAR}` | sRGB to Linear conversion flag | `false` |

You can also use project-specific variables if needed (these are set in the Project page of the plugin):
- `${PROJECT_NAME}` - Project name
- `${WORKFLOW_NAME}` - Workflow subdirectory name
- `${OUTPUT_VERSION}` - Output version string

## Creating a New Workflow

### Method 1: Using the "New Workflow" Button

1. In your OFX host (Flame, Nuke, etc.), add an AnyComfy node
2. Click the **"New Workflow"** button in the Workflow page
3. The plugin will:
   - Create a template workflow with LoadEXR and SaveEXR nodes
   - Save it to the workflows directory on the shared server
   - Open ComfyUI in your browser
4. In ComfyUI:
   - Load the newly created workflow (check the console log for the filename)
   - Add your custom nodes between LoadEXR and SaveEXR
   - Make sure to maintain the image processing chain
   - Save the workflow
5. The workflow is now ready to use in the AnyComfy plugin

### Method 2: Manual Creation

1. Copy `template.json` to a new file
2. Add your custom nodes between LoadEXR (node 1) and SaveEXR (node 2)
3. Maintain the image chain (each node should reference the previous node's output)
4. Save the workflow with a descriptive name
5. Place it in the workflows directory on the shared server

## Example Workflow Structure

```json
{
  "_meta": {
    "description": "Example workflow with custom processing",
    "note": "This workflow applies blur between input and output"
  },

  "1": {
    "inputs": {
      "filepath": "${INPUT_PATH}",
      ...
    },
    "class_type": "LoadEXR"
  },

  "10": {
    "inputs": {
      "blur_radius": 5.0,
      "image": ["1", 0]
    },
    "class_type": "GaussianBlur"
  },

  "20": {
    "inputs": {
      "filename_prefix": "${OUTPUT_PREFIX}",
      "start_frame": "${FRAME}",
      "images": ["10", 0]
    },
    "class_type": "SaveEXR"
  }
}
```

## Workflow Organization

We recommend organizing workflows by purpose:

```
workflows/
  ├── denoise/
  │   ├── denoise_light.json
  │   ├── denoise_medium.json
  │   └── denoise_heavy.json
  ├── upscale/
  │   ├── upscale_2x.json
  │   └── upscale_4x.json
  ├── color/
  │   ├── color_grade_warm.json
  │   └── color_grade_cool.json
  └── template.json
```

## Instance Naming

Each AnyComfy OFX instance automatically gets a unique name based on:
- The OFX instance identifier
- The current timestamp (when creating new workflows)

This ensures that multiple AnyComfy instances don't conflict with each other's workflows or output files.

## Troubleshooting

### Workflow Not Found
- Check that the workflow file is in the correct directory
- Verify the shared mount path is configured correctly
- Check file permissions on the shared server

### Workflow Fails to Execute
- Verify the workflow has both LoadEXR and SaveEXR nodes
- Check that template variables are used correctly (${INPUT_PATH}, ${OUTPUT_PREFIX}, ${FRAME})
- Review ComfyUI server logs for errors
- Test the workflow in ComfyUI UI first before using in OFX

### Output Not Appearing
- Check the SaveEXR node configuration
- Verify output path permissions
- Check ComfyUI server can write to the output directory
- Review plugin logs for errors

## Best Practices

1. **Always test workflows in ComfyUI first** before using them in OFX
2. **Use descriptive workflow names** (e.g., `denoise_temporal_v2.json` instead of `workflow1.json`)
3. **Document your workflows** using the `_meta` field
4. **Keep template variables** - Don't replace ${INPUT_PATH}, ${OUTPUT_PREFIX}, etc. with hardcoded values
5. **Maintain the image chain** - Every node should connect to the previous one
6. **Version your workflows** - Use v1, v2, etc. in filenames when iterating

## Template Workflow

The `template.json` file in this directory is the default template used by the "New Workflow" button. It contains:
- LoadEXR node (node 1) with input path placeholder
- SaveEXR node (node 2) linked to LoadEXR output
- Metadata documenting the template

This is the minimal starting point for any AnyComfy workflow.

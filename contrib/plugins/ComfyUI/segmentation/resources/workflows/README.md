# ComfyUI Workflow Resources

This directory contains workflow JSON templates for the ComfyUI OFX plugins.

## Workflow Template Format

Workflow files are standard ComfyUI workflow JSON files with placeholder support for dynamic values.

### Common Placeholders

These placeholders are replaced by the base plugin:

- `${INPUT_PATH}` - Input EXR file path (converted to ComfyUI server format)
- `${OUTPUT_PREFIX}` - Output file prefix path (converted to ComfyUI server format)
- `${FRAME}` - Current frame number

### SAM Segmentation Placeholders

These placeholders are replaced by the SAM segmentation plugin:

- `${PROMPT}` - Segmentation text prompt (e.g., "foreground", "person")
- `${THRESHOLD}` - Detection confidence threshold (0.0-1.0)
- `${RESOLUTION}` - Preprocessing resolution (longer side in pixels)
- `${SAM_MODEL}` - SAM model name (e.g., "sam_vit_h (2.56GB)")
- `${DINO_MODEL}` - Grounding DINO model name (e.g., "GroundingDINO_SwinT_OGC (694MB)")
- `${LINEAR_TO_SRGB}` - Color space conversion flag ("true" or "false")
- `${SRGB_TO_LINEAR}` - Color space conversion flag ("true" or "false")

## Usage

### Using Bundled Workflows

In the plugin UI, set the "Workflow File" parameter to:
```
resources/workflows/sam_segmentation.json
```

The plugin will automatically locate this file within the plugin bundle.

### Using Custom Workflows

1. Create your workflow JSON file with appropriate placeholders
2. Save it anywhere on your filesystem
3. Set the "Workflow File" parameter to the absolute path:
```
/path/to/your/custom_workflow.json
```

### Fallback to Hardcoded Workflow

If you leave the "Workflow File" parameter empty, the plugin will use its hardcoded workflow as a fallback.

## Creating Custom Workflows

1. Export your workflow from ComfyUI as JSON
2. Replace dynamic values with placeholders (e.g., replace hardcoded paths with `${INPUT_PATH}`)
3. Ensure your workflow has:
   - A LoadEXR node that reads from `${INPUT_PATH}`
   - A SaveEXR node that writes to `${OUTPUT_PREFIX}`
   - Proper frame numbering in SaveEXR using `${FRAME}`

## Example: Minimal Workflow

```json
{
  "1": {
    "inputs": {
      "filepath": "${INPUT_PATH}"
    },
    "class_type": "LoadEXR"
  },
  "2": {
    "inputs": {
      "filename_prefix": "${OUTPUT_PREFIX}",
      "start_frame": ${FRAME},
      "images": ["1", 0]
    },
    "class_type": "SaveEXR"
  }
}
```

## Notes

- Placeholders are case-sensitive
- Numeric placeholders (like `${THRESHOLD}`, `${RESOLUTION}`, `${FRAME}`) should not be quoted in JSON
- String placeholders (like `${INPUT_PATH}`, `${PROMPT}`) should be quoted in JSON
- Boolean placeholders (like `${LINEAR_TO_SRGB}`) are replaced with literal "true" or "false" strings

# ComfyUI Workflow Format Guide

## Overview

ComfyUI uses two different workflow formats:

1. **UI Format** - For editing workflows in the browser
2. **API Format** - For executing workflows via the API

The AnyComfy plugin handles both formats automatically.

## Format Differences

### UI Format (for editing)
```json
{
  "last_node_id": 11,
  "last_link_id": 9,
  "nodes": [
    {
      "id": 10,
      "type": "LoadEXR",
      "pos": [557, 570],
      "widgets_values": ["${INPUT_PATH}", true, 0, 0, 1],
      ...
    }
  ],
  "links": [[9, 10, 0, 11, 0, "IMAGE"]],
  ...
}
```

**Contains:**
- `nodes` array with full node data (position, size, etc.)
- `links` array defining connections
- Canvas metadata (position, zoom, etc.)
- ~3x larger file size

**Used for:**
- Loading workflows in ComfyUI web interface
- Editing and visual design
- Browser-based workflow development

### API Format (for execution)
```json
{
  "10": {
    "class_type": "LoadEXR",
    "inputs": {
      "filepath": "${INPUT_PATH}",
      "linear_to_sRGB": true,
      "image_load_cap": 0,
      ...
    }
  },
  "11": {
    "class_type": "SaveEXR",
    "inputs": {
      "filename_prefix": "${OUTPUT_PREFIX}",
      "images": ["10", 0],  // Connection to node 10, output 0
      ...
    }
  }
}
```

**Contains:**
- Node IDs as keys
- Only execution-relevant data (class_type, inputs)
- Connection info embedded in inputs
- Minimal, execution-focused

**Used for:**
- Submitting workflows to `/prompt` API endpoint
- Plugin processing and rendering
- Automated workflow execution

## How AnyComfy Handles Both Formats

### 1. Template Creation (UI Format)

When you click "New Workflow" in Flame:

```cpp
// Plugin creates UI format template
createTemplateWorkflow() {
    json workflow = load("workflows/template.json");  // UI format
    save_to_input_dir(workflow);  // For browser editing
    open_browser("?load_local_json=test.json");
}
```

**Why UI format?**
- Users need to see and edit the workflow visually
- Browser-based editing requires full UI metadata

### 2. Workflow Editing & Auto-Save (Both Formats)

The `ofx_autosaver.js` extension automatically saves in both formats:

```javascript
app.saveWorkflow = async function() {
    // Save UI format for future editing
    saveToInputDir("test.json", workflowUI);

    // Save API format for execution
    saveToInputDir("test_api.json", workflowAPI);
}
```

**Result:**
- `test.json` → UI format (for editing)
- `test_api.json` → API format (for execution)

### 3. Workflow Execution (API Format)

When the plugin processes a frame:

```cpp
json AnyComfyPlugin::loadWorkflow(int frame) {
    // OPTIMIZATION: Try API format first (no conversion needed!)
    if (exists("test_api.json")) {
        workflow = loadWorkflowFromFile("test_api.json");
        // Already in API format - use directly!
    } else {
        // Fall back to UI format (requires conversion)
        workflow = loadWorkflowFromFile("test.json");
        workflow = convertUIFormatToAPI(workflow);
    }

    // Customize with frame-specific paths
    workflow = customizeWorkflow(workflow, frame, inputPath);

    // Now in API format - ready for execution!
    return workflow;
}
```

**Why this is efficient:**
- **With auto-save**: Uses `test_api.json` directly (99% of cases)
- **No conversion overhead**: API format loads instantly
- **Fallback works**: Template workflows still convert UI → API
- **Best of both worlds**: Fast when possible, compatible always

## Workflow Lifecycle

```
1. CREATE TEMPLATE (Flame)
   ↓
   Plugin creates UI format workflow
   Saves to: /input/test.json

2. EDIT IN BROWSER
   ↓
   User modifies workflow in ComfyUI
   Auto-save extension saves both formats:
     - /input/test.json (UI format)
     - /input/test_api.json (API format)

3. RENDER IN FLAME
   ↓
   Plugin loads workflow
   - Prefers test_api.json if exists (faster)
   - Falls back to test.json and converts
   Submits API format to ComfyUI

4. EXECUTE ON SERVER
   ↓
   ComfyUI receives API format
   Processes frame
   Returns result
```

## Installation

### 1. JavaScript Extensions

Copy to **ComfyUI server** at `C:\Users\...\ComfyUI\web\extensions\`:

```bash
# Auto-loader (for opening workflows from Flame)
cp ofx_autoloader.js  C:\Users\...\ComfyUI\web\extensions\ofx\

# Auto-save (for saving both formats)
cp ofx_autosaver.js    C:\Users\...\ComfyUI\web\extensions\ofx\
```

### 2. Restart ComfyUI Server

The extensions load at server startup.

## Best Practices

1. **Let auto-save handle formats** - Don't manually manage UI vs API files
2. **Use UI format for templates** - Easier for users to understand and edit
3. **Plugin handles conversion** - No manual intervention needed
4. **Delete old workflows** - Clean up test workflows regularly

## Troubleshooting

### Workflow fails to load in browser
- Check browser console for errors
- Verify `test.json` exists in input directory
- Ensure file is valid JSON (use `python3 -m json.tool test.json`)

### Workflow fails to execute
- Check if API format exists (`test_api.json`)
- Plugin logs will show "Converting UI format to API format"
- Verify node connections are preserved

### Auto-save not working
- Check browser console for `[OFX AutoSave]` messages
- Verify `ofx_autosaver.js` is in ComfyUI extensions directory
- Restart ComfyUI server

## Technical Details

### UI → API Conversion

The plugin's converter:
1. Iterates through `nodes` array
2. Extracts node ID, type, and widget values
3. Maps widget values to input parameter names
4. Resolves connections from `links` array
5. Builds API format with node IDs as keys

### Supported Nodes

Currently optimized for:
- LoadEXR
- SaveEXR
- Standard ComfyUI nodes (basic conversion)

Custom nodes may require manual format handling.

## See Also

- [Auto-Loading Guide](INSTALL_AUTO_LOAD.md)
- [Plugin README](README.md)
- [ComfyUI API Documentation](https://github.com/comfyanonymous/ComfyUI/wiki/API)

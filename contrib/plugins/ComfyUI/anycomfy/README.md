# AnyComfy - Generic ComfyUI Workflow Executor OFX Plugin

## Overview

AnyComfy is a **generic** OpenFX plugin that can execute any ComfyUI workflow, unlike specific plugins (like SAM) that are hardcoded for particular AI models. This plugin provides maximum flexibility by allowing users to:

- Execute any ComfyUI workflow that uses EXR input/output
- Create new workflows directly from the OFX interface
- Manage workflows without recompiling the plugin
- Use the same plugin instance for different workflows

## Key Features

### 1. **Universal Workflow Support**
- Works with **any** ComfyUI workflow (no hardcoded models or parameters)
- Only requires LoadEXR and SaveEXR nodes in the workflow
- All workflow logic is configured in ComfyUI UI, not in OFX parameters

### 2. **Workflow Management**
- **Browse and select** workflows from shared server directory
- **Create new workflows** with template generator
- **Automatic browser opening** to ComfyUI for editing workflows
- **Unique naming** to avoid collisions between multiple instances

### 3. **Zero Workflow Assumptions**
- No AI model parameters (SAM, DINO, etc.)
- No processing parameters (threshold, resolution, etc.)
- Complete freedom to configure workflows in ComfyUI

## Architecture

```
AnyComfy Plugin
│
├── Workflow File Selector ──> Select .json workflow from shared server
│
├── New Workflow Button ──────> Create template + Open ComfyUI in browser
│
├── BasePlugin Integration ───> Common server/project/caching functionality
│
└── Generic Workflow Execution > Load workflow, substitute paths, execute
```

### Inheritance Hierarchy

```
OFX::ImageEffect
    │
    ├── BasePlugin (common ComfyUI functionality)
    │       │
    │       ├── Server connection management
    │       ├── File I/O orchestration
    │       ├── Async job management
    │       ├── Caching optimization
    │       └── Parameter management
    │
    └── AnyComfyPlugin (generic workflow executor)
            │
            ├── Workflow file selection
            ├── Template workflow generation
            ├── Browser launching
            └── Zero workflow assumptions
```

## Workflow Requirements

For a workflow to be compatible with AnyComfy, it must:

### 1. **LoadEXR Node** (Input)
```json
"1": {
  "inputs": {
    "filepath": "${INPUT_PATH}",
    "linear_to_sRGB": false,
    ...
  },
  "class_type": "LoadEXR"
}
```

### 2. **SaveEXR Node** (Output)
```json
"N": {
  "inputs": {
    "filename_prefix": "${OUTPUT_PREFIX}",
    "start_frame": "${FRAME}",
    "frame_pad": 4,
    "images": ["...", 0]
  },
  "class_type": "SaveEXR"
}
```

### Template Variable Substitution

The plugin automatically replaces these variables in your workflow:

| Variable | Description | Example |
|----------|-------------|---------|
| `${INPUT_PATH}` | Full path to input EXR | `/mnt/share/project1/inputs/frame_0001.exr` |
| `${OUTPUT_PREFIX}` | Output file prefix | `/mnt/share/project1/outputs/anycomfy_instance1` |
| `${FRAME}` | Current frame number | `1` |
| `${LINEAR_TO_SRGB}` | Color space conversion flag | `false` |
| `${SRGB_TO_LINEAR}` | Color space conversion flag | `false` |

## Usage Workflow

### Creating a New Workflow

1. **Add AnyComfy node** to your OFX host (Flame, Nuke, etc.)
2. **Configure server settings** in the "Server" page
   - Server Address: ComfyUI server IP/hostname
   - Server Port: ComfyUI port (default: 8188)
   - ComfyUI Input Directory: Path to ComfyUI's input folder (e.g., `/Volumes/silo2/002_COMFYUI/in`)
3. **Enter workflow name** in "New Workflow Name" field (optional)
   - Example: `normal_map_deepbump`, `upscale_esrgan`, `denoise_rife`
   - If empty, a unique name is auto-generated
4. **Click "New Workflow" button** in the "Workflow" page
   - Creates workflow directory: `<COMFYUI_INPUT_PATH>/workflows/<WORKFLOW_NAME>/`
   - Creates template file: `<WORKFLOW_NAME>.json`
   - Opens ComfyUI in your default browser
5. **Edit workflow in ComfyUI**
   - Add your custom nodes between LoadEXR and SaveEXR
   - Configure node parameters
   - Save the workflow (creates both `.json` and `_api.json` versions)
6. **Render in OFX**
   - The workflow name is automatically set
   - Start rendering to execute the workflow

### Using an Existing Workflow

1. **Create workflow directory** in `<COMFYUI_INPUT_PATH>/workflows/<WORKFLOW_NAME>/`
2. **Place workflow file** named `<WORKFLOW_NAME>.json` in that directory
   - Optionally add `<WORKFLOW_NAME>_api.json` for faster execution
3. **In AnyComfy plugin**, set "Workflow File Path" to the workflow name
   - Just use the name (e.g., `normal_map_deepbump`), not the full path
4. **Configure server/project settings** as needed
5. **Render** to execute the workflow

## Parameters

### Workflow Page

- **New Workflow** (button)
  - Creates template workflow and opens ComfyUI in browser

- **Workflows Directory** (string)
  - Directory containing workflow files (relative to shared mount)
  - Default: `workflows`

### Project Page (from BasePlugin)

- **Project Name** - Organizes files by project
- **Workflow Name** - Subdirectory for workflow outputs
- **Output Version** - Version string for outputs
- **Workflow File Path** - Path to workflow JSON file

### Processing Page (from BasePlugin)

- **Enable Processing** - Master on/off switch
- **Async Mode** - Blocking vs non-blocking rendering
- **Placeholder Mode** - What to show while processing
- **Enable Cache** - Cache optimization
- **Timeout** - Request timeout in seconds

### Server Page (from BasePlugin)

- **Server Address** - ComfyUI server hostname/IP
- **Server Port** - ComfyUI server port
- **Shared Mount Path** - Client-side shared storage path
- **Server Mount Point** - Server-side shared storage path

## Example Workflows

### 1. Simple Passthrough
```json
{
  "1": {
    "inputs": {"filepath": "${INPUT_PATH}", ...},
    "class_type": "LoadEXR"
  },
  "2": {
    "inputs": {
      "filename_prefix": "${OUTPUT_PREFIX}",
      "start_frame": "${FRAME}",
      "images": ["1", 0]
    },
    "class_type": "SaveEXR"
  }
}
```

### 2. Denoise Workflow
```json
{
  "1": {
    "inputs": {"filepath": "${INPUT_PATH}", ...},
    "class_type": "LoadEXR"
  },
  "10": {
    "inputs": {
      "denoise_strength": 0.5,
      "image": ["1", 0]
    },
    "class_type": "AIDenoiseNode"
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

## Instance Naming and Collision Avoidance

### Automatic Unique Naming

Each AnyComfy instance gets a unique identifier from OFX, which is used to:

1. **Generate unique workflow filenames**
   - Format: `anycomfy_<instance>_<timestamp>.json`
   - Example: `anycomfy_effect1_1703612400.json`

2. **Separate output files**
   - Each instance writes to its own output directory
   - Prevents multiple instances from overwriting each other's results

3. **Independent caching**
   - Each instance maintains its own cache
   - No interference between instances

### Multiple Instances Support

You can safely use multiple AnyComfy instances in the same project:

```
Timeline:
├── Clip 1 → AnyComfy (instance: "denoise_1") → Denoise workflow
├── Clip 2 → AnyComfy (instance: "upscale_1") → Upscale workflow
└── Clip 3 → AnyComfy (instance: "denoise_2") → Same denoise workflow
```

Each instance maintains independent state and won't interfere with others.

## Browser Opening for Workflow Editing

### Automatic Workflow Loading (v1.2+)

**NEW**: The plugin now supports automatic workflow loading! When you click "New Workflow":

1. **Creates template workflow** on shared server
2. **Copies workflow** to ComfyUI's input directory
3. **Opens ComfyUI in browser** with URL parameter: `?load_local_json=<filename>`
4. **OFX.AutoLoader extension** detects the parameter and loads the workflow automatically
5. **Workflow is ready to edit** - no manual loading required!

**Setup Required**:

- Install `ofx_autoloader.js` extension in ComfyUI (one-time setup)
- Configure "ComfyUI Input Directory" parameter in plugin
- **📖 Complete Guide**: [ComfyUI Workflow Auto-Loading](../../../docs/COMFYUI_WORKFLOW_AUTO_LOADING_COMPLETE_GUIDE.md)
- Quick Install: [INSTALL_AUTO_LOAD.md](INSTALL_AUTO_LOAD.md)

### Manual Workflow Loading (Fallback)

If auto-loading is not configured or the extension is not installed:

1. Open ComfyUI manually in browser
2. Navigate to File → Load (or press Ctrl+O)
3. Browse to the workflow file path (shown in plugin logs)
4. Edit and save the workflow

**Browser Support** (both modes):

- macOS: Uses `open` command
- Linux: Uses `xdg-open` command
- Windows: Uses `ShellExecuteA` API

## File Organization

### Workflow Directory Structure (v1.3+)

Workflows are organized in subdirectories where **the directory name matches the workflow name**:

```
<COMFYUI_INPUT_PATH>/workflows/
├── <WORKFLOW_NAME>/
│   ├── <WORKFLOW_NAME>.json         # UI format (for editing in ComfyUI)
│   └── <WORKFLOW_NAME>_api.json     # API format (preferred for execution)
└── ...
```

**Example with actual workflows:**

```
/Volumes/silo2/002_COMFYUI/in/workflows/
├── normal_map_deepbump/
│   ├── normal_map_deepbump.json       # Edit this in ComfyUI
│   └── normal_map_deepbump_api.json   # Auto-used for execution
├── upscale_esrgan/
│   ├── upscale_esrgan.json
│   └── upscale_esrgan_api.json
└── denoise_rife/
    └── denoise_rife.json              # API format optional
```

**Key points:**
- **Directory name = Workflow name**: The subdirectory must have the same name as the workflow
- **API format preferred**: If `<name>_api.json` exists, it's used for execution (no conversion needed)
- **UI format converted**: If only `<name>.json` exists, it's converted to API format at runtime

### Complete Project Structure

```
/mnt/shared/                           # Shared mount path
├── <COMFYUI_INPUT_PATH>/              # ComfyUI input directory
│   └── workflows/                     # All workflows live here
│       └── <WORKFLOW_NAME>/           # Each workflow in its own subdirectory
│           ├── <WORKFLOW_NAME>.json   # Workflow file
│           └── <WORKFLOW_NAME>_api.json  # (optional) API format
├── project1/
│   ├── inputs/
│   │   └── frame_####.exr
│   └── outputs/
│       ├── anycomfy_effect1_####.exr
│       └── anycomfy_effect2_####.exr
└── project2/
    └── ...
```

## Build Information

### Build Targets

- **Target Name**: `AnyComfy`
- **Bundle Identifier**: `com.comfyui.AnyComfy`
- **Plugin Label**: "AnyComfy"
- **Plugin Grouping**: "ComfyUI"

### Dependencies

- **Base**: `ComfyUICommon` (shared base plugin functionality)
- **Support**: `OfxSupport` (OpenFX support library)
- **JSON**: `nlohmann_json` (workflow parsing)
- **HTTP**: `httplib` (REST API communication)
- **WebSocket**: `ixwebsocket` (async updates)
- **Image I/O**: `tinyexr` (EXR file format)
- **Logging**: `spdlog` (structured logging)

### Build Commands

```bash
# Full build
cmake --preset conan-release -DBUILD_COMFYUI_PLUGINS=ON
cmake --build build/Release --config Release --target AnyComfy

# Using plugin build script
./contrib/dev-tools/build-plugin.sh contrib/plugins/ComfyUI/anycomfy AnyComfy
```

### Bundle Contents

```
AnyComfy.ofx.bundle/
├── Contents/
│   ├── Info.plist
│   ├── MacOS/
│   │   └── AnyComfy.ofx (plugin binary)
│   └── Resources/
│       └── workflows/
│           ├── README.md (workflow documentation)
│           └── template.json (default template for new workflows)
```

## Logging

The plugin logs to:
- **macOS/Linux**: `~/Library/Logs/AnyComfy/anycomfy.log`
- **Windows**: `%USERPROFILE%\AppData\Local\AnyComfy\Logs\anycomfy.log`

Log levels:
- `info` - Normal operations
- `warn` - Warnings and recoverable errors
- `error` - Errors and failures
- `debug` - Verbose debugging (if enabled)

## Troubleshooting

### Workflow Not Found Error

**Symptom**: "No workflow file specified" or "Could not resolve workflow path"

**Solutions**:
1. Click "New Workflow" to create a template
2. Or set "Workflow File Path" to an existing workflow
3. Verify shared mount path is correctly configured

### ComfyUI Server Connection Failed

**Symptom**: "Failed to connect to ComfyUI server"

**Solutions**:
1. Verify server address and port
2. Check ComfyUI server is running (`http://server:8188`)
3. Test connection with curl: `curl http://server:8188/system_stats`
4. Check firewall settings

### Path Not Found Error

**Symptom**: `Exception: Path not found: Front` or similar

**Solution**: Workflow has hardcoded paths. **Update to plugin v1.0.1+** which has smart injection to handle non-templated workflows.

See: [SMART_INJECTION_FIX.md](SMART_INJECTION_FIX.md)

### Directory Not Found Error

**Symptom**: `FileNotFoundError: [WinError 3] The system cannot find the path specified: 'Z:\\out\\PROJECT\\workflow\\v001'`

**Root Cause**: Output directory structure doesn't exist on ComfyUI server

**Solutions**:
1. **Quick Fix**: Create directories manually on server
   ```batch
   mkdir Z:\out\PROJECT\workflow\v001
   ```

2. **Best Practice**: Use the provided Python script on server
   ```bash
   python directory_creator.py PROJECT workflow
   ```

3. **See Complete Guide**: [DIRECTORY_SETUP_GUIDE.md](DIRECTORY_SETUP_GUIDE.md)

### Double Backslash Error

**Symptom**: Paths have `\\\\` instead of `\\`, causing "path not found"

**Solution**: **Update to plugin v1.0.2+** which fixes the double-escaping issue.

See: [PATH_ESCAPING_FIX.md](PATH_ESCAPING_FIX.md)

### Workflow Execution Failed

**Symptom**: Workflow executes but fails with errors

**Solutions**:
1. Test workflow in ComfyUI UI first
2. Verify all required models are installed on server
3. Check ComfyUI server logs for errors
4. Verify LoadEXR and SaveEXR nodes are in workflow
5. **Ensure output directories exist on server** (see DIRECTORY_SETUP_GUIDE.md)

### Browser Doesn't Open

**Symptom**: "New Workflow" button creates workflow but browser doesn't open

**Solutions**:
1. Manually open ComfyUI: `http://<server>:<port>`
2. Load workflow from path shown in plugin logs
3. Check browser is set as default application
4. On Linux, ensure `xdg-open` is installed

### Multiple Instances Conflict

**Symptom**: Multiple AnyComfy instances interfere with each other

**Solutions**:
1. Ensure each instance has unique "Project Name" or "Workflow Name"
2. Use different workflow files for each instance
3. Check plugin logs for instance identifiers
4. Clear cache if needed (disable cache temporarily)

## Comparison with SAM Plugin

| Feature | AnyComfy | SAM Segmentation |
|---------|----------|------------------|
| Workflow Support | Any workflow | SAM segmentation only |
| Model Parameters | None (configured in ComfyUI) | SAM model, DINO model |
| Processing Parameters | None | Threshold, resolution, prompts |
| Flexibility | Maximum | Specific to SAM |
| Use Case | Any ComfyUI workflow | Text-based segmentation |
| Workflow Editing | ComfyUI UI | OFX parameters |
| User Complexity | Lower (no params) | Higher (many params) |

## Future Enhancements

### Potential Improvements

1. **Workflow Upload via API**
   - Upload workflow JSON via ComfyUI API
   - Get workflow ID and open browser with URL: `http://server:port/?workflow=<id>`
   - Auto-load workflow in ComfyUI UI

2. **Workflow Library Browser**
   - Browse available workflows in OFX UI
   - Preview workflow descriptions
   - One-click workflow selection

3. **Workflow Validation**
   - Validate workflow has required LoadEXR/SaveEXR nodes
   - Check for template variable usage
   - Detect common workflow errors

4. **Model Dependency Detection**
   - Parse workflow to detect required models
   - Check if models are available on server
   - Display warnings for missing models

5. **Workflow Versioning**
   - Track workflow versions
   - Rollback to previous versions
   - Diff workflow changes

## License

Copyright OpenFX and contributors to the OpenFX project.
SPDX-License-Identifier: BSD-3-Clause

## Contact

For issues, questions, or contributions related to AnyComfy:
- **GitHub**: https://github.com/AcademySoftwareFoundation/openfx
- **OpenFX Group**: https://groups.google.com/g/openeffects-dev

## See Also

- [ComfyUI Plugin Base Documentation](../common/README.md)
- [SAM Segmentation Plugin](../segmentation/README.md)
- [Workflow Examples](resources/workflows/README.md)
- [OpenFX API Documentation](../../../../Documentation/)

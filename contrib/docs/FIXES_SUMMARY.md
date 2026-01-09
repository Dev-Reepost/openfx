# AnyComfy Plugin - Fixes Summary

## Version History

### v1.0.0 (Initial Release)
- Basic AnyComfy plugin with template workflow support
- Works with workflows containing `${INPUT_PATH}`, `${OUTPUT_PREFIX}`, etc.

### v1.0.1 (Smart Injection)
**Date**: December 26, 2025

**Issue**: Plugin only worked with templated workflows, failed with raw ComfyUI exports

**Fix**: Added smart JSON node injection
- Finds LoadEXR/SaveEXR nodes by `class_type`
- Directly modifies their inputs, regardless of current values
- Works with both templated AND non-templated workflows

**Details**: [SMART_INJECTION_FIX.md](SMART_INJECTION_FIX.md)

### v1.0.2 (Path Escaping Fix)
**Date**: December 26, 2025

**Issue**: Double-escaped backslashes causing invalid paths
```
Error: Path not found: Z:\\\\out\\\\PROJECT\\\\v001
```

**Fix**: Removed manual JSON escaping in smart injection
- Let `nlohmann_json` handle escaping automatically
- Raw Windows paths now correctly converted

**Details**: [PATH_ESCAPING_FIX.md](PATH_ESCAPING_FIX.md)

### v1.0.3 (Async Rendering Directory Creation)

**Date**: December 30, 2025

**Issue**: Directories not created when using async rendering (default mode)
```
FileNotFoundError: [WinError 3] path not found: 'Z:\out\SAM_TEST\any_segmentation\v001'
```

**Root Cause**: Directory creation code only in sync `render()`, not in `renderAsync()`

- Async rendering is the default for performance
- Missing directory creation step in async code path
- Input directories created (by ImageIO), but output directories skipped

**Fix**: Added directory creation to `renderAsync()` before async job submission

- Creates full directory tree on client side
- Syncs to server via network mount
- Happens before background thread starts

**Details**: [ASYNC_RENDER_FIX.md](ASYNC_RENDER_FIX.md)

### v1.0.4 (Filename Version Suffix Fix)

**Date**: December 30, 2025

**Issue**: Output files written but not displayed in Flame - filename mismatch
```
Plugin expects:  SAM_TEST_AnyComfy.0023.exr
ComfyUI writes:  SAM_TEST_AnyComfy_v001.0023.exr
                                     ^^^^^^ Extra version suffix
```

**Root Cause**: SaveEXR node's `version` parameter adds suffix to filename

- Smart injection didn't override `version` parameter
- Workflow's original `version: 1` setting retained
- Version already in directory path (.../v001/) shouldn't be in filename

**Fix**: Override `version` to `-1` in smart injection

- Prevents version suffix in filename
- Matches plugin's expected output path format
- Consistent with base plugin architecture

**Details**: [FILENAME_VERSION_FIX.md](FILENAME_VERSION_FIX.md)

### v1.1.0 (Auto-Workflow Name Derivation)

**Date**: December 30, 2025

**Enhancement**: Automatic workflow name derivation from filename

**Problem**: Users had to manually type the "Workflow Name" parameter, leading to:
- Typos creating wrong directories
- Inconsistent naming between workflow files and output paths
- Extra manual work for every workflow selection

**Solution**: Auto-derive workflow name from selected filename
- Extracts filename from path
- Removes extension and common keywords (`workflow`, `wf`, `api`, `comfyui`)
- Splits by separators (`_`, `-`, `.`, space)
- Joins remaining words with underscores

**Examples**:
```
comfyui_normal_map_deepbump_workflow_api.json → normal_map_deepbump
segmentation_workflow.json                     → segmentation
my-awesome-effect_wf.json                      → my_awesome_effect
```

**User Control**:
- Auto-updates only if current value is empty or default
- Preserves manual overrides
- Clear field to re-enable auto-derivation

**Details**: [AUTO_WORKFLOW_NAME.md](AUTO_WORKFLOW_NAME.md)

## Current Status

### Directory Creation - ✓ FIXED (v1.0.3)

**Previous Issue**: Output directories not created in async rendering mode

**Status**: ✓ **RESOLVED** - Plugin now creates directories automatically in both sync and async modes

**How It Works**:

- Client-side directory creation before workflow submission
- Full directory tree created via network mount
- Directories sync to server automatically
- No manual setup required

**Note**: The server-side limitation (ComfyUI's SaveEXR uses `os.mkdir()` not `os.makedirs()`) is now bypassed by creating directories from the client.

## Quick Troubleshooting

| Error | Cause | Solution | Version |
|-------|-------|----------|---------|
| `Path not found: Front` | Hardcoded workflow paths | Update to v1.0.1+ | v1.0.1+ |
| `Z:\\\\out\\\\...` (quadruple backslashes) | Double JSON escaping | Update to v1.0.2+ | v1.0.2+ |
| `[WinError 3] path not found: Z:\out\...` | Directory not created (async rendering) | Update to v1.0.3+ | v1.0.3+ |
| Frames render but don't display in Flame | Filename mismatch (`_v001` suffix) | Update to v1.0.4+ | v1.0.4+ |
| `output file missing: ...AnyComfy.0023.exr` | Files written as `...AnyComfy_v001.0023.exr` | Update to v1.0.4+ | v1.0.4+ |
| `No workflow file specified` | Workflow path not set | Set path or click "New Workflow" | All versions |
| Wrong directory created (e.g., `ANYCOMFY,_TEST` instead of `SAM_TEST`) | Project Name parameter has wrong value | Check and fix parameter value in plugin UI | All versions |
| Comma in project name (e.g., `MY,PROJECT`) | Invalid naming convention | Use underscores only: `MY_PROJECT` | All versions |

## Files Reference

### Documentation
- **[README.md](README.md)** - Main plugin documentation
- **[SMART_INJECTION_FIX.md](SMART_INJECTION_FIX.md)** - Smart injection explained (v1.0.1)
- **[PATH_ESCAPING_FIX.md](PATH_ESCAPING_FIX.md)** - Path escaping fix explained (v1.0.2)
- **[ASYNC_RENDER_FIX.md](ASYNC_RENDER_FIX.md)** - Async rendering directory creation fix (v1.0.3)
- **[FILENAME_VERSION_FIX.md](FILENAME_VERSION_FIX.md)** - Filename version suffix fix (v1.0.4)
- **[AUTO_WORKFLOW_NAME.md](AUTO_WORKFLOW_NAME.md)** - Auto-workflow name derivation (v1.1.0)
- **[DIRECTORY_SETUP_GUIDE.md](DIRECTORY_SETUP_GUIDE.md)** - Directory setup instructions (legacy)
- **[DIRECTORY_ARCHITECTURE.md](DIRECTORY_ARCHITECTURE.md)** - How directory creation works
- **[PROJECT_NAME_ISSUE.md](PROJECT_NAME_ISSUE.md)** - Wrong project name parameter analysis
- **[FIXES_SUMMARY.md](FIXES_SUMMARY.md)** - This file

### Tools
- **[directory_creator.py](directory_creator.py)** - Python script for directory creation

### Workflows
- **[resources/workflows/template.json](resources/workflows/template.json)** - Default template
- **[comfyui_normal_map_deepbump_workflow_api.json](comfyui_normal_map_deepbump_workflow_api.json)** - Example workflow
- **[comfyui_segmentation_segment_anything_workflow_api.json](comfyui_segmentation_segment_anything_workflow_api.json)** - Example workflow
- And others...

## Installation

```bash
# Build and install universal binary
./contrib/dev-tools/build-macos-universal-plugin.sh -p AnyComfy -t AnyComfy --install

# Verify installation
ls -lh ~/Library/OFX/Plugins/AnyComfy.ofx.bundle/Contents/MacOS/AnyComfy.ofx
lipo -info ~/Library/OFX/Plugins/AnyComfy.ofx.bundle/Contents/MacOS/AnyComfy.ofx
```

**Expected**:
```
Architectures: x86_64 arm64
Size: ~5.1 MB
```

## Testing Workflow

### 1. Setup Server Directories

On ComfyUI server:
```bash
python directory_creator.py TEST_PROJECT segmentation
```

### 2. Configure Plugin

In Flame/Nuke:
- **Server Address**: `192.168.1.100`
- **Server Port**: `8188`
- **Shared Mount**: `/Volumes/share` (Mac) or `Z:` (Windows)
- **Server Mount**: `Z:` (if server is Windows)
- **Project Name**: `TEST_PROJECT`
- **Workflow Name**: `segmentation`
- **Output Version**: `v001`

### 3. Select Workflow

**Option A**: Use template
- Click "New Workflow"
- Edit in ComfyUI
- Save

**Option B**: Use existing
- Set "Workflow File Path" to existing `.json`
- File can be templated OR non-templated

### 4. Render

- Add AnyComfy to timeline
- Render frame

**Expected**:
- Input written to: `Z:\in\TEST_PROJECT\inputs\frame_####.exr`
- Output written to: `Z:\out\TEST_PROJECT\segmentation\v001\anycomfy_effect1_####.exr`

## Common Workflow Types

### Templated Workflow (v1.0.0+)

```json
{
  "1": {
    "inputs": {
      "filepath": "${INPUT_PATH}",
      ...
    },
    "class_type": "LoadEXR"
  },
  "2": {
    "inputs": {
      "filename_prefix": "${OUTPUT_PREFIX}",
      "start_frame": "${FRAME}",
      ...
    },
    "class_type": "SaveEXR"
  }
}
```

### Non-Templated Workflow (v1.0.1+)

```json
{
  "3": {
    "inputs": {
      "filepath": "Front",
      ...
    },
    "class_type": "LoadEXR"
  },
  "6": {
    "inputs": {
      "filename_prefix": "Result",
      "start_frame": 1001,
      ...
    },
    "class_type": "SaveEXR"
  }
}
```

**Smart injection automatically replaces**:
- `"Front"` → `"Z:\\in\\project\\input_0001.exr"`
- `"Result"` → `"Z:\\out\\project\\workflow\\v001\\anycomfy_effect1"`
- `1001` → `1` (current frame)

## Best Practices

### 1. Naming Conventions

- **Projects**: `MY_PROJECT` (uppercase, underscores)
- **Workflows**: `segmentation` (lowercase, underscores)
- **Versions**: `v001`, `v002`, etc.

**Avoid**:
- Spaces: `MY PROJECT` ❌
- Commas: `MY,PROJECT` ❌
- Special chars: `MY#PROJECT` ❌

### 2. Directory Structure

Create on server BEFORE rendering:
```
Z:\out\
├── PROJECT1\
│   ├── segmentation\v001\
│   ├── upscale\v001\
│   └── denoise\v001\
└── PROJECT2\
    └── workflow\v001\
```

### 3. Workflow Management

- Store workflows in: `${SHARED_MOUNT}/workflows/`
- Name descriptively: `deepbump_normals.json`
- Test in ComfyUI UI first
- Version workflows: `v1`, `v2`, etc.

### 4. Troubleshooting Workflow

1. **Check plugin version**
   ```bash
   # Should be v1.0.2 or higher
   ls -l ~/Library/OFX/Plugins/AnyComfy.ofx.bundle
   ```

2. **Verify directories exist**
   ```bash
   # On ComfyUI server
   ls Z:\out\PROJECT\workflow\v001
   ```

3. **Test workflow in ComfyUI**
   - Open ComfyUI: `http://server:8188`
   - Load workflow
   - Queue prompt
   - Check for errors

4. **Check plugin logs**
   ```bash
   # macOS
   tail -f ~/Library/Logs/AnyComfy/anycomfy.log
   ```

5. **Review error messages**
   - "Path not found" → Smart injection issue (update to v1.0.1+)
   - "\\\\\" in path → Escaping issue (update to v1.0.2+)
   - "WinError 3" → Directory doesn't exist (create on server)
   - Wrong directory created → Check Project Name parameter value

6. **Verify parameter values**
   - Project Name should match your intended project (no commas!)
   - Workflow Name should match the workflow you're running
   - Check for typos or invalid characters
   - See [PROJECT_NAME_ISSUE.md](PROJECT_NAME_ISSUE.md) for details

## Future Enhancements

### Planned Features

1. **Auto Directory Creation**
   - Plugin sends API request to create directories
   - Requires custom ComfyUI endpoint

2. **Workflow Validation**
   - Check for LoadEXR/SaveEXR before execution
   - Validate template variables
   - Warn about missing models

3. **Workflow Upload via API**
   - Upload workflow JSON to server
   - Get workflow ID
   - Open browser with `?workflow=<id>`

4. **Enhanced Error Messages**
   - Detect common issues
   - Provide actionable solutions
   - Link to documentation

## Support

### Documentation
- Main README: [README.md](README.md)
- Smart Injection: [SMART_INJECTION_FIX.md](SMART_INJECTION_FIX.md)
- Path Escaping: [PATH_ESCAPING_FIX.md](PATH_ESCAPING_FIX.md)
- Directory Setup: [DIRECTORY_SETUP_GUIDE.md](DIRECTORY_SETUP_GUIDE.md)
- Directory Architecture: [DIRECTORY_ARCHITECTURE.md](DIRECTORY_ARCHITECTURE.md)
- Project Name Issue: [PROJECT_NAME_ISSUE.md](PROJECT_NAME_ISSUE.md)

### Tools
- Directory Creator: [directory_creator.py](directory_creator.py)

### Contact
- GitHub: https://github.com/AcademySoftwareFoundation/openfx
- OpenFX Group: https://groups.google.com/g/openeffects-dev

---

**Last Updated**: December 30, 2025
**Current Version**: 1.1.0
**Status**: Production Ready ✓

# AnyComfy Resources

This directory contains resources bundled with the AnyComfy OFX plugin.

## Contents

### JavaScript Extension (Multiple Versions for Compatibility)

**Purpose**: ComfyUI extension for automatic workflow loading

**Files Available**:
- `ofx_autoloader.js` - **Standard version (RECOMMENDED)** - Uses dynamic imports, tested on ancient and modern ComfyUI
- `ofx_autoloader_nomodule.js` - No-module fallback version (for extremely old ComfyUI without ES module support)
- `ofx_autoloader_compat.js` - Compatibility version (ES5 + IIFE)
- `ofx_autoloader_v2.js` - Original v2 testing version (now merged into standard)
- `ofx_autoloader_minimal.js` - Verbose diagnostic version (maximum debugging)

**Installation**: Copy ONE of these files to `ComfyUI/web/extensions/ofx_autoloader.js`

**Standard Installation** (works with most ComfyUI versions):
```bash
cp ofx_autoloader.js /path/to/ComfyUI/web/extensions/
```

**For Extremely Old ComfyUI** (if standard version doesn't work):
```bash
# Only if you see "Failed to import app.js" errors
cp ofx_autoloader_nomodule.js /path/to/ComfyUI/web/extensions/ofx_autoloader.js
```

**What it does**:
- Detects `?load_local_json=<filename>` URL parameter
- Automatically loads workflows from ComfyUI's input folder
- Cleans up URL after loading
- Shows success/error notifications

**Usage**:
1. Install the extension in ComfyUI (see guides below)
2. Configure "ComfyUI Input Directory" parameter in AnyComfy plugin
3. Click "New Workflow" button - browser opens with workflow loaded!

**Documentation & Troubleshooting**:
- [QUICK_FIX_GUIDE.md](QUICK_FIX_GUIDE.md) - Quick compatibility troubleshooting
- [INSTALL_CUSTOM_NODE_VERSION.md](INSTALL_CUSTOM_NODE_VERSION.md) - For custom node installations
- [DIAGNOSTIC_CHECKLIST.md](DIAGNOSTIC_CHECKLIST.md) - Detailed diagnostic steps
- [WORKFLOW_AUTO_LOAD.md](../../../../docs/WORKFLOW_AUTO_LOAD.md) - Complete documentation

### Workflow Templates

**Directory**: `workflows/`

Contains default workflow templates included with the plugin.

**Files**:
- `template.json` - Minimal workflow with LoadEXR and SaveEXR nodes
- `README.md` - Workflow template documentation

## Bundle Structure

When the plugin is built, these resources are copied into the `.ofx.bundle`:

```
AnyComfy.ofx.bundle/
├── Contents/
│   ├── Info.plist
│   ├── MacOS/
│   │   └── AnyComfy.ofx
│   └── Resources/
│       ├── workflows/
│       │   ├── README.md
│       │   └── template.json
│       └── (ofx_autoloader.js - user must install manually)
```

**Note**: The `ofx_autoloader.js` file is **not** automatically bundled because it must be installed in ComfyUI's extensions directory, not in the OFX bundle.

## Installation Instructions

### For End Users

1. **Install the plugin** (standard OFX installation)
   - Copy `AnyComfy.ofx.bundle` to your OFX plugins directory

2. **Install the ComfyUI extension** (manual step):
   ```bash
   cp ofx_autoloader.js /path/to/ComfyUI/web/extensions/
   ```

3. **Configure the plugin**:
   - Set "ComfyUI Input Directory" in the Server page
   - Example: `Z:\ComfyUI\input` (Windows)

4. **Use auto-loading**:
   - Click "New Workflow" button
   - Browser opens with workflow loaded
   - Edit and save in ComfyUI

### For Developers

The `ofx_autoloader.js` file should be distributed separately from the plugin bundle:

**Recommended distribution methods**:
1. **Documentation** - Include installation instructions in README
2. **Setup script** - Provide script to copy extension to ComfyUI
3. **Installer** - If using an installer, copy extension automatically
4. **GitHub release** - Include as separate download asset

**Why separate?**
- ComfyUI extensions must be in ComfyUI's directory structure
- Users may have ComfyUI installed in different locations
- Allows users to opt-out of auto-loading feature

## Updating the Extension

If you modify `ofx_autoloader.js`:

1. **Edit the source** in this directory
2. **Rebuild the plugin** (extension is copied to bundle resources)
3. **Reinstall manually** in ComfyUI:
   ```bash
   cp ofx_autoloader.js /path/to/ComfyUI/web/extensions/
   ```
4. **Restart ComfyUI** server for changes to take effect

## Troubleshooting

### Extension not working?

**Check**:
1. File is in correct location: `ComfyUI/web/extensions/ofx_autoloader.js`
2. ComfyUI server was restarted after installation
3. Browser console shows `[OFX AutoLoader]` messages
4. No JavaScript errors in browser console

**Debug**:
```bash
# Verify file exists
ls /path/to/ComfyUI/web/extensions/ofx_autoloader.js

# Check file permissions
chmod 644 /path/to/ComfyUI/web/extensions/ofx_autoloader.js

# Restart ComfyUI
# (depends on your installation method)
```

### Workflow not auto-loading?

**Check**:
1. "ComfyUI Input Directory" is configured in plugin
2. Workflow file was copied to input directory
3. Browser URL contains `?load_local_json=...` parameter
4. Browser console shows fetch request to `/view?filename=...`

## License

Copyright OpenFX and contributors to the OpenFX project.
SPDX-License-Identifier: BSD-3-Clause

## See Also

- [Workflow Auto-Loading Documentation](../../../../docs/WORKFLOW_AUTO_LOAD.md)
- [AnyComfy Plugin README](../README.md)
- [ComfyUI Extensions](https://github.com/comfyanonymous/ComfyUI/wiki/Extensions)

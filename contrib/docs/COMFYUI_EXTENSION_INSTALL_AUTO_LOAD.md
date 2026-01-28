# Quick Start: Workflow Auto-Loading

**5-minute setup to enable automatic workflow loading in ComfyUI**

## What You'll Get

- Click "New Workflow" → Browser opens with workflow **already loaded**
- No more manual Ctrl+O or drag-and-drop
- Saves 30-60 seconds per workflow creation

## Prerequisites

- AnyComfy OFX plugin installed
- ComfyUI server running
- Network access to shared storage

## Installation Steps

### Step 1: Install ComfyUI Extension (One-Time)

**Copy the JavaScript file to ComfyUI's extensions directory:**

```bash
# Find the extension file in AnyComfy resources
# Location: contrib/plugins/ComfyUI/anycomfy/resources/ofx_autoloader.js

# Copy to ComfyUI (IMPORTANT: note 's' in 'extensions')
# Windows
copy ofx_autoloader.js C:\path\to\ComfyUI\web\extensions\

# Linux/macOS
cp ofx_autoloader.js /path/to/ComfyUI/web/extensions/
```

**⚠️ CRITICAL**: The directory is `web/extensions/` (plural with 's'), NOT `web/extension/`

**Restart ComfyUI server** (required for extension to load)

### Step 1b: Compatibility Version (If Standard Version Doesn't Work)

If you see NO console messages after restarting ComfyUI, your version may be too old for ES6 imports.

**Use the compatibility version instead:**

```bash
# Use this file instead:
# Location: contrib/plugins/ComfyUI/anycomfy/resources/ofx_autoloader_compat.js

# Copy to same location (replace the standard version):
copy ofx_autoloader_compat.js C:\path\to\ComfyUI\web\extensions\ofx_autoloader.js
```

The compatibility version uses older JavaScript patterns and works with ancient ComfyUI versions.

### Step 2: Configure AnyComfy Plugin

**In your OFX host (Flame, Nuke, etc.):**

1. Add **AnyComfy effect** to timeline
2. Open **Server** page in effect parameters
3. Set **"ComfyUI Input Directory"**:
   - **Windows**: `Z:\ComfyUI\input`
   - **Linux**: `/mnt/storage/ComfyUI/input`
   - **macOS**: `/Volumes/Storage/ComfyUI/input`

   *(Replace with your actual server-side ComfyUI input path)*

4. Verify other settings:
   - **Server Address**: `localhost` (or ComfyUI server IP)
   - **Server Port**: `8188`
   - **Server Mount Point**: `Z:` (Windows) or `/mnt/storage` (Linux)

### Step 2.5: Verify Extension Installation

**Before testing, verify the extension loaded correctly:**

1. Open ComfyUI in browser: `http://localhost:8188`
2. Open browser console (press F12, click Console tab)
3. Look for one of these messages:
   - ✓ `[OFX AutoLoader] Extension loaded` (standard version)
   - ✓ `[OFX AutoLoader] Extension registered successfully (compatibility mode)` (compat version)

If you see NO `[OFX AutoLoader]` messages:
- Extension file is in wrong location (check for 's' in `extensions/`)
- ComfyUI wasn't restarted after installing extension
- Try the compatibility version instead

### Step 3: Test Auto-Loading

1. Click **"New Workflow"** button in AnyComfy plugin
2. Browser should open with workflow **already loaded**
3. Console should show: `[OFX AutoLoader] Workflow loaded successfully: <filename>`
4. Edit workflow in ComfyUI
5. Save (Ctrl+S)
6. Return to OFX and render

**Troubleshooting**:
- No auto-load? See [COMFYUI_AUTOLOADER_TROUBLESHOOTING.md](../../../docs/COMFYUI_AUTOLOADER_TROUBLESHOOTING.md)
- Ancient ComfyUI version? See [COMFYUI_VERSION_COMPATIBILITY.md](../../../docs/COMFYUI_VERSION_COMPATIBILITY.md)

## Quick Reference

### File Locations

| Component | Location |
|-----------|----------|
| Extension source | `contrib/plugins/ComfyUI/anycomfy/resources/ofx_autoloader.js` |
| Install location | `ComfyUI/web/extensions/ofx_autoloader.js` |
| Workflow storage | `${SHARED_MOUNT}/workflows/` |
| Auto-load copy | `${COMFYUI_INPUT}/` |

### Plugin Parameters

| Parameter | Example Value | Description |
|-----------|--------------|-------------|
| Server Address | `192.168.1.100` | ComfyUI server IP/hostname |
| Server Port | `8188` | ComfyUI server port |
| Server Mount Point | `Z:` | Server-side shared storage path |
| **ComfyUI Input Directory** | `Z:\ComfyUI\input` | **Server-side ComfyUI input folder** |

### Verification Checklist

- [ ] Extension file copied to `ComfyUI/web/extensions/`
- [ ] ComfyUI server restarted
- [ ] "ComfyUI Input Directory" parameter configured
- [ ] Parameter uses **server-side** path (not client-side)
- [ ] Directory exists and is writable
- [ ] Click "New Workflow" opens browser with loaded workflow

## Common Issues

### ❌ Workflow doesn't auto-load

**Check**:
- Is extension installed? `ls ComfyUI/web/extensions/ofx_autoloader.js`
- Was ComfyUI restarted after installing extension?
- Is "ComfyUI Input Directory" configured in plugin?
- Does browser URL have `?load_local_json=...` parameter?

**Solution**: See [WORKFLOW_AUTO_LOAD.md](../../../docs/WORKFLOW_AUTO_LOAD.md) for detailed troubleshooting

### ❌ File not found error

**Check**:
- Path is **server-side** (not client-side)
- Directory exists: `dir Z:\ComfyUI\input` (Windows) or `ls /path/to/ComfyUI/input` (Linux)
- Network mount is connected

**Solution**: Create directory if missing, verify path configuration

### ❌ Extension not working

**Check**:
- Browser console (F12) shows `[OFX AutoLoader]` messages?
- Any JavaScript errors?
- Extension file has correct permissions?

**Solution**: Clear browser cache (Ctrl+Shift+R), check file permissions

## Optional Configuration

### Disable Auto-Loading

Leave "ComfyUI Input Directory" **empty** to disable auto-loading. Browser will still open, but you'll need to manually load the workflow (Ctrl+O).

### Custom ComfyUI Installation

If ComfyUI is installed in a non-standard location:

1. Find the `web/extensions/` directory in your installation
2. Copy `ofx_autoloader.js` to that location
3. Restart ComfyUI

### Multiple ComfyUI Servers

If you have multiple ComfyUI servers:

- Each server needs the extension installed
- Configure "ComfyUI Input Directory" for the active server
- Switch parameter value when switching servers

## Need Help?

**Documentation**:
- [Full Auto-Loading Documentation](../../../docs/WORKFLOW_AUTO_LOAD.md)
- [AnyComfy Plugin README](README.md)
- [ComfyUI Extensions Guide](https://github.com/comfyanonymous/ComfyUI/wiki/Extensions)

**Logs**:
- Plugin logs: Check OFX host console or `~/Library/Logs/AnyComfy/anycomfy.log`
- ComfyUI logs: Check ComfyUI server console
- Browser logs: Open browser console (F12)

**Common Log Messages**:
```
[info] Copied workflow to ComfyUI input directory: Z:\ComfyUI\input\anycomfy_effect1_123456.json
[info] Opening URL with auto-load: http://localhost:8188/?load_local_json=anycomfy_effect1_123456.json
[OFX AutoLoader] Loading workflow: anycomfy_effect1_123456.json
[OFX AutoLoader] Successfully loaded: anycomfy_effect1_123456.json
```

## Advanced

### Extension Customization

Edit `ofx_autoloader.js` to customize behavior:
- Change retry logic
- Add custom notifications
- Modify URL cleanup behavior
- Add workflow validation

### Automated Installation

Create a setup script:

```bash
#!/bin/bash
# install_autoload.sh

COMFYUI_PATH="/path/to/ComfyUI"
EXTENSION_SRC="contrib/plugins/ComfyUI/anycomfy/resources/ofx_autoloader.js"

# Copy extension
cp "$EXTENSION_SRC" "$COMFYUI_PATH/web/extensions/"

# Restart ComfyUI (example using systemd)
sudo systemctl restart comfyui

echo "Auto-loader installed! Configure plugin and test."
```

---

**Version**: 1.2.0
**Last Updated**: January 9, 2026
**Estimated Setup Time**: 5 minutes

# Quick Fix for Ancient ComfyUI

Your `web/extensions` folder exists (proven by core extensions there), but the file isn't loading. The ES6 `import` statement is likely failing silently.

## Try These Versions in Order

### Version 1: No-Module Version (Try This First)

**File**: `ofx_autoloader_nomodule.js`

This version doesn't use any module system. Copy it to:
```
C:\Users\reepost\ComfyUI\web\extensions\ofx_autoloader.js
```

**Command:**
```bash
copy ofx_autoloader_nomodule.js C:\Users\reepost\ComfyUI\web\extensions\ofx_autoloader.js
```

**Restart ComfyUI**, then check browser console for:
```
[OFX AutoLoader NoModule] File execution started
[OFX AutoLoader NoModule] Initializing...
```

If you see these messages, it's working!

### Version 2: Dynamic Import Version (If v1 Doesn't Work)

**File**: `ofx_autoloader_v2.js`

Uses dynamic imports instead of static imports.

```bash
copy ofx_autoloader_v2.js C:\Users\reepost\ComfyUI\web\extensions\ofx_autoloader.js
```

Restart ComfyUI and check console for:
```
[OFX AutoLoader v2] JavaScript file is being executed
```

### Version 3: Minimal Verbose Version (If Both Above Fail)

**File**: `ofx_autoloader_minimal.js`

Ultra-verbose logging with extensive debugging.

```bash
copy ofx_autoloader_minimal.js C:\Users\reepost\ComfyUI\web\extensions\ofx_autoloader.js
```

Restart ComfyUI and check console for:
```
=== OFX AutoLoader: File is loading ===
```

## Testing Checklist

After copying each version:

1. **Stop ComfyUI** (Ctrl+C)
2. **Restart ComfyUI**:
   ```bash
   python main.py --listen --force-fp32 --fp32-vae --input-directory S:\002_COMFYUI\in --output-directory S:\002_COMFYUI\out
   ```
3. **Open browser**: http://localhost:8188
4. **Open console** (F12)
5. **Look for messages** starting with `[OFX AutoLoader`

## What Each Version Should Show

### No-Module Version (Recommended)
```
[OFX AutoLoader NoModule] File execution started
[OFX AutoLoader NoModule] Initializing...
[OFX AutoLoader NoModule] app found after X attempts
[OFX AutoLoader NoModule] ✓ Extension registered successfully
```

When opening workflow with `?load_local_json=file.json`:
```
[OFX AutoLoader NoModule] Setup phase executing
[OFX AutoLoader NoModule] URL parameter 'load_local_json': file.json
[OFX AutoLoader NoModule] Fetching workflow from: /view?filename=file.json&type=input
[OFX AutoLoader NoModule] ✓✓✓ SUCCESS! Workflow loaded: file.json
```

## If STILL No Messages

If you see ZERO messages from ANY version, there are three possibilities:

### Possibility 1: File Not Being Loaded

Check if file is served:
```
http://localhost:8188/extensions/ofx_autoloader.js
```

- If **404**: File not in right place
- If **shows code**: File is there but not executing

### Possibility 2: JavaScript Errors Preventing Execution

Open browser console and look for ANY JavaScript errors (red text), especially:
- Syntax errors
- Parse errors
- Module errors

### Possibility 3: Extensions Loading from Different Location

Your ComfyUI might load user extensions from a different folder. Try:

```bash
# Create custom node folder
cd C:\Users\reepost\ComfyUI\custom_nodes
mkdir ofx-autoloader
cd ofx-autoloader
echo. > __init__.py
mkdir js

# Copy the no-module version
copy ..\..\path\to\ofx_autoloader_nomodule.js js\ofx_autoloader.js
```

## Quick Test Command Sequence

```bash
# 1. Copy no-module version
copy ofx_autoloader_nomodule.js C:\Users\reepost\ComfyUI\web\extensions\ofx_autoloader.js

# 2. Verify it's there
dir C:\Users\reepost\ComfyUI\web\extensions\ofx_autoloader.js

# 3. Restart ComfyUI
cd C:\Users\reepost\ComfyUI
# Press Ctrl+C to stop current instance
python main.py --listen --force-fp32 --fp32-vae --input-directory S:\002_COMFYUI\in --output-directory S:\002_COMFYUI\out

# 4. Open browser and check console (F12)
```

## Expected Success

When working correctly, the browser console should fill with detailed logs about every step:
- File loading
- Extension registration
- URL parameter detection
- Workflow fetching
- Workflow loading
- Success confirmation

You'll also get a browser alert saying "✓ Workflow Auto-Loaded Successfully!"

## Next Steps After Success

Once you see the console messages, test the full workflow:

1. In OFX plugin, click "New Workflow"
2. Browser opens with URL: `http://localhost:8188/?load_local_json=filename.json`
3. Console shows success messages
4. Workflow appears in ComfyUI
5. Alert notification appears

If this works, you're done! The feature is fully functional.

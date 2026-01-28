# Installing OFX Auto-Loader for Older ComfyUI (Custom Nodes Method)

Your ComfyUI version loads JavaScript from **custom_nodes** directories, not from `web/extensions/`.

## Installation for Custom Nodes Architecture

### Option 1: Create Custom Node Folder (Recommended)

Create this structure on your ComfyUI server:

```
C:\Users\reepost\ComfyUI\custom_nodes\ofx-autoloader\
├── __init__.py          (empty Python file - required)
└── js\
    └── ofx_autoloader.js
```

### Step-by-Step Instructions

**Step 1: Create the directory structure**

```bash
# On ComfyUI server (Windows)
cd C:\Users\reepost\ComfyUI\custom_nodes
mkdir ofx-autoloader
cd ofx-autoloader
mkdir js
```

**Step 2: Create empty __init__.py**

```bash
# Create empty Python file (required for ComfyUI to recognize it)
type nul > __init__.py
```

**Step 3: Copy JavaScript file**

Copy the minimal version to:
```
C:\Users\reepost\ComfyUI\custom_nodes\ofx-autoloader\js\ofx_autoloader.js
```

Use the **minimal version** for maximum compatibility:
- Source: `contrib/plugins/ComfyUI/anycomfy/resources/ofx_autoloader_minimal.js`
- Destination: `C:\Users\reepost\ComfyUI\custom_nodes\ofx-autoloader\js\ofx_autoloader.js`

**Step 4: Restart ComfyUI**

```bash
# Stop ComfyUI (Ctrl+C in terminal)
# Restart:
python main.py --listen --force-fp32 --fp32-vae --input-directory S:\002_COMFYUI\in --output-directory S:\002_COMFYUI\out
```

**Step 5: Verify**

1. Open http://localhost:8188
2. Open browser console (F12)
3. Look for: `=== OFX AutoLoader: File is loading ===`

If you see this, the extension is loading!

## Option 2: Check if web/extensions Works

Some older ComfyUI versions still support `web/extensions/` even though custom nodes use a different location.

**Try this first (simpler):**

```bash
# Check if directory exists
dir C:\Users\reepost\ComfyUI\web\extensions

# If it exists, copy the file there
copy ofx_autoloader_minimal.js C:\Users\reepost\ComfyUI\web\extensions\ofx_autoloader.js
```

Restart ComfyUI and check browser console.

If you see `=== OFX AutoLoader: File is loading ===`, this method works!

## Option 3: Direct Integration with Existing Custom Node

If creating a new custom node doesn't work, you can add it to an existing one.

**Add to ComfyUI-N-Sidebar:**

```bash
# Copy to sidebar's JavaScript directory
copy ofx_autoloader_minimal.js C:\Users\reepost\ComfyUI\custom_nodes\ComfyUI-N-Sidebar\js\ofx_autoloader.js
```

Note: The sidebar extension structure might be different (uses `app/panels/` etc), but it might have a `js/` folder at the root.

## Verification After Installation

### Success Signs

Browser console should show:
```
=== OFX AutoLoader: File is loading ===
=== OFX AutoLoader: Starting initialization ===
=== OFX AutoLoader: App detected, registering extension ===
=== OFX AutoLoader: Extension registered successfully ===
```

### When Opening Workflow

When plugin opens browser with `?load_local_json=filename.json`:
```
=== OFX AutoLoader: Setup phase running ===
=== OFX AutoLoader: URL parameter 'load_local_json' = filename.json ===
=== OFX AutoLoader: Attempting to load: filename.json ===
=== OFX AutoLoader: Fetching from: /view?filename=filename.json&type=input ===
=== OFX AutoLoader: SUCCESS - Workflow loaded: filename.json ===
```

## Recommended: Try Options in This Order

1. **Try Option 2 first** (web/extensions - if directory exists)
   - Simplest, might just work
   - Takes 30 seconds to test

2. **Try Option 1 if Option 2 fails** (create custom node)
   - Proper way for your ComfyUI version
   - Takes 2-3 minutes to set up

3. **Try Option 3 if all else fails** (add to existing custom node)
   - Last resort
   - Might interfere with sidebar updates

## Full Example for Option 1

```bash
# Complete command sequence for Option 1

# Navigate to custom_nodes
cd C:\Users\reepost\ComfyUI\custom_nodes

# Create structure
mkdir ofx-autoloader
cd ofx-autoloader
mkdir js

# Create empty __init__.py
echo. > __init__.py

# Now copy ofx_autoloader_minimal.js to the js\ folder
# (manually or via copy command)

# Verify structure
dir
# Should show: __init__.py and js\
dir js
# Should show: ofx_autoloader.js

# Restart ComfyUI
cd C:\Users\reepost\ComfyUI
python main.py --listen --force-fp32 --fp32-vae --input-directory S:\002_COMFYUI\in --output-directory S:\002_COMFYUI\out
```

## Troubleshooting

### No console messages at all

- JavaScript file not loading
- Try different location (Option 2 vs Option 1)
- Check file permissions (must be readable)
- Check filename is exactly `ofx_autoloader.js` (no .txt extension)

### "window.app not found after 30 seconds"

- ComfyUI too old for extension system
- Might need completely different approach
- Report this if it happens

### Browser shows the file when accessing directly

If `http://localhost:8188/extensions/ofx_autoloader.js` shows the code but it doesn't execute:
- The file is served but not being loaded as an extension
- Use Option 1 (custom node method) instead

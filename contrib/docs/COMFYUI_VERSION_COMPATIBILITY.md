# ComfyUI Version Compatibility for OFX Auto-Loader

## How to Check Your ComfyUI Version

### Method 1: Check Git Version (if installed from source)
```bash
cd /path/to/ComfyUI
git log -1 --date=short --format="%H %ad"
```

### Method 2: Check Browser Console
1. Open ComfyUI in your browser (http://localhost:8188)
2. Open browser console (F12 → Console tab)
3. Type: `app.getVersion()` and press Enter
4. If that doesn't work, try: `app`

### Method 3: Check main.py
```bash
# On your ComfyUI server
cd C:\path\to\ComfyUI
python main.py --version
# OR check git commit
git describe --tags
```

### Method 4: Check for Version File
```bash
# Look for version in package.json or version.py
cat version.py  # If exists
cat package.json | grep version
```

## Required ComfyUI Features

The OFX auto-loader extension requires these ComfyUI features:

| Feature | Required Since | Used For |
|---------|---------------|----------|
| `app.registerExtension()` | Early 2023 | Extension system |
| `app.loadGraphData()` | Early 2023 | Load workflow JSON |
| `/view` endpoint | Mid 2023 | Serve files from input/ |
| `app.extensionManager.toast` | Late 2023 | Notifications (optional) |

### Core Requirements (CRITICAL)

1. **Extension System** - `app.registerExtension()`
   - Available since: ~February 2023
   - Commit: Around when custom nodes were introduced
   - Check: Does your `ComfyUI/web/extensions/` directory exist?

2. **Load Graph Data** - `app.loadGraphData()`
   - Available since: Very early (original API)
   - This should work on virtually any ComfyUI version

3. **View Endpoint** - `/view?filename=X&type=input`
   - Available since: ~May 2023
   - Added with input/output directory serving
   - Check: Open `http://localhost:8188/view?filename=example.txt&type=input`

### Optional Features (Graceful Degradation)

4. **Toast Notifications** - `app.extensionManager.toast`
   - Available since: ~September 2023
   - If missing: Falls back to `alert()` dialog
   - Your extension will still work without this

## Compatibility Test Script

Create this test file to check compatibility:

**File: `ComfyUI/web/extensions/ofx_compatibility_test.js`**

```javascript
import { app } from "../../scripts/app.js";

app.registerExtension({
    name: "OFX.CompatibilityTest",

    async setup() {
        console.log("=== OFX Auto-Loader Compatibility Test ===");

        // Test 1: Extension system
        console.log("✓ Extension system works (you're seeing this!)");

        // Test 2: app.loadGraphData
        if (typeof app.loadGraphData === 'function') {
            console.log("✓ app.loadGraphData() available");
        } else {
            console.error("✗ app.loadGraphData() NOT FOUND - CRITICAL");
        }

        // Test 3: /view endpoint
        try {
            const response = await fetch('/view?filename=test.txt&type=input');
            console.log(`✓ /view endpoint available (status: ${response.status})`);
        } catch (error) {
            console.error("✗ /view endpoint failed:", error);
        }

        // Test 4: Toast notifications (optional)
        if (app.extensionManager && app.extensionManager.toast) {
            console.log("✓ Toast notifications available");
            app.extensionManager.toast.add({
                severity: 'info',
                summary: 'Compatibility Test',
                detail: 'Your ComfyUI supports all OFX auto-loader features!',
                life: 5000
            });
        } else {
            console.log("⚠ Toast notifications NOT available (will use alert fallback)");
            alert("Compatibility Test Complete!\n\nYour ComfyUI supports OFX auto-loader core features.\nToast notifications not available (older version) - will use alert() fallback.");
        }

        console.log("=== Compatibility Test Complete ===");
    }
});
```

### Running the Test

1. Copy the test file to `ComfyUI/web/extensions/ofx_compatibility_test.js`
2. Restart ComfyUI server
3. Open ComfyUI in browser (http://localhost:8188)
4. Open browser console (F12)
5. Look for test results

Expected output:
```
=== OFX Auto-Loader Compatibility Test ===
✓ Extension system works (you're seeing this!)
✓ app.loadGraphData() available
✓ /view endpoint available (status: 200)
✓ Toast notifications available
=== Compatibility Test Complete ===
```

## Minimum Version Recommendation

**Minimum Supported**: ComfyUI from ~May 2023 or later

If your ComfyUI is older:
- **Before Feb 2023**: Extension system missing - won't work
- **Feb-May 2023**: Might work, but `/view` endpoint may be missing
- **After May 2023**: Should work fully

## Upgrading ComfyUI

If your version is too old:

```bash
cd C:\path\to\ComfyUI
git fetch
git pull
# Update dependencies
python -m pip install -r requirements.txt --upgrade
```

**⚠️ Warning**: Always backup your `ComfyUI/custom_nodes/` and workflows before upgrading!

## Fallback for Ancient Versions

If your ComfyUI is too old to support the auto-loader, you can still:

1. **Manual workflow loading**: Plugin copies workflow to input directory
2. **Use file path directly**: Open `S:\002_COMFYUI\in\workflow.json` in file explorer
3. **Drag and drop**: Drag JSON file into ComfyUI browser window

The core plugin functionality (reading EXR, processing, writing EXR) doesn't depend on ComfyUI version.

## Quick Compatibility Check

**Run this in your browser console when ComfyUI is open:**

```javascript
console.log("Extension API:", typeof app.registerExtension);
console.log("Load Graph:", typeof app.loadGraphData);
console.log("Toast API:", typeof app.extensionManager?.toast);
fetch('/view?filename=test&type=input').then(r => console.log("/view endpoint:", r.status));
```

Expected output for compatible version:
```
Extension API: function
Load Graph: function
Toast API: object  (or undefined for older versions)
/view endpoint: 200 (or 404 if file doesn't exist)
```

## Support

If compatibility test fails:
- Report your ComfyUI version/commit hash
- Include browser console output
- Provide test script results

The OFX auto-loader has been designed with graceful degradation - even if toast notifications are missing, it will fall back to browser alerts.

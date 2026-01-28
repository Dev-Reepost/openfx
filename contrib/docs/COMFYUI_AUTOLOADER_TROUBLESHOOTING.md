# Troubleshooting OFX Auto-Loader Installation

## Issue: Extension Not Loading (No Console Messages)

### Step 1: Verify Correct File Location

The file MUST be in the correct directory (note the 's' in extensions):

```
C:\path\to\ComfyUI\
└── web\
    └── extensions\          ← PLURAL "extensions" with 's'
        └── ofx_autoloader.js
```

**Common mistake**: Putting it in `web/extension/` (singular) instead of `web/extensions/` (plural)

### Step 2: Check Server Logs

After placing the file, restart ComfyUI and check the server console for:
```
Loading extension: ofx_autoloader.js
```

If you DON'T see this message, the file is in the wrong location.

### Step 3: Verify File Is Being Served

Open this URL in your browser:
```
http://localhost:8188/extensions/ofx_autoloader.js
```

You should see the JavaScript code. If you get 404, the file is not in the correct location.

### Step 4: Check Browser Console for Import Errors

After fixing the location and restarting ComfyUI, check browser console (F12) for:
- ✓ Expected: `[OFX AutoLoader] ...` messages (even if no workflow to load)
- ✗ Error: `Failed to load module` or `Cannot find module`

## Ancient ComfyUI Version Compatibility

Your console shows this is an older ComfyUI version. The ES6 `import` statement might not work.

### Fallback Version (No Import Statement)

If the regular version doesn't load, use this compatibility version instead:

**File: `ComfyUI/web/extensions/ofx_autoloader.js`**

```javascript
// Copyright OpenFX and contributors to the OpenFX project.
// SPDX-License-Identifier: BSD-3-Clause

/**
 * OFX Auto-Loader Extension for ComfyUI (Compatibility Version)
 * Works with older ComfyUI versions that may have issues with ES6 imports
 */

(function() {
    // Wait for ComfyUI app to be available
    const waitForApp = setInterval(function() {
        if (typeof window.app !== 'undefined' && window.app.registerExtension) {
            clearInterval(waitForApp);
            initAutoLoader();
        }
    }, 100);

    function initAutoLoader() {
        console.log("[OFX AutoLoader] Extension loaded (compatibility mode)");

        window.app.registerExtension({
            name: "OFX.AutoLoader",

            async setup() {
                console.log("[OFX AutoLoader] Setup running...");

                // Check if we have a workflow to auto-load
                const params = new URLSearchParams(window.location.search);
                const fileName = params.get('load_local_json');

                if (!fileName) {
                    console.log("[OFX AutoLoader] No workflow specified in URL");
                    return;
                }

                console.log("[OFX AutoLoader] Detected workflow auto-load request: " + fileName);

                try {
                    // ComfyUI serves files in the 'input' folder via the /view endpoint
                    const url = "/view?filename=" + encodeURIComponent(fileName) + "&type=input";
                    console.log("[OFX AutoLoader] Fetching workflow from: " + url);

                    const response = await fetch(url);

                    if (!response.ok) {
                        throw new Error("HTTP " + response.status + ": " + response.statusText);
                    }

                    // Parse the workflow JSON
                    const workflowJson = await response.json();
                    console.log("[OFX AutoLoader] Successfully fetched workflow");

                    // Load the workflow into ComfyUI
                    if (typeof window.app.loadGraphData === 'function') {
                        await window.app.loadGraphData(workflowJson);
                        console.log("[OFX AutoLoader] Workflow loaded successfully: " + fileName);
                    } else {
                        throw new Error("app.loadGraphData() not available - ComfyUI version too old");
                    }

                    // Clean up the URL parameter
                    window.history.replaceState({}, document.title, "/");
                    console.log("[OFX AutoLoader] URL cleaned up");

                    // Try to show notification
                    if (window.app.extensionManager && window.app.extensionManager.toast) {
                        window.app.extensionManager.toast.add({
                            severity: 'success',
                            summary: 'Workflow Loaded',
                            detail: 'Successfully loaded: ' + fileName,
                            life: 3000
                        });
                    } else {
                        // Fallback to alert for older versions
                        alert("Workflow Auto-Loaded Successfully!\n\n" + fileName);
                    }

                } catch (error) {
                    console.error("[OFX AutoLoader] Failed to load workflow:", error);

                    const errorMsg = "Failed to auto-load workflow \"" + fileName + "\":\n\n" +
                                   error.message + "\n\n" +
                                   "Please load the workflow manually using Ctrl+O or drag-and-drop.\n" +
                                   "Workflow location: ComfyUI/input/" + fileName;
                    alert(errorMsg);

                    // Still clean up the URL even on error
                    window.history.replaceState({}, document.title, "/");
                }
            }
        });

        console.log("[OFX AutoLoader] Extension registered successfully");
    }
})();
```

## Testing the Installation

### Test 1: Check Extension Loads

1. Restart ComfyUI server
2. Open http://localhost:8188
3. Open browser console (F12)
4. Look for: `[OFX AutoLoader] Extension loaded`

If you see this message, the extension is installed correctly!

### Test 2: Create a Test Workflow

1. Create a simple workflow file: `S:\002_COMFYUI\in\test_workflow.json`

```json
{
  "1": {
    "class_type": "LoadImage",
    "inputs": {
      "image": "example.png"
    }
  }
}
```

2. Open URL: `http://localhost:8188/?load_local_json=test_workflow.json`
3. Workflow should auto-load and URL parameter should disappear

## Common Issues

### Issue: "Cannot read properties of undefined (reading 'serialize')"

This error is from undoRedo.js, not our extension. It's unrelated to the auto-loader.

### Issue: No console messages at all

The extension file is not loading. Check:
1. File path: `ComfyUI/web/extensions/ofx_autoloader.js` (note plural 'extensions')
2. File permissions: Make sure it's readable
3. Server restart: Must restart ComfyUI server after adding extension

### Issue: "Failed to load module"

Your ComfyUI version is too old for ES6 imports. Use the compatibility version above.

### Issue: Extension loads but workflow doesn't load

Check:
1. Workflow file exists in the input directory: `S:\002_COMFYUI\in\`
2. Workflow file is valid JSON
3. URL parameter is correct: `?load_local_json=filename.json`
4. Browser console shows fetch errors

## Verification Checklist

- [ ] File is at: `ComfyUI/web/extensions/ofx_autoloader.js`
- [ ] File URL works: http://localhost:8188/extensions/ofx_autoloader.js
- [ ] ComfyUI server restarted after placing file
- [ ] Browser console shows: `[OFX AutoLoader] Extension loaded`
- [ ] Test URL loads: http://localhost:8188/?load_local_json=test.json

## Getting Help

If still not working, provide:
1. Exact file path where you placed ofx_autoloader.js
2. ComfyUI server startup logs
3. Browser console output (full)
4. Result of: http://localhost:8188/extensions/ofx_autoloader.js

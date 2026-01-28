# Extension Not Loading - Diagnostic Checklist

Run through these checks in order and report results:

## Check 1: File Location

On your ComfyUI server, run:

```bash
# Windows (PowerShell)
Get-ChildItem C:\path\to\ComfyUI\web\extensions\ofx_autoloader.js

# Or CMD
dir C:\path\to\ComfyUI\web\extensions\ofx_autoloader.js
```

**Expected**: File exists and shows file size
**If fails**: File is in wrong location

## Check 2: File is Being Served

Open this URL in your browser:
```
http://localhost:8188/extensions/ofx_autoloader.js
```

**Expected**: JavaScript code appears in browser
**If 404**: File not in correct location or wrong filename

## Check 3: Check ComfyUI Server Console

Look at the terminal/console where ComfyUI is running.

When ComfyUI starts, it should show:
```
Loading extensions...
```

**Question**: Do you see any mention of `ofx_autoloader.js` in the server console?

## Check 4: Browser Console - Load Errors

In browser console (F12), look for:
- Any messages containing "ofx" or "autoloader"
- Any JavaScript syntax errors
- Any "Failed to load module" errors

**Paste any relevant errors here**

## Check 5: Other Extensions Loading?

Your console shows other extensions loading (ComfyUI-N-Sidebar).

This proves extensions CAN load, so the issue is specific to our file.

## Check 6: Verify URL Parameter

When the plugin opens the browser, what's the exact URL?

**Expected**: `http://localhost:8188/?load_local_json=filename.json`

**Check**: Is the `?load_local_json=...` part present in the URL?

## Check 7: File Contents Verification

Run this to check first few lines:

```bash
# Windows
type C:\path\to\ComfyUI\web\extensions\ofx_autoloader.js | more

# Show first 5 lines
Get-Content C:\path\to\ComfyUI\web\extensions\ofx_autoloader.js | Select-Object -First 5
```

**Expected first line**: Either:
- `// Copyright OpenFX...` (standard version)
- `(function() {` (compatibility version)

## Likely Issues

### Issue A: Wrong Extension Directory

Some ComfyUI installations have extensions in different locations:
- `web/extensions/` (standard)
- `custom_nodes/` (older versions)
- `extensions/` (some forks)

**Try**: Search for where other extensions are located:
```bash
# Find ComfyUI-N-Sidebar extension
dir /s custom_templates.js
```

Then place `ofx_autoloader.js` in the SAME directory.

### Issue B: File Encoding

The file might have wrong encoding or line endings.

**Try**: Re-create the file using a simple text editor (not Word)

### Issue C: Extension System Disabled

Some ComfyUI configurations disable custom extensions.

**Check**: Look for `--disable-extensions` flag in your startup command

### Issue D: Server Caching

ComfyUI might be caching old extension list.

**Try**:
1. Stop ComfyUI
2. Delete cache: `rm -rf ComfyUI/__pycache__` or similar
3. Restart ComfyUI
4. Clear browser cache (Ctrl+Shift+R)

## Quick Test - Minimal Extension

Create this minimal test extension to verify extension system works:

**File: `ComfyUI/web/extensions/test_extension.js`**

```javascript
console.log("TEST EXTENSION LOADED!");
```

Restart ComfyUI and check browser console for "TEST EXTENSION LOADED!"

- ✓ If you see it: Extension system works, issue is with ofx_autoloader.js file
- ✗ If you don't: Extension system not working or file in wrong location

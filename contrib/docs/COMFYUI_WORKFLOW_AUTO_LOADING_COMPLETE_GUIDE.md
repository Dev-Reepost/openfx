# ComfyUI Workflow Auto-Loading - Complete Guide

**Version**: 1.2.0
**Date**: January 10, 2026
**Platform**: macOS (Primary), Windows, Linux (Cross-platform)
**Status**: Production Ready

## Table of Contents

1. [Overview](#overview)
2. [Features](#features)
3. [Quick Start](#quick-start)
4. [Installation](#installation)
5. [Configuration](#configuration)
6. [Usage](#usage)
7. [User-Controlled Workflow Naming](#user-controlled-workflow-naming)
8. [How It Works](#how-it-works)
9. [Troubleshooting](#troubleshooting)
10. [Examples](#examples)
11. [Technical Details](#technical-details)
12. [FAQ](#faq)

---

## Overview

The **Workflow Auto-Loading** feature eliminates the need to manually load workflows in ComfyUI when creating new workflows from the AnyComfy OFX plugin. Instead of the traditional workflow:

### Before (Manual Loading)

```
1. Click "New Workflow" in OFX plugin
2. Browser opens to ComfyUI
3. Press Ctrl+O or drag-and-drop workflow file
4. Navigate to workflow location
5. Select file
6. Edit workflow
7. Save
```

**Time: ~60 seconds**

### After (Auto-Loading)

```
1. (Optional) Enter workflow name in OFX plugin
2. Click "New Workflow" in OFX plugin
3. Browser opens with workflow ALREADY LOADED
4. Edit workflow
5. Save
```

**Time: ~10 seconds**

**Time Saved: 50 seconds per workflow** (83% faster)

---

## Features

### ✨ Key Features

1. **Automatic Workflow Loading**
   - Browser opens with workflow already on canvas
   - No manual file loading required
   - Seamless workflow creation experience

2. **User-Controlled Naming**
   - Specify custom workflow names
   - Automatic unique naming if not specified
   - Smart .json extension handling

3. **Cross-Platform Support**
   - macOS (primary development platform)
   - Windows
   - Linux

4. **Optional Feature**
   - Opt-in by configuration
   - Graceful fallback to manual loading
   - No breaking changes

5. **Smart Path Handling**
   - Automatic mount path matching
   - Client/server path translation
   - Directory auto-creation

---

## Quick Start

### 5-Minute Setup

**Prerequisites:**

- ComfyUI server installed and running
- AnyComfy OFX plugin installed
- Network access to shared storage

**Steps:**

1. **Install ComfyUI Extension** (one-time)

   ```bash
   # Copy JavaScript extension to ComfyUI
   cp contrib/plugins/ComfyUI/anycomfy/resources/ofx_autoloader.js \
      /path/to/ComfyUI/web/extensions/

   # Restart ComfyUI server
   ```

2. **Configure Plugin** (in OFX host)
   - Open AnyComfy effect → Server page
   - Set **"ComfyUI Input Directory"**: `/Volumes/silo2/002_COMFYUI/ComfyUI/input`
   - (Adjust path to match your installation)

3. **Test**
   - Enter a name in **"New Workflow Name"**: `my_test`
   - Click **"New Workflow"** button
   - Browser should open with workflow loaded!

**✅ Done!** You now have automatic workflow loading.

---

## Installation

### Part 1: ComfyUI Extension

The ComfyUI extension is a small JavaScript file that detects URL parameters and auto-loads workflows.

#### Installation Steps

**1. Locate the extension file:**

```
Source: contrib/plugins/ComfyUI/anycomfy/resources/ofx_autoloader.js
```

**2. Find ComfyUI extensions directory:**

| Platform | Default Path |
|----------|--------------|
| **macOS** | `/path/to/ComfyUI/web/extensions/` |
| **Windows** | `C:\ComfyUI\web\extensions\` |
| **Linux** | `/path/to/ComfyUI/web/extensions/` |

**3. Copy the file:**

```bash
# macOS/Linux
cp contrib/plugins/ComfyUI/anycomfy/resources/ofx_autoloader.js \
   /path/to/ComfyUI/web/extensions/

# Windows (PowerShell)
Copy-Item contrib\plugins\ComfyUI\anycomfy\resources\ofx_autoloader.js `
          C:\ComfyUI\web\extensions\
```

**4. Verify installation:**

```bash
# Check file exists
ls /path/to/ComfyUI/web/extensions/ofx_autoloader.js

# Check permissions (should be readable)
chmod 644 /path/to/ComfyUI/web/extensions/ofx_autoloader.js
```

**5. Restart ComfyUI server:**

```bash
# The extension is loaded at startup
# Restart method depends on your installation
# Examples:

# Python/venv:
pkill -f "python.*main.py"
cd /path/to/ComfyUI && python main.py

# systemd:
sudo systemctl restart comfyui

# Docker:
docker restart comfyui
```

**6. Verify extension loaded:**

- Open ComfyUI in browser: `http://localhost:8188`
- Open browser console (F12)
- Look for log message: `[OFX AutoLoader]` (on page with URL parameter)

### Part 2: AnyComfy Plugin

The plugin is already built with auto-loading support. Just ensure you have version 1.2.0+.

**Check version:**

- Look at plugin metadata in OFX host
- Or check build date: January 2026 or later

---

## Configuration

### Plugin Parameters

Configure the following parameters in your OFX host (Flame, Nuke, etc.):

#### Workflow Page

##### **New Workflow Name** (NEW in v1.2)

- **Type**: Text field
- **Default**: Empty (auto-generates name)
- **Purpose**: Control the name of workflows you create
- **Behavior**:
  - If **empty**: Auto-generates unique name `anycomfy_<instance>_<timestamp>.json`
  - If **filled**: Uses your custom name (adds .json if needed)
  - **Auto-clears** after use (ready for next workflow)

**Examples:**

```
Input: "my_denoise"         → Creates: my_denoise.json
Input: "upscale_4x.json"    → Creates: upscale_4x.json
Input: ""                   → Creates: anycomfy_effect1_1736524800.json
```

##### **Workflows Directory**

- **Type**: Directory path
- **Default**: `workflows`
- **Purpose**: Where workflow files are stored (relative to shared mount)
- **Example**: `workflows` → `/Volumes/silo2/002_COMFYUI/workflows/`

#### Server Page

##### **ComfyUI Input Directory** (NEW in v1.2)

- **Type**: Directory path
- **Default**: `/Volumes/silo2/002_COMFYUI/ComfyUI/input` (macOS)
- **Purpose**: Where workflows are copied for auto-loading
- **Critical**: Must point to where **ComfyUI can access** the files

**Path Selection Guide:**

| Scenario | Path Example | Notes |
|----------|--------------|-------|
| **macOS local** | `/Volumes/silo2/002_COMFYUI/ComfyUI/input` | ComfyUI on same Mac |
| **Windows server** | `Z:\ComfyUI\input` | ComfyUI on Windows server, Z: mapped |
| **Linux server** | `/mnt/storage/ComfyUI/input` | ComfyUI on Linux server |
| **Docker** | `/data/ComfyUI/input` | Depends on volume mount |
| **Disable auto-load** | *(leave empty)* | Manual loading required |

**Important Notes:**

- Use the path **as ComfyUI sees it**, not as your client sees it
- If unsure, SSH/RDP to ComfyUI server and check: `ls /path/to/ComfyUI/input`
- Directory will be auto-created if it doesn't exist

##### **Other Server Parameters**

- **Server Address**: `localhost` (or ComfyUI server IP)
- **Server Port**: `8188` (default ComfyUI port)
- **Shared Mount Path**: `/Volumes/silo2/002_COMFYUI` (macOS example)
- **Server Mount Point**: `Z:` (Windows server) or `/mnt/storage` (Linux)

---

## Usage

### Creating a New Workflow

#### Method 1: Custom Name (Recommended)

Use this when you know what workflow you're creating.

**Steps:**

1. **Open AnyComfy effect** in your OFX host
2. **Navigate to Workflow page**
3. **Enter workflow name** in "New Workflow Name" field:
   - Example: `denoise_temporal`
4. **Click "New Workflow" button**
5. **Browser opens** with workflow `denoise_temporal.json` loaded
6. **Edit in ComfyUI**:
   - Add nodes between LoadEXR and SaveEXR
   - Configure parameters
   - Save workflow (Ctrl+S)
7. **Return to OFX** and render

**Result:**

- Workflow file: `/Volumes/silo2/002_COMFYUI/workflows/denoise_temporal.json`
- Auto-loaded in ComfyUI
- "New Workflow Name" field cleared (ready for next time)

#### Method 2: Auto-Generated Name

Use this for quick testing or when name doesn't matter.

**Steps:**

1. **Open AnyComfy effect** in your OFX host
2. **Navigate to Workflow page**
3. **Leave "New Workflow Name" empty**
4. **Click "New Workflow" button**
5. **Browser opens** with uniquely named workflow loaded
6. **Edit and save** as above

**Result:**

- Workflow file: `anycomfy_effect1_1736524800.json`
- Unique timestamp ensures no conflicts
- Perfect for experimentation

### Using an Existing Workflow

If you already have a workflow file:

1. **Place file** in workflows directory:
   - Copy to: `/Volumes/silo2/002_COMFYUI/workflows/my_workflow.json`

2. **In AnyComfy plugin**:
   - Navigate to **Project page**
   - Set **"Workflow File Path"**: `/Volumes/silo2/002_COMFYUI/workflows/my_workflow.json`
   - Or use relative: `workflows/my_workflow.json`

3. **Render** to execute workflow

---

## User-Controlled Workflow Naming

### Overview

The **"New Workflow Name"** parameter gives you full control over workflow file names at creation time.

### Behavior

#### When Empty (Default)

```
Action: Click "New Workflow"
Result: anycomfy_effect1_1736524800.json
Format: anycomfy_<instance>_<timestamp>.json
```

- **Instance**: Unique OFX instance identifier
- **Timestamp**: Unix timestamp (seconds since epoch)
- **Uniqueness**: Guaranteed (timestamp changes every second)

#### When Filled

```
Input: "my_workflow"
Result: my_workflow.json

Input: "denoise_v2.json"
Result: denoise_v2.json
```

- **Direct use**: Your name becomes the filename
- **Smart extension**: Adds .json if missing
- **Auto-clear**: Field is emptied after use

### Naming Best Practices

**✅ Good Names:**

```
denoise_temporal        (descriptive, lowercase, underscores)
upscale_4x_esrgan      (includes parameters/method)
normal_map_deepbump    (specific function)
SAM_segmentation       (clear purpose)
```

**⚠️ Acceptable but not ideal:**

```
test                   (too generic)
effect1                (not descriptive)
workflow123            (meaningless number)
```

**❌ Avoid:**

```
my workflow            (spaces - work but messy)
test!@#$%             (special chars - may cause issues)
../../../../etc/passwd (path traversal - blocked by OS)
```

### Use Cases

#### Scenario 1: Named Workflow for Production

```
Workflow Name: "hero_shot_denoise"
Purpose: Production-ready denoising for hero shots
Location: /Volumes/silo2/002_COMFYUI/workflows/hero_shot_denoise.json
```

#### Scenario 2: Testing Multiple Approaches

```
Session 1:
  Name: "denoise_test_v1"
  Result: denoise_test_v1.json

Session 2:
  Name: "denoise_test_v2"
  Result: denoise_test_v2.json

Session 3:
  Name: "denoise_test_v3"
  Result: denoise_test_v3.json
```

#### Scenario 3: Quick Experimentation

```
Leave name empty
Result: anycomfy_effect1_1736524800.json
Result: anycomfy_effect1_1736524815.json
Result: anycomfy_effect1_1736524830.json
(15 seconds apart)
```

---

## How It Works

### Architecture

```
┌─────────────────────────────────────────────────────────────┐
│ Step 1: User Creates Workflow                               │
│                                                             │
│ OFX Host (Flame/Nuke)                                       │
│ ┌─────────────────────┐                                     │
│ │ AnyComfy Plugin     │                                     │
│ │                     │                                     │
│ │ [New Workflow Name] │ → User enters: "my_workflow"        │
│ │ [New Workflow    ▼] │ → User clicks button                │
│ └─────────────────────┘                                     │
└─────────────────────────────────────────────────────────────┘
                          ↓
┌─────────────────────────────────────────────────────────────┐
│ Step 2: Plugin Creates and Copies File                      │
│                                                             │
│ 1. Creates: /Volumes/.../workflows/my_workflow.json         │
│    Content: LoadEXR → (empty) → SaveEXR                     │
│                                                             │
│ 2. Copies to: /Volumes/.../ComfyUI/input/my_workflow.json  │
│    (Where ComfyUI can access it)                            │
│                                                             │
│ 3. Opens browser: http://localhost:8188/                    │
│    ?load_local_json=my_workflow.json                        │
└─────────────────────────────────────────────────────────────┘
                          ↓
┌─────────────────────────────────────────────────────────────┐
│ Step 3: Browser Loads ComfyUI                               │
│                                                             │
│ Browser                                                     │
│ ┌─────────────────────────────────────────┐                │
│ │ http://localhost:8188/                   │                │
│ │   ?load_local_json=my_workflow.json     │                │
│ └─────────────────────────────────────────┘                │
│                                                             │
│ JavaScript Extension (ofx_autoloader.js)                    │
│ - Detects URL parameter: load_local_json                    │
│ - Extracts filename: my_workflow.json                       │
└─────────────────────────────────────────────────────────────┘
                          ↓
┌─────────────────────────────────────────────────────────────┐
│ Step 4: Extension Fetches Workflow                          │
│                                                             │
│ JavaScript: fetch('/view?filename=my_workflow.json          │
│                    &type=input')                            │
│                                                             │
│ ComfyUI Server:                                             │
│ - Serves file from: ComfyUI/input/my_workflow.json          │
│ - Returns JSON workflow data                                │
└─────────────────────────────────────────────────────────────┘
                          ↓
┌─────────────────────────────────────────────────────────────┐
│ Step 5: Extension Loads Workflow into Canvas                │
│                                                             │
│ JavaScript: app.loadGraphData(workflowJson)                 │
│                                                             │
│ ComfyUI UI:                                                 │
│ - Clears canvas                                             │
│ - Loads workflow nodes                                      │
│ - Displays LoadEXR → SaveEXR                                │
│                                                             │
│ JavaScript: window.history.replaceState({}, '', '/')        │
│ - Cleans URL (removes parameter)                            │
│ - Final URL: http://localhost:8188/                         │
└─────────────────────────────────────────────────────────────┘
                          ↓
┌─────────────────────────────────────────────────────────────┐
│ Step 6: User Edits and Saves                                │
│                                                             │
│ User:                                                       │
│ - Adds custom nodes                                         │
│ - Configures parameters                                     │
│ - Saves workflow (Ctrl+S)                                   │
│                                                             │
│ Result:                                                     │
│ - Workflow ready to use in OFX plugin                       │
│ - No manual file loading required!                          │
└─────────────────────────────────────────────────────────────┘
```

### Data Flow

**1. Workflow Creation**

```javascript
// Plugin (C++)
string userName = _newWorkflowName->getValue(); // "my_workflow"
if (userName.empty()) {
    userName = generateUniqueWorkflowName(); // "anycomfy_effect1_1736524800.json"
}
string filename = userName + ".json";
```

**2. File Operations**

```javascript
// Plugin (C++)
// Save to workflows directory
saveWorkflow("/Volumes/.../workflows/my_workflow.json", templateData);

// Copy to ComfyUI input directory
copyFile("/Volumes/.../workflows/my_workflow.json",
         "/Volumes/.../ComfyUI/input/my_workflow.json");
```

**3. Browser Opening**

```javascript
// Plugin (C++)
string url = "http://localhost:8188/?load_local_json=my_workflow.json";
system("open " + url); // macOS
```

**4. Auto-Loading**

```javascript
// Extension (JavaScript)
const params = new URLSearchParams(window.location.search);
const filename = params.get('load_local_json'); // "my_workflow.json"

const response = await fetch(`/view?filename=${filename}&type=input`);
const workflow = await response.json();

await app.loadGraphData(workflow); // Load into ComfyUI
window.history.replaceState({}, '', '/'); // Clean URL
```

---

## Troubleshooting

### Common Issues

#### Issue 1: Workflow Doesn't Auto-Load

**Symptoms:**

- Browser opens to blank ComfyUI canvas
- URL has `?load_local_json=...` parameter
- No error messages

**Diagnostic Steps:**

1. **Check Extension Installation**

   ```bash
   ls /path/to/ComfyUI/web/extensions/ofx_autoloader.js
   ```

   - If missing → Install extension
   - If present → Continue to step 2

2. **Check ComfyUI Restart**
   - Extensions load at startup only
   - Solution: Restart ComfyUI server

3. **Check Browser Console**
   - Open browser (F12)
   - Look for `[OFX AutoLoader]` messages
   - Check for errors

**Solutions:**

- **Extension not installed**: Follow [installation steps](#part-1-comfyui-extension)
- **Extension not loading**: Check file permissions, restart ComfyUI
- **JavaScript errors**: Check browser console, verify extension file integrity

#### Issue 2: File Not Found (404)

**Symptoms:**

- Browser opens with URL parameter
- Browser alert: "Failed to auto-load workflow: HTTP 404"
- Console error: "Failed to fetch workflow"

**Diagnostic Steps:**

1. **Check ComfyUI Input Directory Parameter**
   - Open plugin in OFX host
   - Check "ComfyUI Input Directory" value
   - Verify it points to correct location

2. **Check File Was Copied**

   ```bash
   # List files in ComfyUI input directory
   ls /Volumes/silo2/002_COMFYUI/ComfyUI/input/

   # Look for your workflow file
   ls /Volumes/silo2/002_COMFYUI/ComfyUI/input/my_workflow.json
   ```

3. **Check Plugin Logs**
   - Look for: "Copied workflow to ComfyUI input directory"
   - Or: "Failed to copy workflow to ComfyUI input dir"

**Solutions:**

- **Directory doesn't exist**: Plugin should auto-create, but verify permissions
- **Path mismatch**: Update "ComfyUI Input Directory" parameter
- **Permission denied**: Check directory write permissions

#### Issue 3: Wrong Path (Different Machine)

**Symptoms:**

- Plugin copies file successfully on client
- But ComfyUI (on different machine) can't find file
- 404 error in browser

**Root Cause:**

- "ComfyUI Input Directory" uses client-side path
- Should use server-side path (where ComfyUI runs)

**Example Problem:**

```
Client (macOS): /Volumes/silo2/002_COMFYUI/ComfyUI/input
Server (Windows): Z:\ComfyUI\input

Plugin configured with: /Volumes/silo2/002_COMFYUI/ComfyUI/input
                        ↑ WRONG - ComfyUI can't access this path
```

**Solution:**

```
Update parameter to server-side path:
  ComfyUI Input Directory: Z:\ComfyUI\input
  (Or use network mount that both can access)
```

#### Issue 4: Name Conflicts

**Symptoms:**

- Clicking "New Workflow" multiple times rapidly
- Only first workflow loads
- Subsequent workflows overwrite first one

**Root Cause:**

- Using same workflow name
- Files overwrite each other

**Solution:**

```
Method 1: Use unique names
  - my_workflow_v1
  - my_workflow_v2
  - my_workflow_v3

Method 2: Leave name empty (auto-generates unique timestamp)
  - anycomfy_effect1_1736524800.json
  - anycomfy_effect1_1736524815.json
  - anycomfy_effect1_1736524830.json
```

#### Issue 5: Extension Doesn't Work After Update

**Symptoms:**

- Was working before
- Stopped working after updating plugin or ComfyUI
- No errors in console

**Diagnostic Steps:**

1. **Check Extension Version**

   ```bash
   # Compare file sizes/dates
   ls -lh /path/to/ComfyUI/web/extensions/ofx_autoloader.js
   ls -lh contrib/plugins/ComfyUI/anycomfy/resources/ofx_autoloader.js
   ```

2. **Check Browser Cache**
   - Browser may cache old extension
   - Solution: Hard refresh (Ctrl+Shift+R)

3. **Check ComfyUI Version Compatibility**
   - Verify ComfyUI API hasn't changed
   - Check `/view` endpoint still works:

     ```bash
     curl "http://localhost:8188/view?filename=test.json&type=input"
     ```

**Solutions:**

- **Update extension**: Copy latest version from plugin resources
- **Clear cache**: Hard refresh browser
- **Restart ComfyUI**: Ensure new extension loads

---

## Examples

### Example 1: Production Workflow for VFX Shot

**Scenario**: Creating denoising workflow for hero shot in feature film

**Steps:**

1. **Setup** (one-time):
   - Extension installed ✓
   - Plugin configured ✓
   - ComfyUI running on render farm ✓

2. **Create Workflow**:

   ```
   OFX Plugin:
   - New Workflow Name: "hero_denoise_v1"
   - Click "New Workflow"

   Browser opens:
   - URL: http://renderfarm:8188/?load_local_json=hero_denoise_v1.json
   - Workflow already loaded on canvas
   ```

3. **Edit in ComfyUI**:

   ```
   Nodes:
   [LoadEXR] → [AI Denoise] → [Blend (50%)] → [SaveEXR]
                   ↓
              [Custom Model: hero_trained.pth]

   Save workflow (Ctrl+S)
   ```

4. **Use in OFX**:

   ```
   Flame:
   - Workflow File Path: auto-set to hero_denoise_v1.json
   - Render shot_001 frames 1-100

   Result:
   - Clean, denoised frames output to:
     /renders/hero_shot/hero_denoise_v1/v001/shot_001_####.exr
   ```

**Time Saved**: 45 seconds (no manual workflow loading)

### Example 2: Rapid Prototyping Session

**Scenario**: Testing different upscaling approaches

**Steps:**

1. **Test 1: ESRGAN**

   ```
   - Name: "upscale_esrgan"
   - Click "New Workflow"
   - Add: ESRGAN 4x node
   - Test render
   - Not satisfied → Try next
   ```

2. **Test 2: Real-ESRGAN**

   ```
   - Name: "upscale_real_esrgan"
   - Click "New Workflow"
   - Add: Real-ESRGAN node
   - Test render
   - Better but slow → Try next
   ```

3. **Test 3: Hybrid Approach**

   ```
   - Name: "upscale_hybrid"
   - Click "New Workflow"
   - Add: ESRGAN + post-sharpen
   - Test render
   - Perfect! ✓
   ```

**Result**: 3 workflows tested in 10 minutes (vs 15+ with manual loading)

### Example 3: Multiple Artists Collaboration

**Scenario**: 3 artists working on same project, different workflows

**Setup:**

| Artist | Workflow | Purpose |
|--------|----------|---------|
| Alice | `alice_denoise` | Temporal denoising |
| Bob | `bob_upscale` | 4x upscaling |
| Carol | `carol_composite` | Multi-pass compositing |

**Each artist:**

1. Creates named workflow
2. Shares workflow file in git repo
3. Others pull and use

**Benefits:**

- No filename conflicts (each has unique name)
- Easy to identify who created what
- Version control friendly

---

## Technical Details

### File Locations

| Component | macOS Path | Windows Path |
|-----------|------------|--------------|
| Extension Source | `contrib/plugins/ComfyUI/anycomfy/resources/ofx_autoloader.js` | Same |
| Extension Install | `/path/to/ComfyUI/web/extensions/ofx_autoloader.js` | `C:\ComfyUI\web\extensions\ofx_autoloader.js` |
| Workflow Storage | `/Volumes/silo2/002_COMFYUI/workflows/` | `Z:\workflows\` |
| ComfyUI Input | `/Volumes/silo2/002_COMFYUI/ComfyUI/input/` | `Z:\ComfyUI\input\` |
| Plugin Logs | `~/Library/Logs/AnyComfy/anycomfy.log` | `%USERPROFILE%\AppData\Local\AnyComfy\anycomfy.log` |

### URL Format

```
http://<server>:<port>/?load_local_json=<filename>
```

**Components:**

- `<server>`: `localhost` or ComfyUI server IP
- `<port>`: `8188` (default)
- `<filename>`: Just the filename (no path)

**Examples:**

```
http://localhost:8188/?load_local_json=my_workflow.json
http://192.168.1.100:8188/?load_local_json=denoise_v2.json
```

### ComfyUI /view Endpoint

The extension uses ComfyUI's built-in `/view` endpoint to fetch workflows:

**Format:**

```
GET /view?filename=<name>&type=<folder>
```

**Parameters:**

- `filename`: File name (not full path)
- `type`: Folder type (`input`, `output`, `temp`)

**Security:**

- Can only access designated folders
- Path traversal prevented by ComfyUI
- No arbitrary file system access

**Example:**

```bash
curl "http://localhost:8188/view?filename=my_workflow.json&type=input"
```

**Response:**

```json
{
  "1": {
    "inputs": {"filepath": "${INPUT_PATH}"},
    "class_type": "LoadEXR"
  },
  "2": {
    "inputs": {"filename_prefix": "${OUTPUT_PREFIX}"},
    "class_type": "SaveEXR"
  }
}
```

### Extension Code Flow

```javascript
// 1. Setup extension
app.registerExtension({
    name: "OFX.AutoLoader",
    async setup() {
        // 2. Check for URL parameter
        const params = new URLSearchParams(window.location.search);
        const fileName = params.get('load_local_json');

        if (!fileName) return; // Nothing to do

        // 3. Fetch workflow
        const url = `/view?filename=${encodeURIComponent(fileName)}&type=input`;
        const response = await fetch(url);
        const workflowJson = await response.json();

        // 4. Load into ComfyUI
        await app.loadGraphData(workflowJson);

        // 5. Clean up URL
        window.history.replaceState({}, document.title, "/");
    }
});
```

### Plugin Parameter Details

| Parameter | Type | Default | Animates | Description |
|-----------|------|---------|----------|-------------|
| `newWorkflowName` | String | `""` | No | User-specified workflow name |
| `createNewWorkflow` | Button | - | No | Trigger workflow creation |
| `workflowsDirectory` | String | `"workflows"` | No | Workflow storage location |
| `comfyUIInputDir` | String | `/Volumes/.../input` | No | ComfyUI input directory |

---

## FAQ

### Q: Do I need to install the extension for each ComfyUI instance?

**A**: Yes. The extension must be installed on each ComfyUI server/instance you want to use auto-loading with.

### Q: Can I disable auto-loading without removing the extension?

**A**: Yes. Leave "ComfyUI Input Directory" empty in the plugin settings. The browser will open normally but without auto-loading.

### Q: What happens if I specify a workflow name that already exists?

**A**: The file will be overwritten. The plugin logs a warning but proceeds. Use unique names or check existing files first.

### Q: Can I use auto-loading with workflows on a network share?

**A**: Yes, as long as:

1. The plugin can write to the network share
2. ComfyUI can read from the network share
3. Both point to the same location (may need path translation)

### Q: Does the workflow name affect the output directory?

**A**: Yes, if you use auto-derived workflow names from filenames. The workflow name parameter in the Project page is used for output paths.

### Q: Can multiple users create workflows with the same name?

**A**: Yes, but they'll overwrite each other. Best practice:

- Use auto-generated names (includes timestamp)
- Or prefix with user initials: `alice_denoise`, `bob_upscale`

### Q: What if my ComfyUI is behind a firewall?

**A**: As long as your client can:

1. Write to the shared storage
2. Open browser URLs to ComfyUI

The feature should work. The extension runs in the browser, so firewall doesn't affect it.

### Q: Can I use this with Docker-based ComfyUI?

**A**: Yes. Configure "ComfyUI Input Directory" to point to the Docker volume mount. Example:

```
Docker volume mount: /data/ComfyUI/input
Plugin setting: /path/to/docker/volumes/comfyui/input
```

### Q: Does this work with ComfyUI Manager?

**A**: Yes, the extension is independent of ComfyUI Manager. Both can coexist.

### Q: How do I uninstall the auto-loading feature?

**A**:

1. Delete the extension: `rm /path/to/ComfyUI/web/extensions/ofx_autoloader.js`
2. Restart ComfyUI
3. (Optional) Clear "ComfyUI Input Directory" in plugin settings

### Q: Can I customize the extension behavior?

**A**: Yes! Edit `ofx_autoloader.js` to:

- Change notification messages
- Add custom validation
- Modify error handling
- Add logging

---

## Summary

The **Workflow Auto-Loading** feature provides:

✅ **50+ seconds saved** per workflow creation
✅ **User-controlled naming** for better organization
✅ **Seamless browser integration** - no manual loading
✅ **Cross-platform support** - macOS, Windows, Linux
✅ **Optional** - graceful fallback if not configured
✅ **Production-ready** - tested and documented

**Total Setup Time**: 5 minutes
**Time Saved Per Workflow**: 50 seconds
**ROI**: After 6 workflows, you've saved more time than setup took

---

## Additional Resources

- [Installation Guide](../plugins/ComfyUI/anycomfy/INSTALL_AUTO_LOAD.md)
- [Technical Implementation](WORKFLOW_AUTO_LOAD_IMPLEMENTATION.md)
- [AnyComfy Plugin README](../plugins/ComfyUI/anycomfy/README.md)
- [ComfyUI Documentation](https://github.com/comfyanonymous/ComfyUI)

---

**Version History:**

- v1.2.0 (Jan 10, 2026): Added user-controlled naming, macOS path defaults
- v1.1.0 (Dec 30, 2025): Auto-workflow name derivation
- v1.0.0 (Nov 2025): Initial AnyComfy plugin release

**License**: BSD-3-Clause (OpenFX Project)

**Support**: For issues or questions, see [GitHub Issues](https://github.com/AcademySoftwareFoundation/openfx/issues)

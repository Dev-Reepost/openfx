# ComfyUI Custom Directories Setup Guide

**For installations using `--input-directory` and `--output-directory` flags**

## Your Current Setup

### Server Side (Windows)

ComfyUI is started with custom directories on the LAN storage:

```batch
C:\Users\reepost\AppData\Local\Programs\Python\Python310\python.exe main.py ^
  --listen ^
  --force-fp32 ^
  --fp32-vae ^
  --input-directory S:\002_COMFYUI\in ^
  --output-directory S:\002_COMFYUI\out
```

**Key points**:
- **S: drive** is mapped to LAN storage (`\\server\002_COMFYUI`)
- **Input directory**: `S:\002_COMFYUI\in` (custom, not default `ComfyUI/input`)
- **Output directory**: `S:\002_COMFYUI\out` (custom, not default `ComfyUI/output`)

### Client Side (macOS)

The same LAN storage is mounted at:
```
/Volumes/silo2/002_COMFYUI
```

**Equivalent paths**:
- Windows `S:\002_COMFYUI\in` = macOS `/Volumes/silo2/002_COMFYUI/in`
- Windows `S:\002_COMFYUI\out` = macOS `/Volumes/silo2/002_COMFYUI/out`

## Directory Structure on LAN

```
S:\ (or /Volumes/silo2/002_COMFYUI)
├── in/                      ← ComfyUI --input-directory
│   ├── (workflow files copied here for auto-loading)
│   └── (input images)
├── out/                     ← ComfyUI --output-directory
│   └── (processed images from ComfyUI)
└── workflows/               ← AnyComfy workflow storage
    ├── my_denoise.json
    ├── upscale_4x.json
    └── ...
```

## Plugin Configuration

### AnyComfy Plugin Parameters

In your OFX host (Flame), configure:

#### Server Page
```
Server Address: 192.168.x.x (or localhost if on same machine)
Server Port: 8188
Shared Mount Path: /Volumes/silo2/002_COMFYUI
Server Mount Point: S:
ComfyUI Input Directory: /Volumes/silo2/002_COMFYUI/in  ← CRITICAL
```

#### Workflow Page
```
New Workflow Name: (optional, e.g., "my_workflow")
Workflows Directory: workflows
```

## How It Works

### Workflow Creation Flow

```
1. User Action (macOS):
   - New Workflow Name: "my_denoise"
   - Click "New Workflow"

2. Plugin Creates Workflow:
   - Saves to: /Volumes/silo2/002_COMFYUI/workflows/my_denoise.json
   - Content: LoadEXR → (empty) → SaveEXR template

3. Plugin Copies for Auto-Loading:
   - From: /Volumes/silo2/002_COMFYUI/workflows/my_denoise.json
   - To:   /Volumes/silo2/002_COMFYUI/in/my_denoise.json

   (This is the directory ComfyUI monitors with --input-directory)

4. Plugin Opens Browser:
   - URL: http://192.168.x.x:8188/?load_local_json=my_denoise.json

5. ComfyUI Extension (JavaScript):
   - Fetches: GET /view?filename=my_denoise.json&type=input
   - ComfyUI serves from: S:\002_COMFYUI\in\my_denoise.json
   - Loads into canvas
   - User edits and saves

6. Workflow Ready:
   - Original stored in: workflows/my_denoise.json
   - Copy for loading in: in/my_denoise.json
   - Both accessible to plugin and ComfyUI
```

## Why This Setup Works

### Custom Input Directory Benefits

1. **Centralized Storage**
   - All workflow files on LAN
   - Accessible to all machines
   - No local copies needed

2. **ComfyUI /view Endpoint**
   - Serves files from custom `--input-directory`
   - Browser fetches via: `/view?filename=X&type=input`
   - Works seamlessly with custom paths

3. **Path Consistency**
   - Plugin and ComfyUI reference same physical location
   - No path translation issues
   - Reliable file access

## Common Scenarios

### Scenario 1: Creating Workflow on macOS, Running on Windows Server

**Setup**:
- User on macOS with Flame
- ComfyUI on Windows server
- Shared storage: `S:\` (Windows) = `/Volumes/silo2/002_COMFYUI` (macOS)

**Steps**:
1. User creates workflow on macOS:
   ```
   Plugin saves: /Volumes/silo2/002_COMFYUI/workflows/denoise.json
   Plugin copies: /Volumes/silo2/002_COMFYUI/in/denoise.json
   ```

2. Browser opens to Windows server:
   ```
   URL: http://server:8188/?load_local_json=denoise.json
   ```

3. ComfyUI on Windows serves:
   ```
   GET /view?filename=denoise.json&type=input
   → Serves: S:\002_COMFYUI\in\denoise.json
   ```

4. Extension loads workflow:
   ```
   JavaScript: app.loadGraphData(workflowJson)
   ```

**Result**: ✅ Works because both client and server access same file through network mount

### Scenario 2: Multiple Users, Shared Workflows

**Setup**:
- Alice on macOS workstation 1
- Bob on macOS workstation 2
- Carol on Windows workstation 3
- All access same ComfyUI server
- All share `S:\002_COMFYUI`

**Workflow**:
1. Alice creates `alice_denoise.json`:
   ```
   Saves to: /Volumes/silo2/002_COMFYUI/workflows/alice_denoise.json
   ```

2. Bob wants to use Alice's workflow:
   ```
   In OFX:
   - Workflow File Path: /Volumes/silo2/002_COMFYUI/workflows/alice_denoise.json
   - Renders frames
   ```

3. Carol also uses it (from Windows):
   ```
   In OFX:
   - Workflow File Path: S:\002_COMFYUI\workflows\alice_denoise.json
   - Renders frames
   ```

**Result**: ✅ Team collaboration enabled through shared storage

## Installation Checklist

### One-Time Setup

- [ ] **ComfyUI Server** running with correct flags:
  ```batch
  --input-directory S:\002_COMFYUI\in
  --output-directory S:\002_COMFYUI\out
  ```

- [ ] **Install Extension** on ComfyUI server:
  ```bash
  # Copy to ComfyUI installation
  cp ofx_autoloader.js C:\path\to\ComfyUI\web\extensions\
  # Restart ComfyUI
  ```

- [ ] **Verify LAN Mount** on macOS:
  ```bash
  ls /Volumes/silo2/002_COMFYUI/in
  ls /Volumes/silo2/002_COMFYUI/out
  ls /Volumes/silo2/002_COMFYUI/workflows
  ```

- [ ] **Configure Plugin** in Flame:
  ```
  ComfyUI Input Directory: /Volumes/silo2/002_COMFYUI/in
  ```

### Test Procedure

1. **Create Test Workflow**:
   ```
   New Workflow Name: "test_autoload"
   Click "New Workflow"
   ```

2. **Verify Files Created**:
   ```bash
   # On macOS
   ls /Volumes/silo2/002_COMFYUI/workflows/test_autoload.json  # Original
   ls /Volumes/silo2/002_COMFYUI/in/test_autoload.json         # Copy for loading

   # On Windows server
   dir S:\002_COMFYUI\workflows\test_autoload.json
   dir S:\002_COMFYUI\in\test_autoload.json
   ```

3. **Verify Browser Opens**:
   ```
   URL should be: http://server:8188/?load_local_json=test_autoload.json
   ```

4. **Verify Workflow Loads**:
   ```
   - Browser should show ComfyUI with workflow on canvas
   - Should see LoadEXR and SaveEXR nodes
   - Console (F12) should show: "[OFX AutoLoader] Workflow loaded successfully"
   ```

5. **Verify Editing Works**:
   ```
   - Add a node in ComfyUI
   - Save (Ctrl+S)
   - Close browser
   - Reopen workflow → Changes should persist
   ```

## Troubleshooting

### Issue: File Not Found (404)

**Symptom**: Browser alert "Failed to auto-load workflow: HTTP 404"

**Diagnosis**:
```bash
# On macOS - check file exists
ls /Volumes/silo2/002_COMFYUI/in/my_workflow.json

# On Windows server - check file exists
dir S:\002_COMFYUI\in\my_workflow.json
```

**Common Causes**:
1. **Wrong input directory** in plugin settings
   - Check: "ComfyUI Input Directory" = `/Volumes/silo2/002_COMFYUI/in`
   - NOT: `/Volumes/silo2/002_COMFYUI/ComfyUI/input`

2. **ComfyUI started without --input-directory flag**
   - Default location: `ComfyUI/input/`
   - Custom location: `S:\002_COMFYUI\in`
   - Verify ComfyUI startup command

3. **Network mount not accessible**
   - Test: `ls /Volumes/silo2/002_COMFYUI/in`
   - If fails: Check network connection, mount permissions

### Issue: Workflow Loads But Can't Process Images

**Symptom**: Workflow loads, but errors when rendering frames

**Diagnosis**:
Check input/output paths in the workflow:
```json
{
  "1": {
    "inputs": {
      "filepath": "${INPUT_PATH}"  // Should expand to LAN path
    }
  },
  "2": {
    "inputs": {
      "filename_prefix": "${OUTPUT_PREFIX}"  // Should expand to LAN path
    }
  }
}
```

**Solutions**:
1. **Verify base plugin parameters**:
   ```
   Shared Mount Path: /Volumes/silo2/002_COMFYUI
   Server Mount Point: S:
   ```

2. **Check path substitution** in plugin logs:
   ```
   [info] Injecting input path (raw Windows): S:\002_COMFYUI\project\inputs\frame_0001.exr
   [info] Injecting output prefix (raw Windows): S:\002_COMFYUI\out\project\workflow\v001\basename
   ```

3. **Verify ComfyUI can access files**:
   ```batch
   # On Windows server, test file access
   type S:\002_COMFYUI\project\inputs\frame_0001.exr
   ```

### Issue: Multiple Workflows Overwriting Each Other

**Symptom**: Creating new workflow overwrites previous one

**Cause**: Using same workflow name

**Solution**: Use unique names or leave empty for auto-generation
```
User 1: "denoise_v1" → denoise_v1.json
User 2: "upscale_4x" → upscale_4x.json
User 3: (empty)      → anycomfy_effect1_1736524800.json (unique timestamp)
```

## Best Practices

### Naming Conventions

```
Good:
- project_shot_workflow.json (descriptive)
- denoise_temporal_v2.json (versioned)
- hero_compositing.json (clear purpose)

Avoid:
- test.json (too generic)
- 123.json (not descriptive)
- FINAL_FINAL_v3.json (confusing)
```

### Directory Organization

```
S:\002_COMFYUI\
├── in/                    ← Keep clean, auto-loading only
│   └── (temp workflow files for loading)
├── out/                   ← ComfyUI outputs
│   └── project/
│       └── workflow/
│           └── v001/
│               └── frame_####.exr
└── workflows/             ← Permanent workflow storage
    ├── production/
    │   ├── denoise.json
    │   └── upscale.json
    ├── test/
    │   └── experimental.json
    └── archive/
        └── old_workflow.json
```

### Team Workflow

1. **Create** workflows with descriptive names
2. **Store** in `workflows/` directory (version controlled)
3. **Share** workflow files with team
4. **Reference** by path in OFX plugin
5. **Version** important workflows (v1, v2, etc.)

## Summary

### Correct Configuration

```
ComfyUI Server (Windows):
  Start with: --input-directory S:\002_COMFYUI\in

AnyComfy Plugin (macOS):
  Set to: /Volumes/silo2/002_COMFYUI/in

Result: Same physical directory, different path representation
```

### Key Differences from Default Setup

| Aspect | Default ComfyUI | Your Custom Setup |
|--------|----------------|-------------------|
| **Input Dir** | `ComfyUI/input/` | `S:\002_COMFYUI\in` |
| **Output Dir** | `ComfyUI/output/` | `S:\002_COMFYUI\out` |
| **Location** | Local to ComfyUI | Centralized LAN storage |
| **Access** | Single machine | Multi-user shared |
| **Plugin Path** | Local paths | Network mount paths |

### Why This Matters

- ✅ **Centralized**: All files on LAN, accessible to all
- ✅ **Consistent**: Same files for ComfyUI and plugin
- ✅ **Scalable**: Multiple users, multiple machines
- ✅ **Reliable**: No local copies, single source of truth

---

**Documentation Version**: 1.2.1
**Date**: January 10, 2026
**Applies To**: Custom ComfyUI installations with `--input-directory` and `--output-directory` flags

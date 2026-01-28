# Workflow Auto-Loading - Quick Reference

**Version**: 1.2.0 | **Date**: January 10, 2026 | **Status**: Production Ready

---

## Your Setup

```
macOS Client: /Volumes/silo2/002_COMFYUI
Windows Server: S:\002_COMFYUI
ComfyUI Command: --input-directory S:\002_COMFYUI\in
```

---

## Plugin Configuration (Defaults Already Set)

```
✓ Server Address: 192.168.x.x
✓ Server Port: 8188
✓ Shared Mount Path: /Volumes/silo2/002_COMFYUI
✓ Server Mount Point: S:
✓ ComfyUI Input Directory: /Volumes/silo2/002_COMFYUI/in
```

---

## Usage

### Create Workflow

**Simple (Auto-generated name)**:
```
1. Leave "New Workflow Name" empty
2. Click "New Workflow"
→ Creates: anycomfy_effect1_1736524800.json
```

**Custom Name**:
```
1. New Workflow Name: "my_workflow"
2. Click "New Workflow"
→ Creates: my_workflow.json
```

### Organize by Type (Recommended)

```
Workflows Directory: workflows/segmentation_segment_anything/api
New Workflow Name: person_seg
→ Creates: workflows/segmentation_segment_anything/api/person_seg.json
```

---

## Directory Structure

```
/Volumes/silo2/002_COMFYUI/
├── in/                    ← Auto-loading copies
├── out/                   ← Processed outputs
├── workflows/             ← Permanent storage
│   ├── segmentation_segment_anything/api/
│   ├── normal_map_deepbump/api/
│   ├── stable_diffusion/api/
│   └── zdepth_depth_anything/api/
└── models/                ← AI models
```

---

## Workflow Patterns

### Pattern 1: Generic Workflow

```
[Workflow Page]
Workflows Directory: workflows
New Workflow Name: my_test

Result: /Volumes/silo2/002_COMFYUI/workflows/my_test.json
```

### Pattern 2: Type-Organized (Your Current Pattern)

```
[Workflow Page]
Workflows Directory: workflows/segmentation_segment_anything/api
New Workflow Name: character_seg

Result: /Volumes/silo2/002_COMFYUI/workflows/segmentation_segment_anything/api/character_seg.json
```

### Pattern 3: Project-Specific

```
[Workflow Page]
Workflows Directory: workflows/segmentation_segment_anything/api
New Workflow Name: pitch_character_seg

Result: /Volumes/silo2/002_COMFYUI/workflows/segmentation_segment_anything/api/pitch_character_seg.json
```

---

## Path Mappings

| Component | macOS | Windows |
|-----------|-------|---------|
| **Input Dir** | `/Volumes/silo2/002_COMFYUI/in` | `S:\002_COMFYUI\in` |
| **Output Dir** | `/Volumes/silo2/002_COMFYUI/out` | `S:\002_COMFYUI\out` |
| **Workflows** | `/Volumes/silo2/002_COMFYUI/workflows` | `S:\002_COMFYUI\workflows` |

---

## Installation (One-Time Setup)

### 1. Install ComfyUI Extension

```bash
# Copy extension to ComfyUI
cp contrib/plugins/ComfyUI/anycomfy/resources/ofx_autoloader.js \
   C:\path\to\ComfyUI\web\extensions\

# Restart ComfyUI server
```

### 2. Verify Configuration

Plugin defaults are already set correctly for your environment. Just verify:

```
✓ ComfyUI Input Directory: /Volumes/silo2/002_COMFYUI/in
```

### 3. Test

```
1. New Workflow Name: "test_autoload"
2. Click "New Workflow"
3. Browser opens with workflow loaded ✓
```

---

## Troubleshooting

### Workflow doesn't auto-load

**Check**:
1. Extension installed? `ls C:\ComfyUI\web\extensions\ofx_autoloader.js`
2. ComfyUI restarted?
3. Browser console (F12) shows `[OFX AutoLoader]` messages?

**Fix**: Install extension, restart ComfyUI

### File not found (404)

**Check**:
1. File exists? `ls /Volumes/silo2/002_COMFYUI/in/my_workflow.json`
2. Path correct? ComfyUI Input Directory = `/Volumes/silo2/002_COMFYUI/in`

**Fix**: Verify path matches `--input-directory` flag

### Multiple workflows overwrite

**Cause**: Using same name

**Fix**: Use unique names or leave empty for auto-generated timestamp

---

## Common Commands

### Check Directories

```bash
# macOS
ls /Volumes/silo2/002_COMFYUI/in
ls /Volumes/silo2/002_COMFYUI/workflows/segmentation_segment_anything/api

# Windows
dir S:\002_COMFYUI\in
dir S:\002_COMFYUI\workflows\segmentation_segment_anything\api
```

### Verify File Created

```bash
# After creating workflow "test"
ls /Volumes/silo2/002_COMFYUI/workflows/test.json         # Original
ls /Volumes/silo2/002_COMFYUI/in/test.json                # Copy for auto-loading
```

### Check ComfyUI Server

```bash
# Test connection
curl http://192.168.x.x:8188/

# Test file serving
curl "http://192.168.x.x:8188/view?filename=test.json&type=input"
```

---

## Documentation Links

| Document | Purpose |
|----------|---------|
| **[Complete Guide](COMFYUI_WORKFLOW_AUTO_LOADING_COMPLETE_GUIDE.md)** | Comprehensive documentation (1200+ lines) |
| **[Actual Directory Structure](ACTUAL_DIRECTORY_STRUCTURE.md)** | Your production environment details |
| **[Custom Directories Setup](COMFYUI_CUSTOM_DIRECTORIES_SETUP.md)** | Guide for `--input-directory` flag |
| **[Quick Install](../plugins/ComfyUI/anycomfy/INSTALL_AUTO_LOAD.md)** | 5-minute installation guide |
| **[Implementation Details](WORKFLOW_AUTO_LOAD_IMPLEMENTATION.md)** | Technical implementation (for developers) |

---

## Project-Specific Examples

### Example 1: 1858_PITCH Segmentation

```
[Plugin Settings]
Workflows Directory: workflows/segmentation_segment_anything/api
New Workflow Name: pitch_character_seg
Project Name: 1858_PITCH
Workflow Name: segmentation_segment_anything
Output Version: 001

[Files]
Workflow: workflows/segmentation_segment_anything/api/pitch_character_seg.json
Input: in/1858_PITCH/segmentation_segment_anything/v001/
Output: out/1858_PITCH/segmentation_segment_anything/001/
```

### Example 2: 1756_LEREMPLACANT Normal Maps

```
[Plugin Settings]
Workflows Directory: workflows/normal_map_deepbump/api
New Workflow Name: leremplacant_normals
Project Name: 1756_LEREMPLACANT
Workflow Name: normal_map_deepbump
Output Version: 001

[Files]
Workflow: workflows/normal_map_deepbump/api/leremplacant_normals.json
Input: in/1756_LEREMPLACANT/normal_map_deepbump/v001/
Output: out/1756_LEREMPLACANT/normal_map_deepbump/001/
```

---

## Quick Test

```bash
# 1. Create test workflow
# In Flame: New Workflow Name = "test_autoload", Click "New Workflow"

# 2. Verify files
ls /Volumes/silo2/002_COMFYUI/workflows/test_autoload.json
ls /Volumes/silo2/002_COMFYUI/in/test_autoload.json

# 3. Check browser
# URL should be: http://server:8188/?load_local_json=test_autoload.json
# Workflow should load on canvas

# 4. Clean up
rm /Volumes/silo2/002_COMFYUI/workflows/test_autoload.json
rm /Volumes/silo2/002_COMFYUI/in/test_autoload.json
```

---

## Support

**Logs**:
- Plugin: `~/Library/Logs/AnyComfy/anycomfy.log`
- Browser: F12 → Console (look for `[OFX AutoLoader]`)
- ComfyUI: Server console output

**Issues**: Check documentation or review logs for error messages

---

**Last Updated**: January 10, 2026
**Environment**: Production (macOS → Windows Server)
**Plugin Version**: 1.2.0

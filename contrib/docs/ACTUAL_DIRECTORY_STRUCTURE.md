# Actual Directory Structure - Production Setup

**Environment**: macOS Client → Windows ComfyUI Server
**LAN Mount**: `/Volumes/silo2/002_COMFYUI` (macOS) = `S:\002_COMFYUI` (Windows)
**Date**: January 10, 2026

---

## Current Directory Layout

Based on your actual production environment:

```
/Volumes/silo2/002_COMFYUI/
├── in/                          ← --input-directory (auto-loading copies)
│   ├── _MISC/
│   ├── 1756_LEREMPLACANT/
│   │   ├── normal_map_deepbump/
│   │   ├── segmentation_segment_anything/
│   │   ├── zdepth_depth_anything/
│   │   └── zdepth_marigold/
│   ├── 1824_SEUJORGE/
│   ├── 1826_GIVENCHY_MACSTUDIO2/
│   ├── 1858_PITCH/
│   ├── SAM_TEST/
│   ├── TEST/
│   └── ... (project input directories)
│
├── out/                         ← --output-directory (processed outputs)
│   ├── 1756_LEREMPLACANT/
│   │   ├── normal_map_deepbump/
│   │   │   ├── 001/
│   │   │   ├── 002/
│   │   │   └── 003/
│   │   ├── segmentation_segment_anything/
│   │   │   ├── 001/
│   │   │   └── ... (version directories)
│   │   └── ... (workflow outputs)
│   ├── TEST/
│   └── ... (project output directories)
│
├── workflows/                   ← Permanent workflow storage (organized by type)
│   ├── _misc/
│   ├── normal_map_deepbump/
│   │   └── api/
│   ├── relighting_ic_light/
│   │   └── api/
│   ├── segmentation_segment_anything/
│   │   └── api/
│   ├── stable_diffusion/
│   │   └── api/
│   ├── zdepth_depth_anything/
│   │   └── api/
│   └── zdepth_marigold/
│       └── api/
│
├── models/                      ← AI models
│   ├── checkpoints/
│   ├── loras/
│   ├── sams/
│   ├── deepbump/
│   ├── depthanything/
│   ├── diffusers/
│   ├── grounding-dino/
│   └── ... (model directories)
│
├── ofx/                         ← OFX plugin bundles
│   └── plugins/
│       ├── AnyComfy.ofx.bundle/
│       ├── SAMSegmentation.ofx.bundle/
│       └── logs/
│
├── presets/                     ← ComfyUI presets
│
└── _archive/                    ← Archived files
    ├── in/
    ├── out/
    └── workflows/
```

---

## Plugin Configuration (Production Settings)

### Server Page

```
Server Address: 192.168.x.x (your Windows server IP)
Server Port: 8188
Shared Mount Path: /Volumes/silo2/002_COMFYUI
Server Mount Point: S:
ComfyUI Input Directory: /Volumes/silo2/002_COMFYUI/in  ✓ (already default)
```

### Workflow Page

**Option 1: Simple (top-level workflows)**
```
Workflows Directory: workflows
New Workflow Name: my_workflow
```
**Result**: `/Volumes/silo2/002_COMFYUI/workflows/my_workflow.json`

**Option 2: Organized by type (matches your structure)**
```
Workflows Directory: workflows/segmentation_segment_anything/api
New Workflow Name: my_sam_workflow
```
**Result**: `/Volumes/silo2/002_COMFYUI/workflows/segmentation_segment_anything/api/my_sam_workflow.json`

---

## Workflow Organization Patterns

### Your Current Pattern

You've organized workflows by **AI model/type**:

```
workflows/
├── normal_map_deepbump/api/      ← DeepBump normal map workflows
├── relighting_ic_light/api/      ← IC-Light relighting workflows
├── segmentation_segment_anything/api/  ← SAM segmentation workflows
├── stable_diffusion/api/         ← Stable Diffusion workflows
├── zdepth_depth_anything/api/    ← Depth Anything workflows
└── zdepth_marigold/api/          ← Marigold depth workflows
```

This is **excellent organization** because:
- ✅ Workflows grouped by function
- ✅ Clear separation of model types
- ✅ Easy to find specific workflow types
- ✅ Scales well as you add more workflows

### Using AnyComfy with Organized Workflows

**For each workflow type, configure the plugin accordingly:**

#### Example 1: SAM Segmentation Workflows

**Plugin Settings**:
```
Workflows Directory: workflows/segmentation_segment_anything/api
New Workflow Name: person_segmentation
```

**Result**:
- Saves to: `workflows/segmentation_segment_anything/api/person_segmentation.json`
- Copies to: `in/person_segmentation.json` (for auto-loading)

#### Example 2: Normal Map Workflows

**Plugin Settings**:
```
Workflows Directory: workflows/normal_map_deepbump/api
New Workflow Name: product_normal_map
```

**Result**:
- Saves to: `workflows/normal_map_deepbump/api/product_normal_map.json`
- Copies to: `in/product_normal_map.json` (for auto-loading)

---

## Input Directory Organization

Your `in/` directory follows a **project → workflow type** structure:

```
in/
├── 1756_LEREMPLACANT/           ← Project folder
│   ├── normal_map_deepbump/     ← Workflow type
│   ├── segmentation_segment_anything/
│   ├── zdepth_depth_anything/
│   └── zdepth_marigold/
├── 1824_SEUJORGE/
├── TEST/
└── ... (other projects)
```

This matches the **Project Name** and **Workflow Name** parameters in the plugin:

```
Project Name: 1756_LEREMPLACANT
Workflow Name: segmentation_segment_anything
```

**Input path structure**:
```
in/{PROJECT}/{WORKFLOW}/v{VERSION}/frame_####.exr
```

**Example**:
```
in/1756_LEREMPLACANT/segmentation_segment_anything/v001/frame_0001.exr
```

---

## Output Directory Organization

Your `out/` directory mirrors the `in/` structure with version subdirectories:

```
out/
├── 1756_LEREMPLACANT/
│   ├── segmentation_segment_anything/
│   │   ├── 001/                 ← Version folder
│   │   ├── 002/
│   │   └── ...
│   ├── normal_map_deepbump/
│   │   ├── 001/
│   │   └── ...
│   └── ... (workflow outputs)
└── ... (other projects)
```

**Output path structure**:
```
out/{PROJECT}/{WORKFLOW}/{VERSION}/frame_####.exr
```

**Example**:
```
out/1756_LEREMPLACANT/segmentation_segment_anything/001/frame_0001.exr
```

---

## Complete Workflow Example

### Scenario: Creating a new SAM segmentation workflow for project 1858_PITCH

**1. Configure Plugin (in Flame)**:
```
[Workflow Page]
Workflows Directory: workflows/segmentation_segment_anything/api
New Workflow Name: pitch_character_seg

[Project Page]
Project Name: 1858_PITCH
Workflow Name: segmentation_segment_anything
Output Version: 001

[Server Page]
ComfyUI Input Directory: /Volumes/silo2/002_COMFYUI/in  (default ✓)
```

**2. Click "New Workflow"**:

**Files Created**:
```
Permanent storage:
  /Volumes/silo2/002_COMFYUI/workflows/segmentation_segment_anything/api/pitch_character_seg.json

Auto-loading copy:
  /Volumes/silo2/002_COMFYUI/in/pitch_character_seg.json
```

**3. Browser Opens**:
```
URL: http://192.168.x.x:8188/?load_local_json=pitch_character_seg.json
```

**4. Extension Fetches**:
```
GET /view?filename=pitch_character_seg.json&type=input
→ ComfyUI serves: S:\002_COMFYUI\in\pitch_character_seg.json
```

**5. Edit in ComfyUI**:
- Add SAM model nodes
- Configure segmentation parameters
- Save workflow (Ctrl+S)

**6. Render in Flame**:

**Input images from**:
```
/Volumes/silo2/002_COMFYUI/in/1858_PITCH/segmentation_segment_anything/v001/
```

**Output images to**:
```
/Volumes/silo2/002_COMFYUI/out/1858_PITCH/segmentation_segment_anything/001/
```

---

## Directory Pre-Creation

Based on your structure, you need these directories before rendering:

### Input Directories (Created Manually or by Pipeline)

```bash
# Example for project 1858_PITCH
mkdir -p /Volumes/silo2/002_COMFYUI/in/1858_PITCH/segmentation_segment_anything/v001

# Copy input frames
cp ~/renders/pitch_frames/*.exr \
   /Volumes/silo2/002_COMFYUI/in/1858_PITCH/segmentation_segment_anything/v001/
```

### Output Directories (Auto-created by Plugin)

The plugin automatically creates:
```
/Volumes/silo2/002_COMFYUI/out/{PROJECT}/{WORKFLOW}/{VERSION}/
```

**Example**:
```
/Volumes/silo2/002_COMFYUI/out/1858_PITCH/segmentation_segment_anything/001/
```

---

## Best Practices for Your Setup

### 1. Workflow Naming Convention

Based on your organization, use descriptive names that indicate purpose:

```
Good Examples:
- person_fullbody_seg.json
- face_detailed_seg.json
- product_beauty_seg.json
- character_matte.json

Project-Specific:
- pitch_character_seg.json
- givenchy_product_seg.json
- seujorge_face_seg.json
```

### 2. Directory Structure Consistency

**Maintain the pattern**:
```
workflows/{WORKFLOW_TYPE}/api/{specific_workflow}.json
in/{PROJECT}/{WORKFLOW_TYPE}/v{VERSION}/
out/{PROJECT}/{WORKFLOW_TYPE}/{VERSION}/
```

This ensures:
- ✅ Easy to find workflows by type
- ✅ Clear project organization
- ✅ Version tracking for iterations

### 3. Version Numbering

Your structure uses:
- `v001`, `v002`, etc. for input versions (in/)
- `001`, `002`, etc. for output versions (out/)

**Plugin parameter format**:
```
Output Version: 001  (not v001 - plugin adds directory structure)
```

### 4. Archive Organization

You have `_archive/` for old files:
```
_archive/
├── in/
├── out/
└── workflows/
```

**Best practice**:
- Move completed projects to `_archive/` after delivery
- Keep `workflows/` current for active projects
- Archive old workflow versions but keep latest in main directory

---

## Multi-Project Workflow Sharing

### Scenario: Same workflow, multiple projects

**Option 1: Single Workflow, Multiple Projects** (Recommended)

Create one workflow file:
```
workflows/segmentation_segment_anything/api/standard_person_seg.json
```

Use for multiple projects by changing **Project Name** parameter:
```
Project 1:
  Project Name: 1756_LEREMPLACANT
  Workflow File Path: workflows/segmentation_segment_anything/api/standard_person_seg.json

Project 2:
  Project Name: 1858_PITCH
  Workflow File Path: workflows/segmentation_segment_anything/api/standard_person_seg.json
```

**Result**: Different input/output directories, same workflow logic

**Option 2: Project-Specific Workflows**

Create separate workflow files:
```
workflows/segmentation_segment_anything/api/leremplacant_person_seg.json
workflows/segmentation_segment_anything/api/pitch_character_seg.json
```

Use when projects need different settings.

---

## Testing Checklist

### ✅ Verify Directory Structure
```bash
# Check directories exist
ls /Volumes/silo2/002_COMFYUI/in
ls /Volumes/silo2/002_COMFYUI/out
ls /Volumes/silo2/002_COMFYUI/workflows

# Check organized workflow directories
ls /Volumes/silo2/002_COMFYUI/workflows/segmentation_segment_anything/api
ls /Volumes/silo2/002_COMFYUI/workflows/normal_map_deepbump/api
```

### ✅ Test Workflow Creation
```
1. Plugin Settings:
   Workflows Directory: workflows/segmentation_segment_anything/api
   New Workflow Name: test_autoload

2. Click "New Workflow"

3. Verify Files:
   ls /Volumes/silo2/002_COMFYUI/workflows/segmentation_segment_anything/api/test_autoload.json
   ls /Volumes/silo2/002_COMFYUI/in/test_autoload.json

4. Verify Browser:
   URL: http://server:8188/?load_local_json=test_autoload.json
   Workflow should be loaded on canvas
```

### ✅ Test Rendering
```
1. Create input directory:
   mkdir -p /Volumes/silo2/002_COMFYUI/in/TEST/segmentation_segment_anything/v001

2. Copy test frame:
   cp test_frame.exr /Volumes/silo2/002_COMFYUI/in/TEST/segmentation_segment_anything/v001/frame_0001.exr

3. Plugin Settings:
   Project Name: TEST
   Workflow Name: segmentation_segment_anything
   Output Version: 001

4. Render frame 1

5. Verify Output:
   ls /Volumes/silo2/002_COMFYUI/out/TEST/segmentation_segment_anything/001/frame_0001.exr
```

---

## Summary

### Your Production Setup

✅ **Directory Organization**: Well-structured, type-based workflow organization
✅ **Plugin Configuration**: Defaults match your setup perfectly
✅ **Path Mapping**: macOS `/Volumes/silo2/002_COMFYUI` ↔ Windows `S:\002_COMFYUI`
✅ **Scalability**: Structure supports multiple projects, workflows, and versions

### Key Configuration Points

```
ComfyUI Server Command (Windows):
  --input-directory S:\002_COMFYUI\in
  --output-directory S:\002_COMFYUI\out

Plugin Default (macOS):
  ComfyUI Input Directory: /Volumes/silo2/002_COMFYUI/in ✓

Workflow Organization:
  Type-based: workflows/{WORKFLOW_TYPE}/api/
  Project-based: in/{PROJECT}/{WORKFLOW_TYPE}/v{VERSION}/
  Output: out/{PROJECT}/{WORKFLOW_TYPE}/{VERSION}/
```

### Ready for Production

The plugin is configured to work seamlessly with your **actual production environment**. No additional configuration needed!

---

**Last Updated**: January 10, 2026
**Environment**: Production (macOS → Windows ComfyUI Server)
**Status**: Verified ✅

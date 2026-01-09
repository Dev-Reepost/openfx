# Directory Setup Guide for AnyComfy

## Issue: Output Directory Not Found

When running AnyComfy workflows, you may encounter this error:

```
FileNotFoundError: [WinError 3] The system cannot find the path specified:
'Z:\\out\\PROJECT_NAME\\workflow_name\\v001'
```

## Root Cause

ComfyUI's SaveEXR node uses `os.mkdir()` which only creates **one directory level**, not the full directory tree. If parent directories don't exist, the workflow fails.

Example:
```python
# This FAILS if Z:\out\PROJECT\ doesn't exist:
os.mkdir("Z:\\out\\PROJECT\\workflow\\v001")

# This would WORK (creates full tree):
os.makedirs("Z:\\out\\PROJECT\\workflow\\v001", exist_ok=True)
```

Unfortunately, we cannot modify ComfyUI's SaveEXR node code. Therefore, **you must create the output directory structure on the ComfyUI server BEFORE running workflows**.

## Solution 1: Manual Directory Creation (Quick Fix)

### On Windows Server

Open Command Prompt and create the directory structure:

```batch
mkdir Z:\out\PROJECT_NAME\workflow_name\v001
```

Or create the base structure once:

```batch
mkdir Z:\out
cd Z:\out

mkdir PROJECT1
mkdir PROJECT1\segmentation
mkdir PROJECT1\segmentation\v001

mkdir PROJECT2
mkdir PROJECT2\upscale
mkdir PROJECT2\upscale\v001
```

### On Linux/macOS Server

```bash
mkdir -p /mnt/share/out/PROJECT_NAME/workflow_name/v001
```

## Solution 2: Python Script for Automatic Creation

Create a Python script on the ComfyUI server to automatically create directory structures.

### directory_creator.py

```python
#!/usr/bin/env python3
"""
ComfyUI Output Directory Creator
Automatically creates directory structures for AnyComfy workflows
"""

import os
import sys
from pathlib import Path

# Configuration - update these paths for your setup
SERVER_MOUNT = "Z:" if sys.platform == "win32" else "/mnt/share"
OUTPUT_BASE = os.path.join(SERVER_MOUNT, "out")

def create_project_structure(project_name, workflows=None, versions=None):
    """
    Create complete directory structure for a project.

    Args:
        project_name: Name of the project (e.g., "PROJECT1", "ANYCOMFY_TEST")
        workflows: List of workflow names (e.g., ["segmentation", "upscale"])
                  If None, creates basic structure
        versions: List of version names (e.g., ["v001", "v002"])
                 If None, defaults to ["v001"]
    """
    if workflows is None:
        workflows = ["workflow"]
    if versions is None:
        versions = ["v001"]

    print(f"Creating directory structure for project: {project_name}")

    for workflow in workflows:
        for version in versions:
            dir_path = os.path.join(OUTPUT_BASE, project_name, workflow, version)

            try:
                os.makedirs(dir_path, exist_ok=True)
                print(f"  ✓ Created: {dir_path}")
            except Exception as e:
                print(f"  ✗ Failed to create {dir_path}: {e}")

def create_all_common_structures():
    """Create commonly used directory structures."""
    print("=" * 60)
    print("Creating common output directory structures...")
    print("=" * 60)

    # Ensure base output directory exists
    os.makedirs(OUTPUT_BASE, exist_ok=True)
    print(f"Base output directory: {OUTPUT_BASE}")
    print()

    # Common projects and workflows
    projects = {
        "TEST": ["segmentation", "upscale", "denoise"],
        "ANYCOMFY_TEST": ["any_segmentation", "any_upscale"],
    }

    for project, workflows in projects.items():
        create_project_structure(project, workflows, ["v001", "v002", "v003"])
        print()

    print("=" * 60)
    print("Directory creation complete!")
    print("=" * 60)

if __name__ == "__main__":
    if len(sys.argv) > 1:
        # Custom project creation
        project_name = sys.argv[1]
        workflows = sys.argv[2:] if len(sys.argv) > 2 else None
        create_project_structure(project_name, workflows)
    else:
        # Create all common structures
        create_all_common_structures()
```

### Usage

```bash
# On Windows
python directory_creator.py

# On Linux/macOS
python3 directory_creator.py

# Create custom project structure
python directory_creator.py MY_PROJECT segmentation upscale denoise
```

## Solution 3: Pre-flight Directory Check (Future Enhancement)

We could add a pre-flight check in the AnyComfy plugin that:

1. Parses the output path from workflow
2. Sends HTTP request to ComfyUI server to create directories
3. Only then submits the workflow

**Note**: This would require adding a custom endpoint to ComfyUI server or using a custom node.

## Solution 4: Custom ComfyUI Node

Create a custom ComfyUI node that creates directories before SaveEXR runs.

### Directory Creator Node

```python
# custom_nodes/directory_creator.py

class DirectoryCreator:
    @classmethod
    def INPUT_TYPES(cls):
        return {
            "required": {
                "path": ("STRING", {"default": ""}),
            }
        }

    RETURN_TYPES = ()
    FUNCTION = "create_directory"
    OUTPUT_NODE = True
    CATEGORY = "utils"

    def create_directory(self, path):
        import os
        from pathlib import Path

        # Extract directory from path
        if path:
            directory = os.path.dirname(path)
            if directory:
                os.makedirs(directory, exist_ok=True)
                print(f"Created directory: {directory}")

        return ()

NODE_CLASS_MAPPINGS = {
    "DirectoryCreator": DirectoryCreator
}
```

Then add this node to your workflows BEFORE SaveEXR.

## Recommended Approach

### For Development/Testing

Use **Solution 1** (Manual Creation):
- Quick and simple
- Create directories once for each project

### For Production

Use **Solution 2** (Python Script):
- Run the script once when setting up a new project
- Can be integrated into project setup workflows
- Can be run periodically to ensure structure exists

### For Advanced Users

Use **Solution 4** (Custom Node):
- Add the directory creator node to all workflows
- Ensures directories always exist before SaveEXR runs
- Most robust solution

## Directory Structure Best Practices

### Standard Structure

```
Z:\out\  (or /mnt/share/out/)
├── PROJECT1/
│   ├── segmentation/
│   │   ├── v001/
│   │   ├── v002/
│   │   └── v003/
│   ├── upscale/
│   │   ├── v001/
│   │   └── v002/
│   └── denoise/
│       └── v001/
├── PROJECT2/
│   └── workflow_name/
│       └── v001/
└── TEST/
    └── any_workflow/
        └── v001/
```

### Naming Conventions

- **Project names**: Use uppercase with underscores (e.g., `MY_PROJECT`, `TEST_SAM`)
- **Workflow names**: Use lowercase with underscores (e.g., `segmentation`, `any_upscale`)
- **Versions**: Use `v001`, `v002`, etc. format

**Avoid**:
- Spaces in names (use underscores instead)
- Special characters (except underscores and hyphens)
- Commas (can cause parsing issues)

### Example AnyComfy Parameters

```
Project Name:    ANYCOMFY_TEST
Workflow Name:   any_segmentation
Output Version:  v001
```

This creates outputs in:
```
Z:\out\ANYCOMFY_TEST\any_segmentation\v001\{basename}.####.exr
```

## Troubleshooting

### Error: "Path not found: Z:\\\\out\\\\PROJECT..."

**Cause**: Double-escaped backslashes in path (fixed in plugin v1.0.2)

**Solution**: Update to latest AnyComfy plugin

### Error: "The system cannot find the path specified"

**Cause**: Directory structure doesn't exist on server

**Solution**:
1. Identify the missing directory from error message
2. Create it manually or use the Python script
3. Re-run the workflow

### Error: "Permission denied"

**Cause**: No write permissions on output directory

**Solution**:
1. Check folder permissions on the server
2. Ensure ComfyUI server process has write access
3. On Windows: Right-click folder → Properties → Security
4. On Linux: `chmod -R 775 /mnt/share/out/`

### Error: "Invalid character in path"

**Cause**: Special characters in project/workflow names

**Solution**:
- Remove commas, spaces, special characters
- Use underscores instead: `MY_PROJECT` not `MY,PROJECT`
- Stick to alphanumeric + underscores

## Summary

1. **Create output directories on ComfyUI server BEFORE running workflows**
2. **Use the Python script** for easy automation
3. **Follow naming conventions** to avoid path issues
4. **Update to latest plugin** for proper path escaping

---

**Last Updated**: December 26, 2025
**Plugin Version**: 1.0.2 (escaping fix)
**Related Issues**: Path escaping, directory creation

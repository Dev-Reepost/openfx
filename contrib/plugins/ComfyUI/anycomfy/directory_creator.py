#!/usr/bin/env python3
"""
ComfyUI Output Directory Creator
Automatically creates directory structures for AnyComfy workflows

This script should be run on the ComfyUI server to create the necessary
output directory structure before running AnyComfy workflows.

Usage:
    # Create common structures
    python directory_creator.py

    # Create custom project structure
    python directory_creator.py PROJECT_NAME workflow1 workflow2 workflow3

Examples:
    python directory_creator.py
    python directory_creator.py ANYCOMFY_TEST segmentation upscale
    python directory_creator.py MY_PROJECT any_workflow v001 v002
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
    print("ComfyUI Output Directory Creator")
    print("=" * 60)

    # Ensure base output directory exists
    try:
        os.makedirs(OUTPUT_BASE, exist_ok=True)
        print(f"Base output directory: {OUTPUT_BASE}")
        print()
    except Exception as e:
        print(f"ERROR: Failed to create base directory {OUTPUT_BASE}: {e}")
        print("Please check:")
        print("  1. The mount point is correct")
        print("  2. You have write permissions")
        print("  3. The path exists and is accessible")
        sys.exit(1)

    # Common projects and workflows
    projects = {
        "TEST": ["segmentation", "upscale", "denoise"],
        "ANYCOMFY_TEST": ["any_segmentation", "any_upscale", "any_denoise"],
        "PRODUCTION": ["workflow", "v001"],
    }

    for project, workflows in projects.items():
        create_project_structure(project, workflows, ["v001", "v002", "v003"])
        print()

    print("=" * 60)
    print("Directory creation complete!")
    print("=" * 60)
    print()
    print("You can now use these directories in AnyComfy workflows.")
    print("Example parameters:")
    print("  Project Name:    TEST")
    print("  Workflow Name:   segmentation")
    print("  Output Version:  v001")

def validate_name(name):
    """Validate directory name (no special characters)."""
    import re
    if not re.match(r'^[a-zA-Z0-9_-]+$', name):
        print(f"WARNING: '{name}' contains special characters.")
        print("Recommended: Use only letters, numbers, underscores, and hyphens.")
        print("Example: MY_PROJECT not MY,PROJECT or MY PROJECT")
        return False
    return True

def main():
    """Main entry point."""
    print()
    print("ComfyUI Output Directory Creator for AnyComfy")
    print("-" * 60)

    if len(sys.argv) > 1:
        # Custom project creation
        project_name = sys.argv[1]

        if not validate_name(project_name):
            print()
            response = input("Continue anyway? [y/N]: ")
            if response.lower() != 'y':
                print("Cancelled.")
                sys.exit(0)

        workflows = sys.argv[2:] if len(sys.argv) > 2 else None

        print()
        create_project_structure(project_name, workflows)
        print()
        print(f"Created structure for project: {project_name}")
    else:
        # Create all common structures
        print("No arguments provided. Creating common directory structures...")
        print()
        create_all_common_structures()

    print()
    print("Done!")
    print()

if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print()
        print("Cancelled by user.")
        sys.exit(0)
    except Exception as e:
        print()
        print(f"ERROR: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)

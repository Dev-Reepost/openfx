# ComfyUI Plugin Logging Guide

## Overview

The ComfyUI OFX plugins now include comprehensive logging to help debug issues during development and deployment. All log files are automatically created in your home directory.

## Log File Location

Log files are created with timestamps in your home directory:
```
~/comfyui_plugin_YYYYMMDD_HHMMSS.log
```

For example:
```
~/comfyui_plugin_20251107_121500.log
```

## What Gets Logged

The logging system tracks every step of the plugin execution:

### 1. Plugin Initialization
- Plugin constructor
- Parameter fetching
- Logger initialization

### 2. Render Process
- Frame number being processed
- All parameter values (server, storage paths, etc.)
- ComfyUI client creation

### 3. Workflow Execution Steps
- **Step 1**: Writing input image to shared storage
  - Input image path (local and Windows-converted)
  - Image specifications (dimensions, bit depth, channels)
  - EXR write operations

- **Step 2**: Building workflow JSON
  - Workflow parameters (prompt, threshold, models, etc.)
  - Path conversions (Mac → Windows)
  - Workflow JSON size

- **Step 3**: Queueing workflow to ComfyUI
  - Client ID
  - Prompt ID received from server

- **Step 4**: Monitoring execution
  - Queue status updates
  - Node execution progress
  - Server-side progress (0-100%)
  - Execution errors
  - Cache hits
  - Completion status

- **Step 5**: Retrieving history
  - History JSON size
  - Prompt IDs found

- **Step 6**: Parsing output path
  - Output nodes checked
  - Output filename found
  - Constructed output path

- **Step 7**: Loading result
  - EXR file reading
  - Image specifications
  - Buffer copying operations

### 4. Path Conversion
- Original Mac/Unix paths
- Converted Windows paths
- Conversion logic applied

### 5. Errors
- All errors are logged with context
- Exception details
- Stack traces where available

## Log Format

Each log line includes:
- Timestamp (YYYY-MM-DD HH:MM:SS.milliseconds)
- Log level (info, warn, error)
- Message

Example:
```
[2025-11-07 12:15:30.123] [info] RENDER STARTED - Frame: 1
[2025-11-07 12:15:30.125] [info] Parameters:
[2025-11-07 12:15:30.125] [info]   Server: 192.168.1.211:8188
[2025-11-07 12:15:30.125] [info]   Mount Path: /Z
[2025-11-07 12:15:30.126] [info]   Project: TEST_SAM
[2025-11-07 12:15:30.126] [info]   Workflow: segmentation
```

## Using the Logs for Debugging

### Finding the Latest Log
```bash
ls -lht ~/comfyui_plugin_*.log | head -1
```

### Tailing the Log in Real-Time
```bash
tail -f ~/comfyui_plugin_*.log
```

### Searching for Errors
```bash
grep -i error ~/comfyui_plugin_*.log
```

### Checking Path Conversion
```bash
grep "Converting path" ~/comfyui_plugin_*.log
```

### Viewing Workflow Parameters
```bash
grep -A 10 "SAM Parameters:" ~/comfyui_plugin_*.log
```

### Checking ComfyUI Communication
```bash
grep -E "(Queueing workflow|Executing node|Progress:)" ~/comfyui_plugin_*.log
```

## Common Issues to Look For

### 1. Path Conversion Problems
Look for:
```
[info] Converting path for ComfyUI: /Z/in/...
[info] Converted path: Z:\in\...
```

Verify that:
- Input paths start with `/Z/`
- Converted paths start with `Z:\`
- All forward slashes are converted to backslashes

### 2. Missing Parameters
Look for empty values in:
```
[info] Parameters:
[info]   Server: <should not be empty>
[info]   Mount Path: <should not be empty>
```

### 3. ComfyUI Server Connection Issues
Look for:
```
[error] ComfyUI execution failed
[error] Execution error: {...}
```

### 4. File I/O Problems
Look for:
```
[error] Failed to find output file in ComfyUI history
[error] Output image size mismatch
```

### 5. Model Loading Issues
Check the workflow parameters:
```
[info] SAM Parameters:
[info]   SAM Model: sam_vit_h (2.56GB)
[info]   DINO Model: GroundingDINO_SwinT_OGC (694MB)
```

## Performance Monitoring

The logs include timing information through progress updates:
- 0% - Start
- 10% - Input written
- 15% - Workflow built
- 20% - Queued to server
- 30-80% - Server processing
- 85% - History retrieved
- 90% - Loading result
- 95% - Copying to buffer
- 100% - Complete

## Log Cleanup

Old log files can be safely deleted:
```bash
# Delete logs older than 7 days
find ~ -name "comfyui_plugin_*.log" -mtime +7 -delete

# Keep only the 10 most recent logs
ls -t ~/comfyui_plugin_*.log | tail -n +11 | xargs rm -f
```

## Disabling Logging

Logging is always enabled in debug builds. To reduce log verbosity in production, the logging level can be adjusted in the code (see `comfyui_base_plugin.cpp:30`).

## Support

When reporting issues, please include:
1. The complete log file from the failed render
2. Flame console output
3. ComfyUI server console output
4. Your plugin parameter settings

This will help diagnose issues quickly and accurately.

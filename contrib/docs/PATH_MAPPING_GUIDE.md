# Path Mapping Guide for ComfyUI Plugin

## Overview

The ComfyUI plugin needs to access shared storage from both the **client side** (Mac/Linux running Flame) and the **server side** (Windows/Linux running ComfyUI). These systems may mount the same network storage at different paths.

## How It Works

The plugin uses two parameters to handle cross-platform path mapping:

1. **Client Mount Path** - Where the shared storage is mounted on YOUR machine (Mac/Linux)
2. **Server Mount Point** - Where the shared storage is mounted on the COMFYUI server

### Example Scenario

**Your Setup:**
- Mac running Flame mounts storage at: `/Volumes/silo2/002_COMFYUI`
- Windows running ComfyUI has drive mapped as: `Z:`

**Plugin Configuration:**
```
Client Mount Path:    /Volumes/silo2/002_COMFYUI
Server Mount Point:   Z:
```

**Path Conversion:**
```
Input:  /Volumes/silo2/002_COMFYUI/in/TEST_SAM/segmentation/shot01_beauty_0001.exr
Output: Z:\in\TEST_SAM\segmentation\shot01_beauty_0001.exr
```

## Configuration in Flame

When you configure the SAMSegmentation plugin in Flame, set these parameters:

### Server Configuration
```
Server Address:       192.168.1.211  (or IP of your ComfyUI server)
Port:                 8188
```

### Storage Configuration
```
Client Mount Path:    /Volumes/silo2/002_COMFYUI  (Mac)
                      /mnt/storage                (Linux)

Server Mount Point:   Z:                          (Windows with mapped drive)
                      /mnt/storage                (Linux server)
                      \\192.168.1.110\silo2\002_COMFYUI  (Windows UNC path)
```

### Project Organization
```
Project Name:         TEST_SAM
Workflow Name:        segmentation
Basename:             shot01
Layer Name:           beauty
Output Version:       v001
```

## Common Deployment Scenarios

### Scenario 1: Mac Flame → Windows ComfyUI
```
Client Mount Path:    /Volumes/silo2/002_COMFYUI
Server Mount Point:   Z:
```

On Windows server, map the network drive:
```powershell
net use Z: \\192.168.1.110\silo2\002_COMFYUI
```

### Scenario 2: Linux Flame → Linux ComfyUI (Same Path)
```
Client Mount Path:    /mnt/storage
Server Mount Point:   /mnt/storage
```

Both machines mount at the same location - no conversion needed.

### Scenario 3: Mac Flame → Windows ComfyUI (UNC Path)
```
Client Mount Path:    /Volumes/silo2/002_COMFYUI
Server Mount Point:   \\192.168.1.110\silo2\002_COMFYUI
```

ComfyUI server accesses via UNC path (no drive mapping required).

### Scenario 4: Windows Flame → Windows ComfyUI
```
Client Mount Path:    Z:
Server Mount Point:   Z:
```

Both use the same drive letter.

## Path Conversion Logic

The plugin performs this conversion:

1. **Read client path**: `/Volumes/silo2/002_COMFYUI/in/project/file.exr`
2. **Replace mount**: `Z:/in/project/file.exr`
3. **Convert slashes**: `Z:\in\project\file.exr`

This converted path is sent to the ComfyUI server in the workflow JSON.

## Verifying Configuration

After configuring the plugin, check the log file to verify paths are converted correctly:

```bash
tail -f ~/comfyui_plugin_*.log
```

Look for these log entries:
```
[info] Parameters:
[info]   Client Mount: /Volumes/silo2/002_COMFYUI
[info]   Server Mount: Z:
...
[info] Converting path for ComfyUI: /Volumes/silo2/002_COMFYUI/in/TEST_SAM/...
[info]   Client mount: /Volumes/silo2/002_COMFYUI
[info]   Server mount: Z:
[info]   After mount replacement: Z:/in/TEST_SAM/...
[info] Converted path: Z:\in\TEST_SAM\...
```

## Troubleshooting

### Issue: "Path doesn't start with client mount!"

**Problem**: The file path doesn't begin with the Client Mount Path.

**Check:**
1. Verify Client Mount Path is correct
2. Ensure the path doesn't have trailing slashes
3. Check case sensitivity on Linux

**Example:**
```
❌ Wrong:  Client Mount = "/Volumes/silo2/002_COMFYUI/"  (trailing slash)
✓ Right:  Client Mount = "/Volumes/silo2/002_COMFYUI"
```

### Issue: ComfyUI can't find files

**Problem**: Server can't access the converted path.

**Check:**
1. Verify Server Mount Point is accessible on ComfyUI server
2. Test from ComfyUI server command line:
   ```powershell
   dir Z:\in\
   ```
3. Check network connectivity
4. Verify permissions

### Issue: Wrong slash direction

**Problem**: Paths have mixed slashes (e.g., `Z:/in\file.exr`).

**Check:**
- This is normal during conversion (logged as "After mount replacement")
- Final path (logged as "Converted path") should have all backslashes on Windows

## Development vs Production

### Development (Single Machine)
You can run ComfyUI on the same Mac as Flame:

```
Client Mount Path:    /Users/julien/comfyui_storage
Server Mount Point:   /Users/julien/comfyui_storage
```

Both point to the same local directory.

### Production (Separate Machines)
Use network storage with proper mount points:

```
Mac Flame:
  Client Mount Path:    /Volumes/prod_storage/comfyui

Windows ComfyUI:
  Server Mount Point:   P:

Linux ComfyUI:
  Server Mount Point:   /mnt/prod_storage/comfyui
```

## Best Practices

1. **Use absolute paths** - Don't use `~` or relative paths
2. **No trailing slashes** - `/Volumes/storage` not `/Volumes/storage/`
3. **Test connectivity** - Verify both machines can read/write to storage
4. **Check logs** - Always verify path conversion in the log file
5. **Consistent naming** - Use the same directory structure on all machines

## Network Storage Recommendations

### SMB/CIFS (Windows Shares)
- Best for mixed Windows/Mac/Linux environments
- Mount on Mac: `mount_smbfs //server/share /Volumes/share`
- Mount on Windows: `net use Z: \\server\share`
- Mount on Linux: `mount -t cifs //server/share /mnt/share`

### NFS (Unix/Linux Native)
- Best for Linux/Mac environments
- Generally faster than SMB
- Mount on Mac: `mount -t nfs server:/export /Volumes/nfs`
- Mount on Linux: `mount -t nfs server:/export /mnt/nfs`

### Avoid
- ❌ User-specific paths (`~/Documents`)
- ❌ Local-only paths (`/tmp`, `C:\Temp`)
- ❌ Different directory structures on client/server

## Summary

The two-parameter system provides maximum flexibility:

- **Client Mount Path**: Where YOU access the files (Mac/Linux/Windows)
- **Server Mount Point**: Where COMFYUI accesses the same files (Windows/Linux)

This allows any combination of client and server operating systems while maintaining a consistent file exchange mechanism.

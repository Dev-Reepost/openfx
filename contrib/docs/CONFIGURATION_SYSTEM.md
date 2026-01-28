# ComfyUI Plugin Configuration System

**Version:** 1.0
**Date:** 2026-01-21
**Status:** Production

---

## Overview

The ComfyUI OFX plugins support external JSON configuration files for setting default parameter values. This eliminates the need to recompile plugins when deploying to different environments or adjusting server settings.

**Key Benefits:**
- ✅ No recompilation needed to change defaults
- ✅ Per-installation customization via bundle resources
- ✅ Graceful degradation - missing config uses hardcoded fallbacks
- ✅ Standard OFX bundle structure (portable across systems)

---

## Configuration File Location

Configuration files are stored in the plugin bundle's `Contents/Resources/config/` directory:

```
AnyComfy.ofx.bundle/
├── Contents/
│   ├── Info.plist
│   ├── MacOS/
│   │   └── AnyComfy.ofx
│   └── Resources/
│       ├── config/
│       │   ├── defaults.json      ← Configuration file
│       │   └── README.md
│       └── workflows/
│           └── ...
```

### Search Paths

The plugin searches for configuration files in the following order (first match wins):

1. **User library** (development): `~/OFX/Plugins/AnyComfy.ofx.bundle/Contents/Resources/config/defaults.json`
2. **User library** (production): `~/Library/OFX/Plugins/AnyComfy.ofx.bundle/Contents/Resources/config/defaults.json`
3. **System library**: `/Library/OFX/Plugins/AnyComfy.ofx.bundle/Contents/Resources/config/defaults.json`

**If no configuration file is found**, the plugin uses hardcoded C++ defaults.

---

## Configuration File Format

The configuration file is a JSON document with three main sections:

```json
{
  "server": {
    "serverAddress": "192.168.1.211",
    "serverPort": 8188,
    "sharedMountPath": "/Volumes/silo2/002_COMFYUI",
    "serverMountPoint": "Z:",
    "comfyUIInputDir": "/Volumes/silo2/002_COMFYUI/in"
  },
  "controls": {
    "enableProcessing": false,
    "enableCache": true,
    "timeout": 300,
    "asyncMode": 1,
    "placeholderMode": 1
  },
  "project": {
    "workflowName": "my_vfx",
    "outputVersion": "v001"
  }
}
```

---

## Configuration Sections

### 1. Server Configuration

Controls ComfyUI server connection and path mapping.

| Parameter | Type | Default (hardcoded) | Description |
|-----------|------|---------------------|-------------|
| `serverAddress` | string | `"localhost"` | ComfyUI server IP address or hostname |
| `serverPort` | integer | `8188` | ComfyUI server REST API port |
| `sharedMountPath` | string | `"/Volumes/silo2/002_COMFYUI"` | macOS/Linux path to shared storage |
| `serverMountPoint` | string | `"Z:"` | Windows drive letter for shared storage |
| `comfyUIInputDir` | string | `"/Volumes/silo2/002_COMFYUI/in"` | Directory for workflow auto-loading |

**Example Use Cases:**

- **Production Server:** Set `serverAddress` to your dedicated ComfyUI server IP
- **Local Development:** Set `serverAddress` to `"localhost"` or `"127.0.0.1"`
- **Network Storage:** Configure `sharedMountPath` to match your NAS mount point

### 2. Controls Configuration

Default settings for processing behavior.

| Parameter | Type | Default (hardcoded) | Description |
|-----------|------|---------------------|-------------|
| `enableProcessing` | boolean | `false` | Enable ComfyUI processing on plugin load |
| `enableCache` | boolean | `true` | Enable output file caching |
| `timeout` | integer | `300` | Server timeout in seconds (5 minutes) |
| `asyncMode` | integer | `1` | Rendering mode: `0` = blocking, `1` = non-blocking |
| `placeholderMode` | integer | `1` | Placeholder display: `0` = checkerboard, `1` = last valid frame, `2` = solid color |

**asyncMode Values:**
- `0` - **Blocking**: UI freezes during rendering, shows final result
- `1` - **Non-blocking**: Returns immediately with placeholder, processes in background

**placeholderMode Values:**
- `0` - **Checkerboard**: 32x32 gray checkerboard pattern
- `1` - **Last Valid Frame**: Use previous successfully rendered frame
- `2` - **Solid Color**: Gray solid color (50% gray)

### 3. Project Configuration

Default project organization settings.

| Parameter | Type | Default (hardcoded) | Description |
|-----------|------|---------------------|-------------|
| `workflowName` | string | `""` (empty) | Default workflow subdirectory name |
| `outputVersion` | string | `"v001"` | Default output version tag |

**Note:** For AnyComfy plugin, `workflowName` is auto-derived from the selected workflow file and this default is not used.

---

## How It Works

### 1. Plugin Loads Configuration on Startup

During plugin factory initialization (`describeInContext`):

```cpp
// Load config from bundle
json configDefaults = BasePlugin::loadConfigDefaults();

// Pass to parameter setup
BasePlugin::describeCommonParameters(desc, context,
    projectPage, processingPage, serverPage, &configDefaults);
```

### 2. Parameters Use Config Values

Each parameter checks the config and falls back to hardcoded defaults:

```cpp
// Hardcoded fallback
std::string serverAddressDefault = "localhost";

// Override from config if available
if (configDefaults &&
    configDefaults->contains("server") &&
    (*configDefaults)["server"].contains("serverAddress")) {
    serverAddressDefault = (*configDefaults)["server"]["serverAddress"].get<std::string>();
}

// Set parameter default
serverAddr->setDefault(serverAddressDefault.c_str());
```

### 3. Logging Confirms Config Loading

Check log output to verify configuration:

```
[info] === BasePlugin::loadConfigDefaults() called ===
[info] ✓ Successfully loaded config from: /Users/julien/Library/OFX/Plugins/AnyComfy.ofx.bundle/Contents/Resources/config/defaults.json
[info] Config contents: {
  "server": {
    "serverAddress": "192.168.1.211",
    ...
  }
}
```

---

## Customization Guide

### Step 1: Locate Your Plugin Bundle

**macOS:**
```bash
# User library (most common)
~/Library/OFX/Plugins/AnyComfy.ofx.bundle/

# Development builds
~/OFX/Plugins/AnyComfy.ofx.bundle/

# System-wide
/Library/OFX/Plugins/AnyComfy.ofx.bundle/
```

**Linux:**
```bash
/usr/OFX/Plugins/AnyComfy.ofx.bundle/
```

**Windows:**
```
%COMMONPROGRAMFILES%/OFX/Plugins/AnyComfy.ofx.bundle/
```

### Step 2: Edit Configuration File

```bash
# Navigate to bundle
cd ~/Library/OFX/Plugins/AnyComfy.ofx.bundle/Contents/Resources/config/

# Edit with text editor
nano defaults.json
# or
code defaults.json
# or
open -a TextEdit defaults.json
```

### Step 3: Modify Values

Edit only the values you want to change. All fields are optional.

**Example: Change server address only**
```json
{
  "server": {
    "serverAddress": "192.168.1.100"
  }
}
```

All other parameters will use hardcoded defaults.

### Step 4: Restart Host Application

The configuration is loaded when the plugin is initialized. Restart your OFX host (Flame, Nuke, etc.) to apply changes.

---

## Deployment Scenarios

### Scenario 1: Production Studio with Dedicated ComfyUI Server

**Setup:**
- ComfyUI server: `comfy-server.studio.local` (192.168.10.50)
- Shared storage: `/mnt/projects/comfy` (macOS) → `S:\` (Windows)
- Default timeout: 10 minutes

**Configuration:**
```json
{
  "server": {
    "serverAddress": "192.168.10.50",
    "serverPort": 8188,
    "sharedMountPath": "/mnt/projects/comfy",
    "serverMountPoint": "S:",
    "comfyUIInputDir": "/mnt/projects/comfy/input"
  },
  "controls": {
    "enableProcessing": true,
    "enableCache": true,
    "timeout": 600,
    "asyncMode": 1,
    "placeholderMode": 1
  }
}
```

### Scenario 2: Artist Workstation with Local ComfyUI

**Setup:**
- ComfyUI running locally
- Local storage only
- Fast operations, short timeout

**Configuration:**
```json
{
  "server": {
    "serverAddress": "127.0.0.1",
    "serverPort": 8188,
    "sharedMountPath": "/Users/artist/ComfyUI",
    "serverMountPoint": "C:\\ComfyUI",
    "comfyUIInputDir": "/Users/artist/ComfyUI/input"
  },
  "controls": {
    "enableProcessing": true,
    "timeout": 180,
    "asyncMode": 1
  }
}
```

### Scenario 3: Multi-Site Deployment

**For remote office with different network:**
- ComfyUI server: Different IP per site
- Shared storage: Site-specific NAS

**Site A (Los Angeles):**
```json
{
  "server": {
    "serverAddress": "10.1.50.100",
    "sharedMountPath": "/Volumes/nas-la/comfy"
  }
}
```

**Site B (Vancouver):**
```json
{
  "server": {
    "serverAddress": "10.2.50.100",
    "sharedMountPath": "/Volumes/nas-van/comfy"
  }
}
```

Deploy appropriate config file to each site's plugin installation.

---

## Troubleshooting

### Configuration Not Loading

**Symptom:** Plugin uses localhost instead of configured server address

**Solutions:**

1. **Verify File Location:**
   ```bash
   ls -la ~/Library/OFX/Plugins/AnyComfy.ofx.bundle/Contents/Resources/config/defaults.json
   ```

2. **Check File Permissions:**
   ```bash
   chmod 644 ~/Library/OFX/Plugins/AnyComfy.ofx.bundle/Contents/Resources/config/defaults.json
   ```

3. **Validate JSON Syntax:**
   ```bash
   python3 -m json.tool defaults.json
   ```
   Should print formatted JSON without errors.

4. **Check Plugin Logs:**
   ```bash
   # Look for config loading messages
   grep "loadConfigDefaults" ~/Library/Logs/ComfyUI/*.log
   ```

### Invalid JSON Syntax

**Symptom:** Plugin fails to load or uses hardcoded defaults

**Common Mistakes:**

❌ **Trailing comma:**
```json
{
  "server": {
    "serverAddress": "192.168.1.1",  // ← Remove this comma
  }
}
```

✅ **Correct:**
```json
{
  "server": {
    "serverAddress": "192.168.1.1"
  }
}
```

❌ **Missing quotes:**
```json
{
  "server": {
    serverAddress: "192.168.1.1"  // ← Add quotes around key
  }
}
```

✅ **Correct:**
```json
{
  "server": {
    "serverAddress": "192.168.1.1"
  }
}
```

### Partial Configuration

**Question:** Can I specify only some parameters?

**Answer:** Yes! All parameters are optional. Specify only what you want to override.

**Minimal Example (server address only):**
```json
{
  "server": {
    "serverAddress": "192.168.1.211"
  }
}
```

All other parameters use hardcoded defaults.

---

## Advanced Topics

### Per-Plugin Configuration

Each plugin can have its own configuration:

- `AnyComfy.ofx.bundle/Contents/Resources/config/defaults.json`
- `SAMSegmentation.ofx.bundle/Contents/Resources/config/defaults.json`

Configuration is loaded per-plugin, not globally.

### Version Control for Configurations

**Recommended:** Keep configuration templates in version control:

```
project-configs/
├── configs/
│   ├── production.json
│   ├── development.json
│   └── artist-workstation.json
└── deploy.sh
```

**Deployment script:**
```bash
#!/bin/bash
cp configs/production.json \
   ~/Library/OFX/Plugins/AnyComfy.ofx.bundle/Contents/Resources/config/defaults.json
```

### Environment-Specific Configs

Use different configs for different environments:

```bash
# Development
ln -sf configs/dev.json \
   ~/Library/OFX/Plugins/AnyComfy.ofx.bundle/Contents/Resources/config/defaults.json

# Production
ln -sf configs/prod.json \
   ~/Library/OFX/Plugins/AnyComfy.ofx.bundle/Contents/Resources/config/defaults.json
```

---

## Configuration Schema Reference

### Complete Schema with Types

```json
{
  "server": {
    "serverAddress": "<string>",         // IP or hostname
    "serverPort": <integer>,             // 1-65535
    "sharedMountPath": "<string>",       // Absolute path (macOS/Linux)
    "serverMountPoint": "<string>",      // Drive letter (Windows, e.g. "Z:")
    "comfyUIInputDir": "<string>"        // Absolute path
  },
  "controls": {
    "enableProcessing": <boolean>,       // true or false
    "enableCache": <boolean>,            // true or false
    "timeout": <integer>,                // Seconds (1-3600)
    "asyncMode": <integer>,              // 0 or 1
    "placeholderMode": <integer>         // 0, 1, or 2
  },
  "project": {
    "workflowName": "<string>",          // Subdirectory name
    "outputVersion": "<string>"          // Version tag (e.g. "v001")
  }
}
```

### Validation Rules

- All sections are optional
- All parameters within sections are optional
- Unknown sections/parameters are ignored (forward compatibility)
- Invalid types cause parameter to use hardcoded default
- Invalid JSON causes entire config to be ignored

---

## See Also

- [Session 16 Documentation](progress/SESSION_16_CONFIGURATION_AND_OPTIMIZATION.md) - Implementation details
- [AnyComfy Plugin README](../plugins/ComfyUI/anycomfy/README.md) - Plugin-specific configuration
- [Developer Guide](guides/developer-guide.md) - Adding new configurable parameters

---

**Last Updated:** 2026-01-21

# OFX Plugin Configuration

This directory contains JSON configuration files that define default parameter values for the AnyComfy OFX plugin.

## Overview

The configuration system allows you to customize the default values of plugin parameters without modifying the source code. This is useful for:

- Site-specific defaults (e.g., your ComfyUI server address)
- User preferences (e.g., default timeout, cache settings)
- Project templates (e.g., standard workflow names, version formats)

## Configuration File

The main configuration file is `defaults.json`.

### Location

When the plugin is built and bundled, this file will be located at:
```
~/Library/OFX/Plugins/AnyComfy.ofx.bundle/Contents/Resources/config/defaults.json
```

### File Format

The configuration file is a JSON document with three main sections:

```json
{
  "server": {
    "serverAddress": "localhost",
    "serverPort": 8188,
    "macMountPath": "/Volumes/silo2/002_COMFYUI",
    "winMountPath": "\\\\192.168.1.110\\silo2\\002_COMFYUI",
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
    "workflowName": "segmentation",
    "outputVersion": "v001"
  }
}
```

## Configuration Sections

### Server Configuration

Controls ComfyUI server connection and file system paths:

- **serverAddress** (string): ComfyUI server hostname or IP address
  - Default: `"localhost"`
  - Example: `"192.168.1.100"`, `"comfyui-server.local"`

- **serverPort** (integer): ComfyUI server port number
  - Default: `8188`
  - Range: 1-65535

- **macMountPath** (string): macOS client mount path
  - Default: `"/Volumes/silo2/002_COMFYUI"`
  - Example: `"/Volumes/storage/comfyui"`

- **winMountPath** (string): Windows server mount path (UNC format)
  - Default: `"\\\\192.168.1.110\\silo2\\002_COMFYUI"`
  - Example: `"\\\\server\\share"`

- **comfyUIInputDir** (string): Path to ComfyUI input directory
  - Default: `"/Volumes/silo2/002_COMFYUI/in"`
  - Should match ComfyUI's `--input-directory` flag

### Controls Configuration

Controls plugin behavior and processing options:

- **enableProcessing** (boolean): Enable ComfyUI processing by default
  - Default: `false` (recommended for smooth UI loading)
  - Set to `true` to enable processing immediately

- **enableCache** (boolean): Enable ComfyUI caching system
  - Default: `true`
  - Improves performance for repeated renders

- **timeout** (integer): Maximum processing time in seconds
  - Default: `300` (5 minutes)
  - Range: 10-3600

- **asyncMode** (integer): Rendering mode
  - `0` = Blocking (wait for result)
  - `1` = Non-blocking (progressive rendering, default)

- **placeholderMode** (integer): What to show during async processing
  - `0` = Source passthrough
  - `1` = Checkerboard pattern (default)
  - `2` = Gray frame
  - `3` = Last valid result

### Project Configuration

Controls default project organization:

- **workflowName** (string): Default workflow subdirectory name
  - Default: `"segmentation"`
  - Example: `"upscale"`, `"denoise"`, `"my_effect"`

- **outputVersion** (string): Default version identifier
  - Default: `"v001"`
  - Format: Any string (commonly `"v001"`, `"v002"`, etc.)

## Customizing Defaults

### For Your Studio

1. Build and install the plugin
2. Navigate to the bundle resources:
   ```bash
   cd ~/Library/OFX/Plugins/AnyComfy.ofx.bundle/Contents/Resources/config/
   ```
3. Edit `defaults.json`:
   ```bash
   nano defaults.json
   ```
4. Update the values to match your studio setup
5. Restart your OFX host application

### Example: Remote ComfyUI Server

```json
{
  "server": {
    "serverAddress": "render-farm.studio.local",
    "serverPort": 8188,
    "macMountPath": "/Volumes/render-storage/comfyui",
    "winMountPath": "\\\\render-farm\\storage\\comfyui",
    "comfyUIInputDir": "/Volumes/render-storage/comfyui/input"
  },
  "controls": {
    "enableProcessing": false,
    "enableCache": true,
    "timeout": 600,
    "asyncMode": 1,
    "placeholderMode": 1
  },
  "project": {
    "workflowName": "fx",
    "outputVersion": "v001"
  }
}
```

### Example: Local Development

```json
{
  "server": {
    "serverAddress": "localhost",
    "serverPort": 8188,
    "macMountPath": "/Users/artist/ComfyUI",
    "winMountPath": "/Users/artist/ComfyUI",
    "comfyUIInputDir": "/Users/artist/ComfyUI/input"
  },
  "controls": {
    "enableProcessing": true,
    "enableCache": true,
    "timeout": 120,
    "asyncMode": 1,
    "placeholderMode": 1
  },
  "project": {
    "workflowName": "test",
    "outputVersion": "dev"
  }
}
```

## Fallback Behavior

If the configuration file is missing or cannot be parsed:
- The plugin will use hardcoded default values
- A warning will be logged to `~/comfyui_plugin_YYYYMMDD.log`
- The plugin will continue to function normally

## Troubleshooting

### Configuration Not Loading

1. **Check file location**:
   ```bash
   ls -la ~/Library/OFX/Plugins/AnyComfy.ofx.bundle/Contents/Resources/config/defaults.json
   ```

2. **Verify JSON syntax**:
   ```bash
   cat ~/Library/OFX/Plugins/AnyComfy.ofx.bundle/Contents/Resources/config/defaults.json | python -m json.tool
   ```

3. **Check plugin log**:
   ```bash
   tail -f ~/comfyui_plugin_$(date +%Y%m%d).log
   ```

### Invalid JSON Syntax

Common JSON errors:
- Missing quotes around strings: `"serverAddress": localhost` ❌ → `"serverAddress": "localhost"` ✓
- Trailing commas: `"timeout": 300,` ❌ (last item in object)
- Wrong quotes: `'timeout': 300` ❌ → `"timeout": 300` ✓
- Missing braces: Check that all `{` have matching `}`

## Development

### Testing Configuration Changes

1. Edit the config file in the source tree:
   ```bash
   nano contrib/plugins/ComfyUI/anycomfy/resources/config/defaults.json
   ```

2. Rebuild and install the plugin:
   ```bash
   ./contrib/dev-tools/build-plugin.sh contrib/plugins/ComfyUI/anycomfy AnyComfy-support
   ```

3. Check that the config was bundled:
   ```bash
   cat ~/Library/OFX/Plugins/AnyComfy.ofx.bundle/Contents/Resources/config/defaults.json
   ```

4. Test in your OFX host application

### Adding New Parameters

To add new configurable parameters:

1. Add the parameter to `defaults.json`
2. Update `BasePlugin::describeCommonParameters()` to read the config value
3. Update this README with documentation

## See Also

- [ComfyUI Plugin README](../../README.md) - Main plugin documentation
- [OFX Specification](https://openeffects.org/) - OpenFX API reference

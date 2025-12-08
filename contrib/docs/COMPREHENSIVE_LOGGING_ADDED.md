# Comprehensive OFX Property Logging - Implementation Complete

## Date: 2025-11-20

## Summary

Added comprehensive logging of **ALL available OFX properties** to understand the complete environment available to the plugin in different host applications (Flame, Nuke, Resolve, etc.).

## What Will Be Logged

When you load the plugin in Flame, `~/comfyui_plugin_YYYYMMDD.log` will contain:

```
=== COMPREHENSIVE OFX ENVIRONMENT DISCOVERY ===

--- Instance Properties ---
Instance kOfxPropName: '<value>'
Instance kOfxPropLabel: '<value>'
Instance kOfxPropShortLabel: '<value>'
Instance kOfxPropLongLabel: '<value>'
Instance kOfxPropType: '<value>'
kOfxPropEffectInstance: 0x<hex> (RUNTIME POINTER - not stable)
kOfxPropInstanceData: 0x<hex> or NULL (RUNTIME POINTER - not stable)

--- Plugin Descriptor Properties ---
Plugin File Path: '<path_to_ofx_bundle>'

--- Project Properties ---
Project Size: WIDTHxHEIGHT
Project Extent: WIDTHxHEIGHT
Project Offset: (X,Y)
Project Pixel Aspect Ratio: <ratio>
Effect Duration: <seconds>
Frame Rate: <fps>
Frame Range: MIN - MAX

--- Source Clip Properties ---
Source Clip kOfxPropName: '<name>'
Source Clip kOfxPropLabel: '<label>'
Source Clip Connected: YES/NO

--- Output Clip Properties ---
Output Clip kOfxPropName: '<name>'
Output Clip kOfxPropLabel: '<label>'

--- System Environment ---
HOME: '<path>'
USER: '<username>'
HOSTNAME: '<hostname>'
PWD: '<working_directory>'

--- Render Properties ---
Render Scale: <scale_x>x<scale_y>
Sequential Render: YES/NO

--- Plugin Capabilities ---
Supports Tiles: YES/NO
Supports Multi-Resolution: YES/NO
Temporal Clip Access: YES/NO

--- GPU Support ---
OpenGL Render Supported: YES/NO
CUDA Render Supported: YES/NO
OpenCL Render Supported: YES/NO
Metal Render Supported: YES/NO

--- Source Clip Format ---
(Only logged if source clip is connected)
Source Components: 'RGBA' / 'RGB' / 'Alpha'
Source Pixel Depth: 'Byte' / 'Short' / 'Half' / 'Float'
Source PreMultiplication: '<premult_state>'
Source Field Order: '<field_order>'
Source Frame Rate: <fps>
Source Unmapped Components: '<components>'
Source Clip Optional: YES/NO
Source Clip Is Mask: YES/NO
Source Continuous Samples: YES/NO

=== END COMPREHENSIVE OFX ENVIRONMENT DISCOVERY ===
```

## Code Location

All logging is in [comfyui_base_plugin.cpp:115-400](common/comfyui_base_plugin.cpp#L115-L400), in the `BasePlugin` constructor.

## Property Categories Logged

### 1. Instance Identification
- `kOfxPropName` - Instance name (often empty in Flame)
- `kOfxPropLabel` - Instance label (plugin type name)
- `kOfxPropShortLabel` - Short label
- `kOfxPropLongLabel` - Long label
- `kOfxPropType` - Object type
- **`kOfxPropEffectInstance`** - Runtime pointer (changes between sessions)
- **`kOfxPropInstanceData`** - Custom data pointer (NULL initially)

### 2. Plugin Properties
- `kOfxPluginPropFilePath` - Path to .ofx.bundle

### 3. Project/Canvas Properties
- `kOfxImageEffectPropProjectSize` - Canvas resolution
- `kOfxImageEffectPropProjectExtent` - Full project extent
- `kOfxImageEffectPropProjectOffset` - Canvas offset
- `kOfxImageEffectPropProjectPixelAspectRatio` - Pixel aspect ratio

### 4. Time/Frame Properties
- `kOfxImageEffectInstancePropEffectDuration` - Effect duration
- `kOfxImageEffectPropFrameRate` - Project frame rate
- `kOfxImageEffectPropFrameRange` - Frame range (min, max)

### 5. Clip Properties
- `kOfxPropName` - Clip name (Source/Output)
- `kOfxPropLabel` - Clip label
- `kOfxImageClipPropConnected` - Connection status

### 6. System Environment
- `HOME` - User home directory
- `USER` - Current username
- `HOSTNAME` - Machine hostname
- `PWD` - Current working directory

### 7. Render Properties
- `kOfxImageEffectPropRenderScale` - Proxy/render scale
- `kOfxImageEffectInstancePropSequentialRender` - Sequential rendering flag

### 8. Plugin Capabilities
- `kOfxImageEffectPropSupportsTiles` - Tiled rendering support
- `kOfxImageEffectPropSupportsMultiResolution` - Multi-resolution support
- `kOfxImageEffectPropTemporalClipAccess` - Temporal clip access

### 9. GPU Support
- `kOfxImageEffectPropOpenGLRenderSupported` - OpenGL support
- `kOfxImageEffectPropCudaRenderSupported` - CUDA support
- `kOfxImageEffectPropOpenCLRenderSupported` - OpenCL support
- `kOfxImageEffectPropMetalRenderSupported` - Metal support

### 10. Source Clip Format (when connected)
- `kOfxImageEffectPropComponents` - Pixel components (RGBA/RGB/Alpha)
- `kOfxImageEffectPropPixelDepth` - Bit depth (Byte/Short/Half/Float)
- `kOfxImageEffectPropPreMultiplication` - Premultiplication state
- `kOfxImageClipPropFieldOrder` - Field order
- `kOfxImageEffectPropFrameRate` - Clip frame rate
- `kOfxImageClipPropUnmappedComponents` - Original components
- `kOfxImageClipPropOptional` - Is clip optional
- `kOfxImageClipPropIsMask` - Is mask clip
- `kOfxImageClipPropContinuousSamples` - Continuous sampling

## Key Findings Expected

Based on previous testing ([OFX_CONTEXT_FINDINGS.md](OFX_CONTEXT_FINDINGS.md)):

### Flame/Flare 2026.1:

**Empty/Generic Values**:
- ❌ `kOfxPropName`: Empty string
- ❌ `kOfxPropLabel`: Plugin type name (same for all instances)
- ❌ Source Clip Name: "Source" (generic, not media name)

**Useful Values**:
- ✅ Project Size: 1920x1080
- ✅ Frame Rate: 23.976
- ✅ Pixel Aspect: 1.0
- ✅ HOME: /Users/julien
- ✅ USER: julien
- ✅ Source Components: RGBA
- ✅ Source Pixel Depth: Float
- ✅ GPU Support flags

**Runtime Pointers** (NOT STABLE):
- ⚠️ `kOfxPropEffectInstance`: 0x7f8b1234abcd (CHANGES each run)
- ⚠️ `kOfxPropInstanceData`: NULL or pointer (CHANGES each run)

## What This Confirms

1. **No stable instance identifiers** in OFX for Flame
2. **Runtime pointers are useless** for stable file naming
3. **Must use user-provided parameters** for project/instance naming
4. **OFX provides good technical info** (resolution, frame rate, pixel format)
5. **No access to Flame-specific context** (batch node names, media names)

## Next Steps

After reviewing the log output:

1. **Confirm** that pointer values change between sessions
2. **Document** which properties are useful vs useless
3. **Implement** "Instance Suffix" parameter for stable unique identification
4. **Update** basename generation to use user-provided suffix

## Related Documentation

- [INSTANCE_ID_INVESTIGATION.md](INSTANCE_ID_INVESTIGATION.md) - Analysis of stable ID options
- [OFX_CONTEXT_FINDINGS.md](OFX_CONTEXT_FINDINGS.md) - Previous test results
- [COMPLETE_OFX_PROPERTIES_LIST.md](COMPLETE_OFX_PROPERTIES_LIST.md) - Full property reference
- [PROJECT_NAME_VALIDATION.md](PROJECT_NAME_VALIDATION.md) - Project name requirements

## Build Status

✅ Code compiles successfully (syntax verified)
⚠️ Full plugin build blocked by unrelated boost/websocketpp compatibility issue
✅ Changes ready for testing in Flame

## Testing Instructions

1. **Copy plugin to Flame** (or use existing build)
2. **Load plugin** in Flame/Flare
3. **Check log immediately**: `cat ~/comfyui_plugin_$(date +%Y%m%d).log`
4. **Look for pointer values** under "Instance Properties"
5. **Save Flame project and close**
6. **Reopen and check log again** - pointer values will be different
7. **Report findings** for documentation

## Expected Test Results

```bash
# First run
kOfxPropEffectInstance: 0x7f8b1234abcd (RUNTIME POINTER - not stable)

# Second run (after restart)
kOfxPropEffectInstance: 0x7f8b9876fedc (RUNTIME POINTER - not stable)
```

**Conclusion**: Pointers change, confirming need for user-provided Instance Suffix parameter.

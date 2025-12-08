# Complete List of OFX Properties Available

## Core Properties (kOfxProp*)

### Available on Multiple Objects

- `kOfxPropName` - Unique name/identifier (string)
- `kOfxPropLabel` - User-visible label (string)
- `kOfxPropShortLabel` - Short label ≤13 chars (string)
- `kOfxPropLongLabel` - Long label ≤32 chars (string)
- `kOfxPropType` - Object type identifier (string)
- `kOfxPropVersion` - Version number array (int[])
- `kOfxPropVersionLabel` - Human-readable version (string)
- `kOfxPropIcon` - Icon file path (string[2]: SVG, PNG)

### Plugin/Effect Specific

- `kOfxPluginPropFilePath` - Path to plugin binary (string)
- `kOfxPropPluginDescription` - Plugin description (string)
- `kOfxPropEffectInstance` - Effect instance handle (pointer)
- `kOfxPropInstanceData` - Custom instance data (pointer)

### Time/Interaction

- `kOfxPropTime` - Current time value (double)
- `kOfxPropIsInteractive` - Interactive render? (int: 0/1)
- `kOfxPropChangeReason` - Why parameter changed (string)

### Host

- `kOfxPropAPIVersion` - OFX API version (int[])
- `kOfxPropHostOSHandle` - OS window handle (pointer)

## Image Effect Properties (kOfxImageEffectProp*)

### Effect Context

- `kOfxImageEffectPropContext` - Effect context type (string)
  - Values: Filter, Generator, Transition, Paint, Retimer, General
- `kOfxImageEffectPropPluginHandle` - Plugin handle (pointer)

### Project/Canvas Properties

- `kOfxImageEffectPropProjectSize` - Canvas size [w,h] (int[2])
- `kOfxImageEffectPropProjectExtent` - Full extent [w,h] (int[2])
- `kOfxImageEffectPropProjectOffset` - Canvas offset [x,y] (int[2])
- `kOfxImageEffectPropProjectPixelAspectRatio` - Pixel aspect (double)

### Frame/Time Properties

- `kOfxImageEffectPropFrameRate` - Frame rate (double)
- `kOfxImageEffectPropFrameRange` - Frame range [min,max] (double[2])
- `kOfxImageEffectPropFrameStep` - Frame step (double)
- `kOfxImageEffectPropUnmappedFrameRate` - Unmapped frame rate (double)
- `kOfxImageEffectPropUnmappedFrameRange` - Unmapped range (double[2])

### Render Properties

- `kOfxImageEffectPropRenderScale` - Render scale [x,y] (double[2])
- `kOfxImageEffectPropRenderWindow` - Render region [x1,y1,x2,y2] (int[4])
- `kOfxImageEffectPropRenderQualityDraft` - Draft quality? (int: 0/1)
- `kOfxImageEffectPropSequentialRenderStatus` - Sequential render? (int: 0/1)
- `kOfxImageEffectPropInteractiveRenderStatus` - Interactive render? (int: 0/1)
- `kOfxImageEffectPropFieldToRender` - Which field to render (string)
- `kOfxImageEffectPropRegionOfDefinition` - RoD [x1,y1,x2,y2] (double[4])
- `kOfxImageEffectPropRegionOfInterest` - RoI [x1,y1,x2,y2] (double[4])

### Image Format Properties

- `kOfxImageEffectPropComponents` - Pixel components (string)
  - Values: RGBA, RGB, Alpha, None
- `kOfxImageEffectPropPixelDepth` - Bit depth (string)
  - Values: Byte, Short, Half, Float, None
- `kOfxImageEffectPropPreMultiplication` - Premultiplied alpha (string)
- `kOfxImageEffectPropSupportedComponents` - Supported components (string[])
- `kOfxImageEffectPropSupportedPixelDepths` - Supported depths (string[])

### Plugin Capabilities

- `kOfxImageEffectPropSupportedContexts` - Supported contexts (string[])
- `kOfxImageEffectPropSupportsMultipleClipDepths` - Multiple depths? (int: 0/1)
- `kOfxImageEffectPropSupportsMultipleClipPARs` - Multiple PARs? (int: 0/1)
- `kOfxImageEffectPropSupportsMultiResolution` - Multi-res? (int: 0/1)
- `kOfxImageEffectPropSupportsTiles` - Tiled rendering? (int: 0/1)
- `kOfxImageEffectPropSupportsOverlays` - Overlays supported? (int: 0/1)
- `kOfxImageEffectPropTemporalClipAccess` - Temporal access? (int: 0/1)
- `kOfxImageEffectPropNoSpatialAwareness` - Not spatially aware? (int: 0/1)

### Fielding

- `kOfxImageEffectPropSetableFielding` - Can set fielding? (int: 0/1)
- `kOfxImageEffectPropSetableFrameRate` - Can set frame rate? (int: 0/1)

### GPU Properties

- `kOfxImageEffectPropOpenGLEnabled` - OpenGL available? (int: 0/1)
- `kOfxImageEffectPropOpenGLRenderSupported` - OpenGL render? (int: 0/1)
- `kOfxImageEffectPropOpenGLTextureIndex` - GL texture index (int)
- `kOfxImageEffectPropOpenGLTextureTarget` - GL texture target (int)
- `kOfxImageEffectPropCudaEnabled` - CUDA available? (int: 0/1)
- `kOfxImageEffectPropCudaRenderSupported` - CUDA render? (int: 0/1)
- `kOfxImageEffectPropCudaStream` - CUDA stream (pointer)
- `kOfxImageEffectPropOpenCLEnabled` - OpenCL available? (int: 0/1)
- `kOfxImageEffectPropOpenCLRenderSupported` - OpenCL render? (int: 0/1)
- `kOfxImageEffectPropOpenCLCommandQueue` - OpenCL queue (pointer)
- `kOfxImageEffectPropMetalEnabled` - Metal available? (int: 0/1)
- `kOfxImageEffectPropMetalRenderSupported` - Metal render? (int: 0/1)
- `kOfxImageEffectPropMetalCommandQueue` - Metal queue (pointer)

### Colour Management

- `kOfxImageEffectPropColourManagementStyle` - CM style (string)
- `kOfxImageEffectPropColourManagementConfig` - CM config (string)
- `kOfxImageEffectPropColourManagementAvailableConfigs` - Available configs (string[])

## Image Effect Instance Properties (kOfxImageEffectInstanceProp*)

- `kOfxImageEffectInstancePropEffectDuration` - Effect duration (double)
- `kOfxImageEffectInstancePropSequentialRender` - Sequential? (int: 0/1)

## Clip Properties (kOfxImageClipProp*)

### Connection/Status

- `kOfxImageClipPropConnected` - Is clip connected? (int: 0/1)
- `kOfxImageClipPropOptional` - Is clip optional? (int: 0/1)
- `kOfxImageClipPropIsMask` - Is this a mask clip? (int: 0/1)

### Image Format

- `kOfxImageClipPropUnmappedComponents` - Original components (string)
- `kOfxImageClipPropUnmappedPixelDepth` - Original bit depth (string)

### Fielding/Sampling

- `kOfxImageClipPropFieldOrder` - Field order (string)
- `kOfxImageClipPropFieldExtraction` - Field extraction (string)
- `kOfxImageClipPropContinuousSamples` - Continuous? (int: 0/1)

## System Environment Variables (Not OFX, but accessible)

- `HOME` - User home directory
- `USER` - Current user name
- `HOSTNAME` - Machine hostname
- `PWD` - Current working directory
- Platform-specific environment variables

## NOT Available in OFX

These are commonly requested but **NOT** provided by OFX:

❌ Project name
❌ Shot/sequence name
❌ Scene/composition name
❌ Timeline name
❌ Node/instance custom name (beyond kOfxPropName which is often empty)
❌ Connected media filename
❌ Render job ID
❌ Batch/queue name
❌ User custom metadata
❌ File paths (except plugin binary path)

## Key Findings for Our Plugin

### What We CAN Use

1. **Effect Context** - Know if we're Filter/Generator/etc
2. **Project Canvas Size** - Resolution info
3. **Frame Rate/Range** - Timing information
4. **Render Status** - Interactive vs batch
5. **Clip Connection Status** - Know if input connected
6. **Clip Components** - RGBA/RGB/Alpha
7. **System Environment** - HOME, USER, PWD

### What We CANNOT Use

1. **Project Name** - Must be user parameter
2. **Shot Name** - Must be user parameter
3. **Media Clip Name** - Clip name is generic "Source", not actual media
4. **Instance Name** - Often empty or generic in Flame
5. **Node Custom Name** - Not accessible via OFX

## Tested Results (Flame/Flare 2026.1)

From actual testing in Autodesk Flare:

```
Effect Context: Filter
Instance kOfxPropName: '' (EMPTY)
Instance kOfxPropLabel: 'ComfyUI SAM Segmentation' (plugin type, not instance)
Project Size: 1920x1080
Project Pixel Aspect Ratio: 1.0
Frame Rate: 23.976
Source Clip kOfxPropName: 'Source' (generic)
Source Clip kOfxPropLabel: '' (EMPTY)
HOME: '/Users/julien'
USER: 'julien'
```

**Conclusion**: For unique naming, we MUST rely on user parameters, not OFX properties.

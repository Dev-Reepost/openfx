# OFX Context Properties Available to Plugins

This document catalogs all context information available to an OFX plugin at runtime through the OpenFX API.

## Summary

**OFX does NOT provide:**
- ❌ Project name
- ❌ Sequence/shot name
- ❌ Scene/composition name
- ❌ Timeline name
- ❌ User name
- ❌ Render job ID
- ❌ Batch/queue context

**OFX DOES provide:**
- ✅ Plugin instance name (unique identifier)
- ✅ Plugin instance label (user-visible name)
- ✅ Host application name/version
- ✅ Current time/frame
- ✅ Project size/extent/pixel aspect
- ✅ Render context (interactive vs background)
- ✅ Clip names

---

## 1. Instance Properties (Effect Instance)

Available via `getPropertySet().propGetString()` on the ImageEffect instance:

### `kOfxPropName` (Instance Name)
```cpp
std::string instanceName = getPropertySet().propGetString(kOfxPropName, false);
```

- **Type**: ASCII C string
- **Description**: Unique name of the effect instance
- **Read Only**: Yes
- **Example Values**: Unknown (host-dependent) - Could be "effect1", "SAM_node_1", or internal UUID
- **Use Case**: Unique identifier for this specific instance among all instances

### `kOfxPropLabel` (Instance Label)
```cpp
std::string instanceLabel = getPropertySet().propGetString(kOfxPropLabel, false);
```

- **Type**: UTF8 C string
- **Description**: User-visible name of the effect instance
- **Read/Write**: Typically readable, sometimes writable
- **Default**: Same as `kOfxPropName` initially
- **Example Values**: "SAM Segmentation 1", "User renamed node"
- **Use Case**: Display name shown to user in UI

### `kOfxPropShortLabel` / `kOfxPropLongLabel`
- Short version (≤13 characters): `kOfxPropShortLabel`
- Long version (≤32 characters): `kOfxPropLongLabel`
- Both default to `kOfxPropName` but reset if `kOfxPropLabel` changes

---

## 2. Host Properties (Host Descriptor)

Available through host descriptor (queried during plugin initialization):

### Host Identification
```cpp
// Would need to access host property set during setHost() or describe()
// Not directly available in ImageEffect instance
```

**Available host properties:**
- `kOfxPropName` - Host application name (e.g., "uk.co.thefoundry.nuke")
- `kOfxPropLabel` - Host display name (e.g., "Nuke", "Flame")
- `kOfxPropVersion` - Host version as integer array (e.g., [14, 0, 2])
- `kOfxPropVersionLabel` - Human-readable version string (e.g., "14.0v2")

### Host Capabilities
- `kOfxImageEffectHostPropIsBackground` - 0=interactive, 1=background render

---

## 3. Render Context Properties

Available in `RenderArguments` passed to `render()`:

### Time/Frame Information
```cpp
void render(const RenderArguments &args) {
    double time = args.time;  // Current effect time
    // Frame number depends on project frame rate
}
```

- `kOfxPropTime` - Current time in effect's time coordinate system
- `kOfxPropIsInteractive` - Is this interactive render or batch?

### Render Scale
- `kOfxImageEffectPropRenderScale` - Scale factor for proxy rendering

---

## 4. Project Properties (Clip/Effect Context)

### Project Size/Extent
Available on effect property set:

- `kOfxImageEffectPropProjectSize` - Project canvas size [width, height]
- `kOfxImageEffectPropProjectExtent` - Full project extent [width, height]
- `kOfxImageEffectPropProjectOffset` - Project window offset [x, y]
- `kOfxImageEffectPropProjectPixelAspectRatio` - Pixel aspect ratio

### Effect Context
- `kOfxImageEffectPropContext` - Effect context type:
  - `kOfxImageEffectContextGenerator`
  - `kOfxImageEffectContextFilter`
  - `kOfxImageEffectContextTransition`
  - `kOfxImageEffectContextPaint`
  - `kOfxImageEffectContextGeneral`
  - `kOfxImageEffectContextRetimer`

---

## 5. Clip Properties

Available via `_srcClip->getPropertySet()`:

### Clip Identification
```cpp
std::string clipName = _srcClip->getPropertySet().propGetString(kOfxPropName);
```

- `kOfxPropName` - Clip name (e.g., "Source", "Output")
- `kOfxPropLabel` - User-visible clip label

### Clip Image Properties
- `kOfxImageEffectPropComponents` - Pixel components (RGBA, RGB, Alpha)
- `kOfxImageEffectPropPixelDepth` - Bit depth (8, 16, 32)
- `kOfxImageEffectPropFrameRate` - Clip frame rate

---

## 6. What's NOT Available

OFX is designed to be host-agnostic and portable. The following are **NOT** provided:

### Project/Scene Context
- ❌ Project name (e.g., "MyCommercial_2024")
- ❌ Shot/sequence name (e.g., "shot_010")
- ❌ Scene/composition name
- ❌ Timeline/sequence identifier

### File/Path Context
- ❌ Project file path
- ❌ Render output path
- ❌ Media cache location

### Render/Batch Context
- ❌ Render job ID
- ❌ Batch/queue name
- ❌ Farm node identifier

### User/System Context
- ❌ User name
- ❌ System hostname
- ❌ Render farm context

---

## Implementation Notes for ComfyUI Plugin

### What We Can Use

1. **Instance Name** (`kOfxPropName`):
   - Available via `getPropertySet().propGetString(kOfxPropName)`
   - May provide useful identifier (host-dependent)
   - Could be: "effect_1", "SAM_node_1", or UUID
   - **Currently implemented** - stored in `_instanceName`

2. **Instance Label** (`kOfxPropLabel`):
   - User's custom name for the node
   - Falls back to plugin name if not renamed
   - **Currently implemented** - fallback if kOfxPropName empty

3. **Current Frame** (`args.time`):
   - Available in render arguments
   - **Already used** for frame numbering

### What We Must Request from User

Since OFX doesn't provide project/shot context:

1. **Project Name** - User parameter `_projectName`
   - ✅ Already implemented
   - Host-agnostic (works in Nuke, Flame, Resolve, etc.)

2. **Basename** - User parameter `_basename` with auto-generation
   - ✅ Now implemented with auto-generation option
   - When auto-generate enabled: `{project}_{instance_name}`
   - When disabled: manual user input (e.g., "shot01")

### Auto-Basename Pattern

Following Pybox convention:
```python
# Pybox (has access to Flame project API)
basename = f"{flame.project.name}_{flame.batch.node.name}"

# OFX (limited to OFX properties)
basename = f"{projectName}_{instanceName}"
```

Where:
- `projectName` = User parameter (no OFX equivalent)
- `instanceName` = `kOfxPropName` or `kOfxPropLabel` from OFX

---

## Testing Required

When the plugin loads in Flame, check the log to see what values are returned:

```bash
grep "OFX Instance" ~/comfyui_plugin_*.log
```

Expected patterns:
- **Flame**: May provide meaningful names like "SAM_1", "node_segmentation"
- **Nuke**: Likely provides node name (e.g., "SAMSegmentation1")
- **Resolve**: Unknown pattern
- **Generic**: May be UUID or sequential ID

This will determine the usefulness of auto-basename generation across different hosts.

---

## References

- [ofxCore.h](../../../include/ofxCore.h) - Core OFX properties
- [ofxImageEffect.h](../../../include/ofxImageEffect.h) - Image effect properties
- [OpenFX Specification](http://openeffects.org/) - Official documentation

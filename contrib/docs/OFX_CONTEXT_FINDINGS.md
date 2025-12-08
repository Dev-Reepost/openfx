# OFX Context Properties - Flame/Flare Test Results

## Test Date: 2025-11-20
## Host: Autodesk Flare 2026.1 (Build 2026.1.0.207)

---

## Summary of Findings

### ❌ Instance-Specific Names NOT Available

**Critical Finding**: Flame/Flare does **NOT** provide instance-specific identifiers through OFX properties. The properties return generic values that are not useful for auto-generating unique basenames.

---

## Detailed Results

### Instance Properties (Plugin Construction)

```
=== OFX Context Properties Discovery ===
Instance kOfxPropName: ''                          ❌ EMPTY
Instance kOfxPropLabel: 'ComfyUI SAM Segmentation' ⚠️  Plugin type name (not instance)
Instance kOfxPropShortLabel: ''                    ❌ EMPTY
Instance kOfxPropLongLabel: ''                     ❌ EMPTY
```

**Analysis:**
- `kOfxPropName` is **completely empty** - no identifier provided
- `kOfxPropLabel` returns the **plugin type name** ("ComfyUI SAM Segmentation"), not a unique instance identifier
- This value is the same for ALL instances of the plugin
- Useless for distinguishing between multiple nodes

### Clip Properties (Construction Time)

```
--- Source Clip Properties ---
Source Clip kOfxPropName: 'Source'  ⚠️  Generic name
Source Clip kOfxPropLabel: ''       ❌ EMPTY
Source Clip Connected: NO

--- Output Clip Properties ---
Output Clip kOfxPropName: 'Output'  ⚠️  Generic name
Output Clip kOfxPropLabel: ''       ❌ EMPTY
```

**Analysis:**
- Clip names are generic: "Source" and "Output"
- Not the actual connected media clip name
- Same for all instances

### Runtime Clip Properties (During Render)

```
--- Runtime Source Clip Info ---
Source Clip Connected: YES
Source Clip Name: 'Source'          ⚠️  Still generic
Source Clip Label: ''               ❌ EMPTY
Source Clip Components: OfxImageComponentRGBA
```

**Analysis:**
- Even at render time, clip name remains generic "Source"
- Does not reflect the actual input clip/layer/media name
- No useful naming information available

---

## Comparison with Other Hosts

### Expected Behavior in Other Hosts (Unconfirmed)

#### Nuke (Foundry)
**Hypothesis**: Might provide node names
- `kOfxPropName`: Possibly "SAMSegmentation1", "SAMSegmentation2"
- `kOfxPropLabel`: User-renamed node name?
- **Status**: UNTESTED

#### DaVinci Resolve (Blackmagic)
**Hypothesis**: Unknown
- **Status**: UNTESTED

#### General OFX Hosts
**Conclusion**: OFX spec doesn't mandate instance-specific naming, so behavior is host-dependent.

---

## Implications for Auto-Basename Feature

### Current Implementation
```cpp
// Auto-generate mode (when enabled)
basename = projectName + "_" + instanceName;

// With Flame results:
basename = "default_project" + "_" + "";
// Result: "default_project_" (broken!)
```

### Fallback Behavior
Since `instanceName` is empty, the code falls back to `kOfxPropLabel`:
```cpp
if (_instanceName.empty()) {
    _instanceName = getPropertySet().propGetString(kOfxPropLabel, false);
}
```

Result:
```cpp
basename = "default_project" + "_" + "ComfyUI_SAM_Segmentation";
// Result: "default_project_ComfyUI_SAM_Segmentation"
```

### Problem
This basename would be **identical for all instances** of the plugin!

Multiple SAM nodes would all write to:
- `default_project_ComfyUI_SAM_Segmentation.0001.exr`
- `default_project_ComfyUI_SAM_Segmentation.0002.exr`

Causing **file collisions** if multiple instances process the same frame.

---

## Recommendations

### 1. Change Auto-Generate Default (RECOMMENDED)

**Action**: Change `autoGenerateBasename` default from `true` to `false`

```cpp
autoBasename->setDefault(false);  // Change to false
```

**Rationale**:
- Auto-generation doesn't work in Flame/Flare
- Would cause file naming collisions
- Users must provide unique basenames manually

### 2. Update Parameter Hints

**Current**:
```cpp
autoBasename->setHint("Automatically generate basename from project name + node instance name (like Pybox: '{project}_{nodename}'). When disabled, uses manual basename below.");
```

**Updated**:
```cpp
autoBasename->setHint("Automatically generate basename from project name + node instance name. "
                      "WARNING: May not work in all hosts (Flame/Flare doesn't provide unique instance names). "
                      "If multiple nodes produce the same basename, disable this and use manual basename.");
```

### 3. Add Collision Detection (Optional)

Could add a warning when auto-generated basename is not unique:
```cpp
if (autoGenerate && _instanceName == "ComfyUI_SAM_Segmentation") {
    _logger->warn("Auto-generated basename may not be unique - consider using manual basename");
}
```

### 4. Document Host Limitations

Add to user documentation:
```
Auto-Generate Basename Limitations:
- Flame/Flare: Does NOT provide unique instance names - use manual basename
- Nuke: Unknown - needs testing
- Resolve: Unknown - needs testing
```

---

## Pybox vs OFX Comparison

### Pybox (Flame Python API)
```python
# Direct access to Flame internals
flame.project.name              # "TEST_SAM"
flame.batch.current_node.name   # "my_segmentation_node"

# Result: Unique per node
basename = "TEST_SAM_my_segmentation_node"
```

### OFX (Host-Agnostic API)
```cpp
// Limited to OFX properties (host-dependent)
getPropertySet().propGetString(kOfxPropName)   // "" (empty in Flame)
getPropertySet().propGetString(kOfxPropLabel)  // "ComfyUI SAM Segmentation" (same for all)

// Result: NOT unique per node
basename = "default_project_ComfyUI_SAM_Segmentation"
```

**Conclusion**: OFX's host-agnostic design means we can't access host-specific context like Pybox can.

---

## Files Modified

### comfyui_image_io.cpp
**Added**: Automatic directory creation
- `createDirectoryRecursive()` - Recursively creates parent directories
- `getDirectory()` - Extracts directory from file path
- Updated `writeEXR()` to create directories before writing

**Fixes**: "Cannot write a file" error when subdirectories don't exist

**Impact**: Plugin now creates `/in/<project>/<workflow>/` directories automatically

---

## Test Commands

### View OFX Properties from Log
```bash
grep "OFX Context Properties\|Instance kOfx\|Clip kOfx" ~/comfyui_plugin_*.log
```

### Check for Errors
```bash
grep "RENDER FAILED\|ERROR\|Failed to" ~/comfyui_plugin_*.log | tail -20
```

### Monitor Real-Time
```bash
tail -f ~/comfyui_plugin_$(date +%Y%m%d).log
```

---

## Next Steps

1. **Change auto-generate default to `false`** (prevent collisions)
2. **Update parameter hints** (warn about host limitations)
3. **Test in other hosts** (Nuke, Resolve) if available
4. **Consider adding frame/time suffix** to auto-basename as additional uniqueness
5. **Document manual basename requirement** for Flame users

---

## Final Assessment

### What Works ✅
- Plugin successfully loads in Flame/Flare
- Logging system works correctly
- Runtime clip information available
- Directory auto-creation implemented

### What Doesn't Work ❌
- Auto-basename generation in Flame/Flare
- OFX provides no unique instance identifiers
- Clip names are generic, not actual media names

### Recommendation 📝
**Use manual basename by default** and document that users must provide unique names when using multiple instances of the plugin.

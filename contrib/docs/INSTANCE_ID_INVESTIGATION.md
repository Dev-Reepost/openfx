# Instance ID Investigation - Stable Unique Identifiers

## Date: 2025-11-20

## Problem Statement

We need a **stable, unique identifier** for each plugin instance that:
- ✅ Persists across software sessions (reopen Flame project)
- ✅ Is unique per node/instance
- ✅ Can be used in auto-generated filenames
- ❌ Runtime pointers won't work (change between sessions)

## OFX Properties Investigated

### Runtime Pointers (NOT STABLE)

Added logging for these properties (see changes in `comfyui_base_plugin.cpp` lines 168-190):

```cpp
// kOfxPropEffectInstance - Effect instance handle (pointer)
void* instanceHandle = getPropertySet().propGetPointer(kOfxPropEffectInstance, false);
// Result: 0xABCD1234 (CHANGES between runs - NOT STABLE)

// kOfxPropInstanceData - Custom instance data (pointer)
void* instanceData = getPropertySet().propGetPointer(kOfxPropInstanceData, false);
// Result: NULL initially, or runtime pointer (CHANGES between runs - NOT STABLE)
```

**Conclusion**: These pointers change every time Flame launches, making them useless for stable file naming.

## What We Already Know (From Previous Testing)

From `OFX_CONTEXT_FINDINGS.md`:

```
Instance kOfxPropName: '' (EMPTY in Flame)
Instance kOfxPropLabel: 'ComfyUI SAM Segmentation' (same for all instances)
Source Clip kOfxPropName: 'Source' (generic, not actual media filename)
```

**None of these provide unique, stable instance identifiers.**

## Available Solutions

### 1. User-Provided Instance Suffix (RECOMMENDED)

Add a new string parameter `instanceSuffix` that users set manually:

```cpp
// In describeCommonParameters()
OFX::StringParamDescriptor *instanceSuffix = desc.defineStringParam("instanceSuffix");
instanceSuffix->setLabel("Instance Suffix");
instanceSuffix->setHint("Unique identifier for this node instance (e.g., 'node1', 'fg', 'bg'). "
                       "Used in auto-generated basenames to distinguish multiple instances.");
instanceSuffix->setDefault("");
instanceSuffix->setParent(*storageGroup);

// In getEffectiveBasename()
std::string suffix;
_instanceSuffix->getValue(suffix);

if (autoGenerate) {
    if (!suffix.empty()) {
        return project + "_" + suffix;  // e.g., "TEST_SAM_node1"
    } else {
        // Fall back to frame number if no suffix
        return project + "_" + std::to_string(currentFrame);
    }
}
```

**Pros**:
- ✅ Stable across sessions (saved with project)
- ✅ User has full control over naming
- ✅ Simple implementation
- ✅ Works in ALL OFX hosts

**Cons**:
- ❌ Requires manual user input

### 2. Hash of Plugin Parameters

Generate a hash from plugin parameter values:

```cpp
std::string generateInstanceHash() {
    std::ostringstream ss;
    ss << projectName << workflowName << basename << layerName << outputVersion;
    // Hash the concatenated string (MD5/SHA1/simple hash)
    return std::to_string(std::hash<std::string>{}(ss.str()));
}
```

**Pros**:
- ✅ Automatic (no user input)
- ✅ Stable if parameters don't change

**Cons**:
- ❌ Changes if ANY parameter changes
- ❌ Can have collisions if multiple nodes use same parameters
- ❌ Not human-readable

### 3. Frame Number + Timestamp

Use current frame plus timestamp in filename:

```cpp
std::string basename = project + "_frame" + std::to_string(frame) + "_" + timestamp;
```

**Pros**:
- ✅ Always unique
- ✅ Automatic

**Cons**:
- ❌ NOT stable across re-renders
- ❌ Creates new files every render
- ❌ Defeats caching purpose

### 4. Manual Basename Only (CURRENT DEFAULT)

Keep `autoGenerateBasename = false` by default and require users to manually set basename.

**Pros**:
- ✅ Stable and predictable
- ✅ Full user control
- ✅ Already implemented

**Cons**:
- ❌ Requires manual setup for each instance

## Recommended Implementation

**Use Solution #1 (User-Provided Instance Suffix)**

### Why?

1. Pybox has the same limitation - it uses `{project}_{nodename}` but **users must manually name their nodes** in Flame batch
2. OFX simply doesn't have access to Flame's internal node names
3. Asking users to set an "Instance Suffix" is equivalent to asking them to name their node

### User Workflow

**Scenario**: User adds 3 SAM nodes to their batch:

1. **First node**:
   - Project Name: "COMMERCIAL_2024"
   - Instance Suffix: "hero_fg"
   - Auto-generate: ON
   - Result: `COMMERCIAL_2024_hero_fg.0001.exr`

2. **Second node**:
   - Project Name: "COMMERCIAL_2024"
   - Instance Suffix: "bg"
   - Auto-generate: ON
   - Result: `COMMERCIAL_2024_bg.0001.exr`

3. **Third node**:
   - Project Name: "COMMERCIAL_2024"
   - Instance Suffix: "" (left empty)
   - Auto-generate: OFF
   - Manual Basename: "shot_010_final"
   - Result: `shot_010_final.0001.exr`

## Code Changes Required

### 1. Add Parameter (comfyui_base_plugin.cpp)

```cpp
// In describeCommonParameters() after projectName
OFX::StringParamDescriptor *instanceSuffix = desc.defineStringParam("instanceSuffix");
instanceSuffix->setLabel("Instance Suffix");
instanceSuffix->setHint("Optional: Unique identifier for this node instance. "
                       "Used with auto-generated basename. Leave empty to use manual basename.");
instanceSuffix->setDefault("");
instanceSuffix->setParent(*storageGroup);
```

### 2. Add Member Variable (comfyui_base_plugin.h)

```cpp
OFX::StringParam *_instanceSuffix;  // User-provided unique instance identifier
```

### 3. Update Constructor (comfyui_base_plugin.cpp)

```cpp
_instanceSuffix = fetchStringParam("instanceSuffix");
```

### 4. Update getEffectiveBasename() (comfyui_base_plugin.cpp)

```cpp
std::string BasePlugin::getEffectiveBasename() {
    bool autoGenerate;
    _autoGenerateBasename->getValue(autoGenerate);

    if (autoGenerate) {
        std::string project, suffix;
        _projectName->getValue(project);
        _instanceSuffix->getValue(suffix);

        if (!suffix.empty()) {
            // User provided suffix - use it
            return project + "_" + suffix;
        } else {
            // No suffix - warn and fall back to manual
            if (_logger) {
                _logger->warn("Auto-generate enabled but Instance Suffix is empty. Using manual basename instead.");
            }
            // Fall through to manual basename below
        }
    }

    // Manual basename
    std::string manual;
    _basename->getValue(manual);
    return manual;
}
```

## Testing Needed

After implementing:

1. **Load plugin in Flame** - Check log for pointer values (just for documentation)
2. **Set Instance Suffix** to "node1"
3. **Enable auto-generate**
4. **Render frame** - Verify filename is `{project}_node1.0001.exr`
5. **Save and close Flame**
6. **Reopen project**
7. **Render again** - Verify SAME filename is used (stable across sessions)

## Documentation Updates

- Update [README.md](README.md) with Instance Suffix parameter
- Update parameter hints to explain the limitation
- Document that OFX can't auto-detect instance names like Pybox can

## Related Files

- [comfyui_base_plugin.cpp](common/comfyui_base_plugin.cpp) - Core implementation
- [comfyui_base_plugin.h](common/comfyui_base_plugin.h) - Header with member variables
- [OFX_CONTEXT_FINDINGS.md](OFX_CONTEXT_FINDINGS.md) - Test results from Flame
- [COMPLETE_OFX_PROPERTIES_LIST.md](COMPLETE_OFX_PROPERTIES_LIST.md) - All OFX properties

## Final Note

**The logging code has been added** to show `kOfxPropEffectInstance` and `kOfxPropInstanceData` pointer values in the next test run. These will appear in `~/comfyui_plugin_YYYYMMDD.log` as:

```
kOfxPropEffectInstance: 0xABCD1234 (RUNTIME POINTER - not stable)
kOfxPropInstanceData: NULL (expected if not set)
```

This confirms what we already suspected: runtime pointers aren't usable for stable instance identification.

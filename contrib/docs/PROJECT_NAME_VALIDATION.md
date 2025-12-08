# Project Name Validation - Implementation

## Changes Made

### 1. Removed Default Value

**File**: `comfyui_base_plugin.cpp` (line 1091)

**Before**:
```cpp
project->setDefault("default_project");
```

**After**:
```cpp
project->setDefault("");  // No default - user must set this
```

**Rationale**:
- Prevents accidental use of generic "default_project" name
- Forces user to consciously choose a project name
- Avoids file organization confusion with default names

### 2. Updated Parameter Hint

**Before**:
```cpp
project->setHint("Project name for organizing files (in/<PROJECT>/<WORKFLOW>/)");
```

**After**:
```cpp
project->setHint("REQUIRED: Project name for organizing files (in/<PROJECT>/<WORKFLOW>/). Plugin will not process if empty.");
```

**Impact**: User is clearly informed that this parameter is mandatory.

### 3. Added Runtime Validation

**File**: `comfyui_base_plugin.cpp` (lines 291-302)

**Implementation**:
```cpp
// Validate required parameters
std::string projectName;
_projectName->getValue(projectName);

if (projectName.empty()) {
    std::string errorMsg = "Project Name is required but not set. Please set the Project Name parameter in the plugin settings.";
    if (_logger) _logger->error(errorMsg);
    setPersistentMessage(OFX::Message::eMessageError, "", errorMsg.c_str());
    throw std::runtime_error(errorMsg);
}

if (_logger) _logger->info("Project Name validation passed: '{}'", projectName);
```

**Validation Flow**:
1. Check if processing is enabled (if not, passthrough - no validation needed)
2. Get project name value
3. If empty → Show error message to user + log error + throw exception
4. If set → Log validation success and continue

### 4. User Notification Mechanism

Uses OFX's `setPersistentMessage()` to display error in the host UI:

```cpp
setPersistentMessage(OFX::Message::eMessageError, "", errorMsg.c_str());
```

**Effect**:
- **Flame/Flare**: Error appears in the UI (exact location host-dependent)
- **Nuke**: Error typically appears in the node error panel
- **Resolve**: Error appears in the effects panel

**Message Content**:
```
Project Name is required but not set.
Please set the Project Name parameter in the plugin settings.
```

---

## Behavior

### When Project Name is Empty

1. **Plugin loads**: Normal (no error at construction)
2. **User enables processing**: No immediate error
3. **User attempts to render**:
   - ❌ **Render fails immediately**
   - 🔴 **Error message displayed** in host UI
   - 📝 **Error logged** to `~/comfyui_plugin_*.log`
   - 🚫 **No workflow submission** to ComfyUI server

### When Project Name is Set

1. **Validation passes**: Logged as `Project Name validation passed: '<name>'`
2. **Workflow continues**: Normal processing

---

## File Organization Impact

### Before (with default)
```
/Volumes/silo2/002_COMFYUI/
├── in/
│   └── default_project/           ❌ Generic, non-descriptive
│       └── segmentation/
│           └── shot01.0001.exr
└── out/
    └── default_project/           ❌ All projects mixed together
        └── segmentation/
            └── v001/
```

### After (user must set)
```
/Volumes/silo2/002_COMFYUI/
├── in/
│   ├── COMMERCIAL_2024/          ✅ Descriptive project names
│   │   └── segmentation/
│   ├── MUSIC_VIDEO_XYZ/
│   │   └── segmentation/
│   └── FEATURE_FILM_ABC/
│       └── segmentation/
└── out/
    ├── COMMERCIAL_2024/          ✅ Clear organization
    ├── MUSIC_VIDEO_XYZ/
    └── FEATURE_FILM_ABC/
```

---

## User Workflow

### Initial Setup (First Time)

1. **Add plugin to clip** in Flame/timeline
2. **See parameters** - Project Name is empty
3. **Try to render** → ❌ Error: "Project Name is required but not set"
4. **Open plugin settings**
5. **Set Project Name** (e.g., "COMMERCIAL_2024")
6. **Render again** → ✅ Success

### Subsequent Use

Once set, the project name persists with the plugin instance. User only needs to set it once per node.

---

## Error Message Locations by Host

### Flame/Flare
- Console output: "Plugin rendering failed"
- Log file: Full error message with stack trace
- UI: Persistent message (location TBD - needs testing)

### Nuke (Expected)
- Node error indicator (red node)
- Error panel below node properties
- Message log window

### Resolve (Expected)
- Effects panel warning icon
- Inspector error message
- Console log

---

## Testing Checklist

- [x] Project Name empty → Render fails with clear error
- [ ] Project Name set → Render succeeds
- [ ] Error message appears in Flame UI
- [ ] Error logged to file correctly
- [ ] No workflow submitted when validation fails
- [ ] Project Name persists across sessions
- [ ] Multiple instances can have different project names

---

## Migration Notes

### For Existing Users

**If upgrading from previous version**:
- Old instances may have "default_project" stored
- These will continue to work (not empty)
- Recommendation: Update to meaningful project names

**If creating new instances**:
- Must set Project Name before first render
- No default value provided
- Clear error if forgotten

---

## Code References

- Parameter definition: [comfyui_base_plugin.cpp:1088-1092](../common/comfyui_base_plugin.cpp#L1088-L1092)
- Validation logic: [comfyui_base_plugin.cpp:291-302](../common/comfyui_base_plugin.cpp#L291-L302)
- Error handling: [comfyui_base_plugin.cpp:313-320](../common/comfyui_base_plugin.cpp#L313-L320)

---

## Related Documentation

- [OFX_CONTEXT_FINDINGS.md](OFX_CONTEXT_FINDINGS.md) - Why we can't auto-detect project names
- [README.md](README.md) - General plugin usage
- [DEBUGGING_GUIDE.md](DEBUGGING_GUIDE.md) - Troubleshooting errors

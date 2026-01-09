# Project Name Issue - Wrong Parameter Value

## Observation

The AnyComfy plugin created a directory with the wrong project name:

**Actual Path:**
```
Z:\out\ANYCOMFY,_TEST\any_segmentation\v001
```

**Expected Path:**
```
Z:\out\SAM_TEST\any_segmentation\v001
```

## Analysis

### What This Tells Us

1. **Directory Creation IS Working ✓**
   - The full directory path was successfully created
   - The base plugin's `createDirectoryRecursive()` function worked correctly
   - No issues with the directory creation mechanism itself

2. **Parameter Configuration Issue ❌**
   - The Project Name parameter is set to `"ANYCOMFY,_TEST"`
   - It should be set to `"SAM_TEST"`
   - This is a **user configuration error**, not a code bug

3. **Naming Convention Violation ⚠️**
   - The project name contains a **comma**: `ANYCOMFY,_TEST`
   - This violates best practices (see [DIRECTORY_SETUP_GUIDE.md](DIRECTORY_SETUP_GUIDE.md#naming-conventions))
   - Should use underscores only: `ANYCOMFY_TEST`

## Root Cause

### Where Project Name Comes From

The project name is taken from the **Project Name** parameter in the plugin UI:

**File**: [anycomfy_plugin.cpp](anycomfy_plugin.cpp)

**Parameter Definition (lines 91-97):**
```cpp
StringParamDescriptor* projectName = desc.defineStringParam("projectName");
projectName->setLabel("Project Name");
projectName->setHint("Name of the project (used for organizing outputs)");
projectName->setStringType(eStringTypeSingleLine);
projectName->setDefault("TEST");
projectName->setParent(*serverGroup);
```

**Used in render() via base class:**

The `BasePlugin::render()` method reads this parameter:
```cpp
std::string projectName;
_projectName->getValue(projectName);  // Gets "ANYCOMFY,_TEST" from UI
```

**Used to construct output path:**
```cpp
std::string outputDir = mountPath + "/out/" + projectName + "/" + workflowName + "/" + version;
// Results in: /mnt/share/out/ANYCOMFY,_TEST/any_segmentation/v001
```

## Why This Happened

### Scenario 1: User Manually Entered Wrong Value

The user typed `"ANYCOMFY,_TEST"` into the Project Name field in the host application (Flame/Nuke), either:
- Intentionally (testing with comma)
- By mistake (typo or autocomplete)
- Copied from somewhere with a comma

### Scenario 2: Default Value Issue

The plugin's default is `"TEST"`, so this isn't the default. The value `"ANYCOMFY,_TEST"` was explicitly set by the user.

### Scenario 3: Parameter Preset/Template

The host application might have loaded a preset or template with this value.

## Implications

### 1. Comma in Project Name

**Problem:**
```
ANYCOMFY,_TEST  ❌ (contains comma)
```

**Best Practice:**
```
ANYCOMFY_TEST   ✓ (underscores only)
SAM_TEST        ✓ (underscores only)
```

**Why Commas Are Bad:**
- Can cause parsing issues in some systems
- May break CSV exports or scripts
- Violates documented naming conventions
- Could cause issues with path handling on different platforms

**Documentation Reference:**

From [DIRECTORY_SETUP_GUIDE.md](DIRECTORY_SETUP_GUIDE.md):

> **Naming Conventions**
> - **Project names**: Use uppercase with underscores (e.g., `MY_PROJECT`, `TEST_SAM`)
> - **Avoid**:
>   - Spaces in names (use underscores instead)
>   - Special characters (except underscores and hyphens)
>   - **Commas (can cause parsing issues)**

### 2. Wrong Project Name

The project name should match the intended workflow:
- If testing SAM integration → use `"SAM_TEST"`
- If testing AnyComfy generic → use `"ANYCOMFY_TEST"` (no comma!)
- If production → use actual project name

## Solution

### Immediate Fix (User Action)

**In the host application (Flame/Nuke):**

1. **Open the AnyComfy plugin settings**
2. **Change Project Name parameter:**
   ```
   From: ANYCOMFY,_TEST
   To:   SAM_TEST         (if testing SAM integration)
   Or:   ANYCOMFY_TEST    (if testing AnyComfy)
   ```
3. **Re-render the frame**

**Expected Output:**
```
Z:\out\SAM_TEST\any_segmentation\v001\anycomfy_effect1_0001.exr
```

### Long-Term Prevention

#### Option 1: Add Parameter Validation (Recommended)

Add validation to reject invalid project names:

**File**: `anycomfy_plugin.cpp` (in `describeInContext()`)

```cpp
StringParamDescriptor* projectName = desc.defineStringParam("projectName");
projectName->setLabel("Project Name");
projectName->setHint("Name of the project (alphanumeric, underscores, and hyphens only)");
projectName->setStringType(eStringTypeSingleLine);
projectName->setDefault("TEST");
projectName->setParent(*serverGroup);

// Add validation (if OFX supports it)
// Note: Not all hosts support this, so runtime validation may be needed
```

**Runtime validation in `render()`:**

```cpp
std::string projectName;
_projectName->getValue(projectName);

// Validate project name
if (projectName.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_-")
    != std::string::npos) {
    throw std::runtime_error(
        "Invalid project name: '" + projectName + "'\n"
        "Use only letters, numbers, underscores, and hyphens.\n"
        "Example: MY_PROJECT or PROJECT_001"
    );
}
```

#### Option 2: Sanitize Input Automatically

Automatically remove/replace invalid characters:

```cpp
std::string projectName;
_projectName->getValue(projectName);

// Sanitize: replace commas and spaces with underscores
std::replace(projectName.begin(), projectName.end(), ',', '_');
std::replace(projectName.begin(), projectName.end(), ' ', '_');

// Log warning if sanitized
std::string originalName = projectName;
if (originalName != projectName) {
    std::cerr << "WARNING: Project name sanitized from '"
              << originalName << "' to '" << projectName << "'" << std::endl;
}
```

#### Option 3: Enhanced Documentation

Already done! The documentation clearly states:

- [DIRECTORY_SETUP_GUIDE.md](DIRECTORY_SETUP_GUIDE.md) - Naming conventions
- [FIXES_SUMMARY.md](FIXES_SUMMARY.md) - Common errors table
- [README.md](README.md) - Best practices

## Verification

After fixing the project name, verify the correct path is created:

### 1. Check Plugin Parameters

```
Project Name:    SAM_TEST  (or ANYCOMFY_TEST)
Workflow Name:   any_segmentation
Output Version:  v001
```

### 2. Expected Output Path

**On Windows Server:**
```
Z:\out\SAM_TEST\any_segmentation\v001\anycomfy_effect1_0001.exr
```

**On Unix Client:**
```
/mnt/share/out/SAM_TEST/any_segmentation/v001/anycomfy_effect1_0001.exr
```

### 3. Verify Directory Creation

```bash
# On ComfyUI server (Windows)
dir Z:\out\SAM_TEST\any_segmentation\v001

# On Unix client
ls -la /mnt/share/out/SAM_TEST/any_segmentation/v001/
```

## Related Issues

### Not a Code Bug

This is **NOT** a bug in the AnyComfy plugin code. The plugin correctly:
- ✓ Reads the Project Name parameter
- ✓ Uses it to construct the output path
- ✓ Creates the directory structure
- ✓ Writes files to the correct location

The issue is **user-supplied parameter value**.

### Directory Creation Working

From [DIRECTORY_ARCHITECTURE.md](DIRECTORY_ARCHITECTURE.md):

> **INPUT directories**: ✅ Automatically created by existing code in `comfyui_image_io.cpp`
> **OUTPUT directories**: ✅ Automatically created by existing code in `comfyui_base_plugin.cpp:1689-1765`
>
> **No code duplication** - All directory creation uses the same `createDirectoryRecursive()` function.

The fact that `Z:\out\ANYCOMFY,_TEST\any_segmentation\v001` was created proves directory creation is working.

### Path Escaping Fixed

From [PATH_ESCAPING_FIX.md](PATH_ESCAPING_FIX.md):

The path doesn't show double backslashes (`\\\\`), which confirms the path escaping fix (v1.0.2) is working correctly.

## Summary

| Issue | Status | Action |
|-------|--------|--------|
| Directory creation | ✓ Working | None needed |
| Path escaping | ✓ Fixed (v1.0.2) | None needed |
| Project name value | ❌ Wrong | User must change parameter |
| Comma in name | ⚠️ Violates conventions | User should use underscores |
| Code bug | ✗ None | None needed |

## Recommendations

### For Users

1. **Always use valid project names:**
   - ✓ `SAM_TEST`
   - ✓ `ANYCOMFY_TEST`
   - ✓ `MY_PROJECT`
   - ✗ `ANYCOMFY,_TEST`
   - ✗ `MY,PROJECT`
   - ✗ `MY PROJECT`

2. **Check parameter values before rendering:**
   - Review all parameters in plugin UI
   - Verify paths match your directory structure
   - Test with single frame first

3. **Follow naming conventions:**
   - See [DIRECTORY_SETUP_GUIDE.md](DIRECTORY_SETUP_GUIDE.md#naming-conventions)

### For Developers

Consider adding parameter validation in a future version to prevent invalid names:

```cpp
// Validate and sanitize project name
std::string sanitizeProjectName(const std::string& name) {
    std::string sanitized = name;
    std::replace(sanitized.begin(), sanitized.end(), ',', '_');
    std::replace(sanitized.begin(), sanitized.end(), ' ', '_');
    return sanitized;
}
```

---

**Issue Type**: User Configuration Error
**Severity**: Low (user can fix by changing parameter)
**Code Bug**: No
**Documentation**: Complete
**Date**: December 30, 2025
**Version**: 1.0.2

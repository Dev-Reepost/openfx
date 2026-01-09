# Conan 2.x Option Scoping Fix

## Issue

Build scripts were showing this warning:

```
WARN: legacy: Unscoped option definition is ambiguous.
Use '&:build_comfyui_plugins=True' to refer to the current package.
Use '*:build_comfyui_plugins=True' or other pattern if the intent was to apply to dependencies
```

## Root Cause

In **Conan 2.x**, options must be explicitly scoped to avoid ambiguity. The old Conan 1.x syntax of `-o option_name=value` is deprecated.

### The Problem

When using unscoped syntax like `-o build_comfyui_plugins=True`, Conan doesn't know if you mean:
- Apply to **current package only** (openfx)
- Apply to **all packages** in the dependency tree

This creates ambiguity that could lead to unexpected behavior.

## Conan 2.x Scoping Syntax

| Syntax | Meaning | Use Case |
|--------|---------|----------|
| `&:option=value` | Current package only | Most common - applies to the package being built |
| `*:option=value` | All packages | Rarely needed - applies to all dependencies |
| `package:option=value` | Specific package | When you need to set option for a specific dependency |

## Solution

Since `build_comfyui_plugins` is defined in the **openfx** package (not in dependencies), we use the `&:` scope to explicitly indicate it applies to the current package only.

### Changes Made

Updated all build scripts to use scoped syntax:

**Before:**
```bash
-o build_comfyui_plugins=True
```

**After:**
```bash
-o "&:build_comfyui_plugins=True"
```

### Files Modified

1. **[contrib/dev-tools/build-macos-universal-plugin.sh](../dev-tools/build-macos-universal-plugin.sh)**
   - Line 167: Changed to `-o "&:build_comfyui_plugins=True"`
   - Line 179: Changed to `-o "&:build_comfyui_plugins=True"`

2. **[contrib/dev-tools/build-plugin.sh](../dev-tools/build-plugin.sh)**
   - Line 255: Changed to `conan_options="-o \"&:build_comfyui_plugins=True\""`

3. **[contrib/dev-tools/build-linux-universal-plugin.sh](../dev-tools/build-linux-universal-plugin.sh)**
   - Line 179: Changed to `-o "&:build_comfyui_plugins=True"`

4. **[contrib/dev-tools/build-linux-plugin.sh](../dev-tools/build-linux-plugin.sh)**
   - Line 123: Changed to `-o "&:build_comfyui_plugins=True"`

## Verification

### Before Fix
```bash
$ ./contrib/dev-tools/build-macos-universal-plugin.sh -p AnyComfy -t AnyComfy
WARN: legacy: Unscoped option definition is ambiguous.
Use '&:build_comfyui_plugins=True' to refer to the current package.
...
```

### After Fix
```bash
$ ./contrib/dev-tools/build-macos-universal-plugin.sh -p AnyComfy -t AnyComfy
# No warnings about ambiguous options
[SUCCESS] arm64 build completed
[SUCCESS] x86_64 build completed
[SUCCESS] Build complete!
```

### Testing
```bash
# Test for warning presence
$ ./contrib/dev-tools/build-macos-universal-plugin.sh -p AnyComfy -t AnyComfy 2>&1 | grep -i "ambiguous" | wc -l
0  # ✅ No warnings found
```

## Why This Matters

### 1. **Future-Proofing**
Conan 2.x is deprecating unscoped syntax. Using scoped syntax ensures compatibility with future Conan versions.

### 2. **Clarity**
Explicitly scoping options makes build scripts more readable and intentions clearer.

### 3. **Correctness**
Scoped syntax prevents accidental option propagation to dependencies, which could cause unexpected build behavior.

### 4. **Clean Builds**
Eliminates warning noise in build logs, making real issues easier to spot.

## Best Practices

### When Adding New Conan Options

1. **Always use scoped syntax** in Conan 2.x:
   ```bash
   # ✅ Correct - scoped to current package
   conan install -o "&:my_option=True" .

   # ❌ Deprecated - unscoped (generates warning)
   conan install -o my_option=True .
   ```

2. **Choose the right scope:**
   - `&:` for package-specific options (99% of cases)
   - `*:` only when you need to apply to all packages
   - `package_name:` for targeting specific dependencies

3. **Quote options in shell scripts:**
   ```bash
   # ✅ Proper quoting
   conan_options="-o \"&:build_comfyui_plugins=True\""

   # In direct commands
   conan install -o "&:build_comfyui_plugins=True" .
   ```

## Related Documentation

- **Conan 2.x Migration Guide**: https://docs.conan.io/2/migrating_to_2.0.html
- **Conan Options Reference**: https://docs.conan.io/2/reference/conanfile/methods/options.html
- **OpenFX Conanfile**: [conanfile.py](../../conanfile.py) (defines `build_comfyui_plugins` option)

## Impact

- ✅ **No breaking changes** - Functionality remains identical
- ✅ **No build system changes** - Same build commands work
- ✅ **Cleaner logs** - Warning eliminated from all builds
- ✅ **Better compatibility** - Ready for future Conan versions

## Date

**Fixed**: December 26, 2025
**Conan Version**: 2.1.0+
**Affected Platforms**: macOS, Linux, Windows (all build scripts)

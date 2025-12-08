# CMakeLists.txt Fix - Non-Interfering Build Scripts

## Problem

The [CMakeLists.txt](../../CMakeLists.txt) was defaulting to universal binary (`CMAKE_OSX_ARCHITECTURES "x86_64;arm64"`), which caused:

1. **build-plugin.sh** to fail because x86_64 Conan dependencies weren't available
2. Simple arm64-only development builds to become slow and complex
3. Build scripts to interfere with each other

## Solution

Changed [CMakeLists.txt](../../CMakeLists.txt#L19-L24) to **default to native architecture**:

### Before (Problematic)

```cmake
if(APPLE)
  # Universal binary support - requires Conan dependencies for both architectures
  # Both arm64 and x86_64 Conan dependencies are now available
  # Can be overridden by setting CMAKE_OSX_ARCHITECTURES on command line
  if(NOT DEFINED CMAKE_OSX_ARCHITECTURES)
    set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64")  # ← BREAKS SIMPLE BUILDS
  endif()
endif()
```

### After (Fixed)

```cmake
if(APPLE)
  # Default to native architecture for simple builds
  # For universal binary (x86_64 + arm64), use build-macos-universal-plugin.sh
  # which builds each architecture separately and combines with lipo
  # CMAKE_OSX_ARCHITECTURES can be overridden on command line if needed
endif()
```

## Impact

### build-plugin.sh (Simple arm64-only builds)

- ✅ **Now works** - defaults to native architecture (arm64 on M1/M2/M3/M4)
- ✅ **Fast** - only needs arm64 Conan dependencies
- ✅ **No interference** from universal binary settings

### build-macos-universal-plugin.sh (Universal binary builds)

- ✅ **Still works** - explicitly sets `CMAKE_OSX_ARCHITECTURES` for each build
- ✅ **Independent** - not affected by CMakeLists.txt default
- ✅ **No changes needed** to the script

## How It Works

### Native Architecture Default

When `CMAKE_OSX_ARCHITECTURES` is not set, CMake defaults to the host's native architecture:

- **M1/M2/M3/M4 Mac**: Builds arm64
- **Intel Mac**: Builds x86_64

### Explicit Architecture Override

Both build scripts can override if needed:

**build-plugin.sh**: Doesn't set, uses native

```bash
cmake -S . -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_COMFYUI_PLUGINS=ON
    # No CMAKE_OSX_ARCHITECTURES → native
```

**build-macos-universal-plugin.sh**: Sets explicitly for each build

```bash
# arm64 build
cmake -S . -B "$ARM64_DIR" \
    -DCMAKE_OSX_ARCHITECTURES=arm64

# x86_64 build
cmake -S . -B "$X86_64_DIR" \
    -DCMAKE_OSX_ARCHITECTURES=x86_64
```

## Testing

### Verify build-plugin.sh Works (arm64-only)

```bash
# Clean build
rm -rf build/

# Build arm64-only
./contrib/dev-tools/build-plugin.sh \
    contrib/plugins/ComfyUI/segmentation \
    SAMSegmentation \
    --install-dir ~/Library/OFX/Plugins

# Verify architecture
lipo -info ~/Library/OFX/Plugins/SAMSegmentation.ofx.bundle/Contents/MacOS/SAMSegmentation.ofx
# Should show: "Non-fat file: ... is architecture: arm64"
```

### Verify build-macos-universal-plugin.sh Works (universal)

```bash
# Clean build
rm -rf build/

# Build universal
./contrib/dev-tools/build-macos-universal-plugin.sh \
    -p SAMSegmentation \
    -t SAMSegmentation \
    --install

# Verify architecture
lipo -info ~/Library/OFX/Plugins/SAMSegmentation.ofx.bundle/Contents/MacOS/SAMSegmentation.ofx
# Should show: "Architectures in the fat file: ... are: x86_64 arm64"
```

## Files Changed

1. **[CMakeLists.txt](../../CMakeLists.txt#L19-L24)** - Removed universal binary default
2. **[BUILD_WORKFLOWS.md](BUILD_WORKFLOWS.md)** - New comprehensive documentation
3. **[CMAKELIST_FIX.md](CMAKELIST_FIX.md)** - This document

No changes needed to:

- [contrib/dev-tools/build-plugin.sh](../dev-tools/build-plugin.sh) - Works as-is
- [contrib/dev-tools/build-macos-universal-plugin.sh](../dev-tools/build-macos-universal-plugin.sh) - Works as-is
- [contrib/plugins/ComfyUI/segmentation/CMakeLists.txt](../../contrib/plugins/ComfyUI/segmentation/CMakeLists.txt) - Already correct

## Summary

The fix ensures:

- ✅ Simple, fast arm64-only builds for development (build-plugin.sh)
- ✅ Universal binary support when needed (build-macos-universal-plugin.sh)
- ✅ No interference between the two workflows
- ✅ Clear, understandable build system behavior
- ✅ Consistent with user's requirement: "simple, robust, consistent, flexible"

Both scripts now work independently without interfering with each other.

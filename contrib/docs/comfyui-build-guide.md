// Copyright OpenFX and contributors to the OpenFX project.
// SPDX-License-Identifier: BSD-3-Clause

# ComfyUI OFX Plugins - Build Guide

## Quick Start

### For First-Time Build

```bash
# 1. Navigate to OpenFX root
cd /Users/julien/src/openfx

# 2. Install dependencies and build
conan install -s build_type=Release \
              -o build_comfyui_plugins=True \
              -pr:b=default \
              --build=missing .

# 3. Configure CMake
cmake --preset conan-release \
      -DBUILD_COMFYUI_PLUGINS=ON

# 4. Build
cmake --build build/Release --config Release --parallel

# 5. Install plugins
cmake --build build/Release --target install --config Release
```

### For Subsequent Builds

```bash
# Just rebuild (dependencies already cached)
cmake --build build/Release --config Release --parallel
cmake --build build/Release --target install --config Release
```

## Environment Variables

**No environment variables are required!** Everything is managed by Conan and CMake.

### Optional: Custom Plugin Install Directory

If you want plugins installed to a custom location:

```bash
cmake --preset conan-release \
      -DBUILD_COMFYUI_PLUGINS=ON \
      -DPLUGIN_INSTALLDIR=/path/to/custom/plugins
```

## Where Dependencies Are Stored

Conan manages all C++ libraries automatically:

```
~/.conan2/p/                         # Package cache (shared across projects)
  ├── nlohm*/                        # nlohmann/json
  ├── httpli*/                       # cpp-httplib
  ├── webso*/                        # websocketpp
  ├── openi*/                        # OpenImageIO
  └── opens*/                        # OpenSSL

/Users/julien/src/openfx/build/Release/generators/
  ├── *.cmake                        # CMake find_package configs (auto-generated)
  └── conan_toolchain.cmake          # Conan CMake integration
```

**You don't need to:**
- Download libraries manually
- Set include paths
- Set library paths
- Configure environment variables

**Conan does it all automatically!**

## Build Configurations

### Debug Build

```bash
conan install -s build_type=Debug \
              -o build_comfyui_plugins=True \
              -pr:b=default \
              --build=missing .

cmake --preset conan-debug \
      -DBUILD_COMFYUI_PLUGINS=ON

cmake --build build/Debug --config Debug --parallel
```

### Release Build (Optimized)

```bash
conan install -s build_type=Release \
              -o build_comfyui_plugins=True \
              -pr:b=default \
              --build=missing .

cmake --preset conan-release \
      -DBUILD_COMFYUI_PLUGINS=ON

cmake --build build/Release --config Release --parallel
```

### Universal Binary (macOS - both Intel + Apple Silicon)

This is already configured by default in CMakeLists.txt:

```cmake
if(APPLE)
  set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64")
endif()
```

Just build normally - you'll get a universal binary automatically!

## Troubleshooting

### Issue: "Package not found"

**Problem:** Conan can't find a package in its repositories

**Solution:**
```bash
# Update Conan remotes
conan remote list
# Should show: conancenter: https://center.conan.io

# If missing, add it:
conan remote add conancenter https://center.conan.io
```

### Issue: "find_package(httplib) not found"

**Problem:** CMake can't find Conan-installed packages

**Solution:**
```bash
# Make sure you ran conan install BEFORE cmake configure
conan install -s build_type=Release \
              -o build_comfyui_plugins=True \
              -pr:b=default \
              --build=missing .

# THEN configure CMake
cmake --preset conan-release -DBUILD_COMFYUI_PLUGINS=ON
```

### Issue: "ComfyUI plugins disabled"

**Problem:** You see "ComfyUI plugins disabled. Use -DBUILD_COMFYUI_PLUGINS=ON"

**Solution:** Two things needed:

1. Conan option: `-o build_comfyui_plugins=True`
2. CMake option: `-DBUILD_COMFYUI_PLUGINS=ON`

Both must be enabled!

### Issue: Build fails with "missing header"

**Problem:** C++ compiler can't find library headers

**Solution:**
```bash
# Clean build and reinstall dependencies
rm -rf build/
conan install -s build_type=Release \
              -o build_comfyui_plugins=True \
              -pr:b=default \
              --build=missing .

cmake --preset conan-release -DBUILD_COMFYUI_PLUGINS=ON
cmake --build build/Release --config Release
```

### Issue: Plugins not found by OFX host

**Problem:** Built plugins don't show up in Flame/Nuke/etc.

**Solution:**

1. Check installation location:
```bash
# macOS
ls ~/Library/OFX/Plugins/

# Should see: ComfyUISegmentation.ofx.bundle (when implemented)
```

2. Verify bundle structure:
```bash
tree ~/Library/OFX/Plugins/ComfyUISegmentation.ofx.bundle/
# Should have:
# Contents/
#   Info.plist
#   MacOS/
#     ComfyUISegmentation (binary)
```

3. Check host OFX paths:
   - Some hosts use custom plugin directories
   - Check host preferences/settings

## Clean Build

If things get messy, start fresh:

```bash
# Remove all build artifacts
rm -rf build/
rm -rf CMakeCache.txt
rm -rf cmake-build-*/

# Remove Conan cache (optional - will re-download everything)
conan remove "*" -c

# Rebuild from scratch
conan install -s build_type=Release \
              -o build_comfyui_plugins=True \
              -pr:b=default \
              --build=missing .

cmake --preset conan-release -DBUILD_COMFYUI_PLUGINS=ON
cmake --build build/Release --config Release --parallel
```

## Verifying Dependencies

Check which dependencies are installed:

```bash
conan list "*:*"
```

Should show:
- nlohmann_json/3.11.3
- cpp-httplib/0.15.3
- websocketpp/0.8.2
- openimageio/2.5.10.1
- openssl/3.2.1
- (and their transitive dependencies)

## Advanced: Custom Dependency Versions

If you need different versions:

```python
# Edit conanfile.py
def requirements(self):
    if self.options.build_comfyui_plugins:
        self.requires("nlohmann_json/3.11.3")  # Change version here
        self.requires("cpp-httplib/0.15.3")
        # etc.
```

Then rebuild:
```bash
conan install ... --build=missing .
cmake --preset conan-release ...
```

## Platform-Specific Notes

### macOS
- Universal binaries built by default
- Plugins install to `~/Library/OFX/Plugins/`
- Requires macOS 10.15+ for full C++17 support

### Linux
- Plugins install to `~/.local/share/OFX/Plugins/`
- May need to install OpenGL dev packages: `sudo apt install libgl-dev`

### Windows
- Use Visual Studio 2019+ for C++17 support
- Plugins install to `%COMMONPROGRAMFILES%\OFX\Plugins\`
- Use CMake GUI or Visual Studio integration

## Summary

**Key Points:**
1. ✅ Conan manages all C++ dependencies automatically
2. ✅ No manual library downloads or environment variables needed
3. ✅ Enable with `-o build_comfyui_plugins=True` (Conan) and `-DBUILD_COMFYUI_PLUGINS=ON` (CMake)
4. ✅ Universal binaries on macOS by default
5. ✅ Standard OFX plugin installation paths

**Two-command build:**
```bash
conan install -s build_type=Release -o build_comfyui_plugins=True -pr:b=default --build=missing .
cmake --preset conan-release -DBUILD_COMFYUI_PLUGINS=ON && cmake --build build/Release --parallel
```

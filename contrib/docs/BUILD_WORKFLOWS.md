# Build Workflows for ComfyUI Plugins

This document explains the two independent build workflows for ComfyUI plugins.

## Overview

We have two separate, non-interfering build scripts:

1. **build-plugin.sh** - Simple, fast arm64-only builds (M1/M2/M3/M4 Macs)
2. **build-macos-universal-plugin.sh** - macOS universal binary builds (arm64 + x86_64)

Both scripts work independently and don't interfere with each other.

## Workflow 1: Simple arm64-only Build (Recommended for Development)

**Use this when:**
- Developing and testing on Apple Silicon Mac
- Quick iterative development cycle
- Only need to support M1/M2/M3/M4 Macs

**Command:**
```bash
./contrib/dev-tools/build-plugin.sh \
    contrib/plugins/ComfyUI/segmentation \
    SAMSegmentation \
    --install-dir ~/Library/OFX/Plugins
```

**What it does:**
1. Installs Conan dependencies for native architecture (arm64)
2. Configures CMake with native architecture
3. Builds the plugin
4. Creates .ofx.bundle structure
5. Installs to specified directory

**Build time:** ~2-5 minutes (with cached dependencies)

**Output:** `SAMSegmentation.ofx.bundle` (arm64 only)

## Workflow 2: Universal Binary Build (For Distribution)

**Use this when:**
- Need to support both Apple Silicon and Intel Macs
- Preparing plugin for distribution
- Testing on both architectures

**Command:**
```bash
./contrib/dev-tools/build-macos-universal-plugin.sh \
    -p SAMSegmentation \
    -t SAMSegmentation \
    --install
```

**What it does:**
1. Builds arm64 version with arm64 Conan dependencies
2. Builds x86_64 version with x86_64 Conan dependencies (built from source)
3. Combines both binaries using `lipo`
4. Creates universal .ofx.bundle
5. Installs to ~/Library/OFX/Plugins

**Build time:**
- First time: ~30-40 minutes (building x86_64 dependencies from source)
- Subsequent: ~5-10 minutes (cached dependencies)

**Output:** `SAMSegmentation.ofx.bundle` (universal binary: arm64 + x86_64)

## How They Work Independently

### CMakeLists.txt Configuration

The root [CMakeLists.txt](../../CMakeLists.txt) defaults to **native architecture**:

```cmake
if(APPLE)
  # Default to native architecture for simple builds
  # For universal binary (x86_64 + arm64), use build-macos-universal-plugin.sh
  # which builds each architecture separately and combines with lipo
  # CMAKE_OSX_ARCHITECTURES can be overridden on command line if needed
endif()
```

This means:
- **build-plugin.sh**: Uses native architecture (arm64 on M1/M2/M3/M4)
- **build-macos-universal-plugin.sh**: Explicitly sets architecture for each build

### build-plugin.sh Implementation

Does NOT set `CMAKE_OSX_ARCHITECTURES`, so defaults to native:
```bash
cmake -S . -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_COMFYUI_PLUGINS=ON \
    # ... no CMAKE_OSX_ARCHITECTURES set
```

### build-macos-universal-plugin.sh Implementation

Explicitly sets architecture for each separate build:

**arm64 build:**
```bash
cmake -S . -B "$ARM64_DIR" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_OSX_ARCHITECTURES=arm64 \  # ← Explicit
    -DBUILD_COMFYUI_PLUGINS=ON
```

**x86_64 build:**
```bash
cmake -S . -B "$X86_64_DIR" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_OSX_ARCHITECTURES=x86_64 \  # ← Explicit
    -DBUILD_COMFYUI_PLUGINS=ON
```

Then combines with lipo:
```bash
lipo -create "$ARM64_BINARY" "$X86_64_BINARY" -output "$UNIVERSAL_BINARY"
```

## Dependency Management

Both workflows use **Conan** for all dependencies (no Homebrew):

### Dependencies (from [conanfile.py](../../conanfile.py))
```python
if self.options.build_comfyui_plugins:
    self.requires("nlohmann_json/3.11.3")
    self.requires("cpp-httplib/0.15.3")
    self.requires("websocketpp/0.8.2")
    self.requires("tinyexr/1.0.7")
    self.requires("miniz/3.0.2")
    self.requires("openssl/3.2.1")
    self.requires("boost/1.84.0", override=True)
```

### Why Conan Instead of Homebrew?

Homebrew provides only native architecture packages:
- arm64 packages on Apple Silicon Macs
- x86_64 packages on Intel Macs

Conan can build dependencies for any architecture from source, enabling universal binary builds on a single machine.

## Verifying Your Build

### Check Binary Architecture

**arm64-only build:**
```bash
lipo -info ~/Library/OFX/Plugins/SAMSegmentation.ofx.bundle/Contents/MacOS/SAMSegmentation.ofx
```
Should output:
```
Non-fat file: ... is architecture: arm64
```

**Universal binary:**
```bash
lipo -info ~/Library/OFX/Plugins/SAMSegmentation.ofx.bundle/Contents/MacOS/SAMSegmentation.ofx
```
Should output:
```
Architectures in the fat file: ... are: x86_64 arm64
```

### Test in Flame

1. **Copy plugin** to OFX directory (or use `--install` flag)
2. **Launch Flame** and check the plugin appears
3. **Test functionality** with your workflow

**Important:** Don't use "-universal" suffix in bundle name. OFX hosts expect standard naming:
- ✅ `SAMSegmentation.ofx.bundle` or `ComfyUISAMSegmentation.ofx.bundle`
- ❌ `SAMSegmentation-universal.ofx.bundle`

## Troubleshooting

### build-plugin.sh Issues

**Problem:** "tinyexr not found" or similar dependency errors

**Solution:** Install Conan dependencies first:
```bash
conan install . \
    -s build_type=Release \
    -pr:b=default \
    --build=missing \
    -o build_comfyui_plugins=True
```

### build-macos-universal-plugin.sh Issues

**Problem:** x86_64 build takes forever (30+ minutes)

**Cause:** Building x86_64 dependencies from source on Apple Silicon

**Solution:** This is expected for first build. Subsequent builds use cached dependencies and are much faster.

**Problem:** "Plugin not found" in Flame

**Solution:** Check bundle naming - remove any "-universal" suffix:
```bash
cd ~/Library/OFX/Plugins
mv SAMSegmentation-universal.ofx.bundle SAMSegmentation.ofx.bundle
```

### Both Scripts

**Problem:** CMake configure errors

**Solution:** Clean build directory and try again:
```bash
rm -rf build/
```

## Summary

| Aspect | build-plugin.sh | build-macos-universal-plugin.sh |
|--------|----------------|--------------------------------|
| **Use case** | Development, testing | Distribution, Intel Mac support |
| **Architectures** | arm64 (native) | arm64 + x86_64 (universal) |
| **Build time (first)** | 2-5 min | 30-40 min |
| **Build time (cached)** | 2-5 min | 5-10 min |
| **Dependencies** | Native Conan packages | Conan builds from source |
| **Complexity** | Simple, one build | Complex, two builds + lipo |
| **Interference** | None - independent scripts |

Choose the workflow that matches your needs. For daily development on Apple Silicon, use **build-plugin.sh**. For distribution to users with Intel Macs, use **build-macos-universal-plugin.sh**.

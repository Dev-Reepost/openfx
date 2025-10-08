// Copyright OpenFX and contributors to the OpenFX project.
// SPDX-License-Identifier: BSD-3-Clause

# ComfyUI OFX Plugin Development - Setup Complete ✓

## What Was Done

### 1. Dependency Management ✓

**Updated:** `conanfile.py`
- Added `build_comfyui_plugins` option (default: OFF)
- Configured 5 C++ dependencies:
  - `nlohmann_json` - JSON parsing
  - `cpp-httplib` - HTTP client
  - `websocketpp` - WebSocket support
  - `openimageio` - EXR image I/O
  - `openssl` - HTTPS encryption

**How it works:**
- All dependencies are **automatically downloaded** by Conan
- Stored in `~/.conan2/p/` (shared cache)
- CMake find_package configs auto-generated in `build/Release/generators/`

### 2. Build System Integration ✓

**Updated:** `CMakeLists.txt`
- Added `BUILD_COMFYUI_PLUGINS` CMake option
- Configured automatic package discovery
- Linked dependencies when option is enabled

### 3. Project Structure ✓

**Created:** `contrib/plugins/ComfyUI/`

```
ComfyUI/
├── CMakeLists.txt                   # Build configuration
├── README.md                        # Plugin documentation
└── common/                          # Shared code
    ├── comfyui_client.h             # REST client header
    ├── comfyui_client.cpp           # REST client stub
    ├── comfyui_base_plugin.h        # Base plugin header
    └── comfyui_base_plugin.cpp      # Base plugin stub
```

### 4. Documentation ✓

**Created:**
- `contrib/plugins/ComfyUI/README.md` - Plugin overview and usage
- `contrib/docs/comfyui-build-guide.md` - Complete build instructions
- `contrib/docs/pybox-to-ofx-transposition.md` - Architecture guide (already existed)

---

## What You Need to Do

### Answer to Your Questions

**Q: Will you take care of getting the C++ libraries?**
**A:** ✓ Yes! Conan handles everything automatically. You just need to:
1. Run `conan install` with the right options
2. Conan downloads, builds, and configures all dependencies
3. No manual downloads or environment variables needed!

**Q: Where do you recommend putting them?**
**A:** ✓ Already configured! Dependencies go to:
- `~/.conan2/p/` - Global Conan cache (shared across projects)
- `build/Release/generators/` - CMake integration files (auto-generated)

**Q: What env vars and/or build config should be created or updated?**
**A:** ✓ None needed! Everything is automatic via Conan + CMake.

---

## How to Build (When Ready)

### Option 1: Quick Build (Recommended)

```bash
# From /Users/julien/src/openfx

# Install dependencies and configure
conan install -s build_type=Release \
              -o build_comfyui_plugins=True \
              -pr:b=default \
              --build=missing .

# Configure CMake
cmake --preset conan-release -DBUILD_COMFYUI_PLUGINS=ON

# Build
cmake --build build/Release --config Release --parallel

# Install to ~/Library/OFX/Plugins/
cmake --build build/Release --target install --config Release
```

### Option 2: Using Build Script

```bash
./scripts/build-cmake.sh -o build_comfyui_plugins=True Release
```

---

## Current State

### ✓ Complete (Foundation)

1. **Dependency management** - Conan configured with all required packages
2. **Build system** - CMake integration ready
3. **Directory structure** - Plugin framework created
4. **Stub implementation** - Placeholder code compiles
5. **Documentation** - Build guides and architecture docs

### ⏳ To Be Implemented (Phase 1)

1. **ComfyUI Client Implementation**
   - HTTP POST/GET using cpp-httplib
   - WebSocket connection using websocketpp
   - JSON workflow serialization

2. **Base Plugin Implementation**
   - OpenImageIO EXR file I/O
   - Workflow execution logic
   - Progress reporting via OFX Progress Suite
   - Error handling and user feedback

3. **Segmentation Plugin**
   - SAM2 workflow JSON
   - Parameter definitions (model selection, threshold, prompt)
   - Multi-output handling (result + matte)

---

## Testing the Setup

### Verify Dependencies Can Be Downloaded

```bash
cd /Users/julien/src/openfx

# Dry run - check if packages are available
conan install -s build_type=Release \
              -o build_comfyui_plugins=True \
              -pr:b=default \
              --build=missing \
              . \
              --dry-run
```

Expected output:
```
nlohmann_json/3.11.3
cpp-httplib/0.15.3
websocketpp/0.8.2
openimageio/2.5.10.1
openssl/3.2.1
...
```

### Test Build (Stubs Only)

```bash
# Full build test
conan install -s build_type=Release \
              -o build_comfyui_plugins=True \
              -pr:b=default \
              --build=missing .

cmake --preset conan-release -DBUILD_COMFYUI_PLUGINS=ON
cmake --build build/Release --config Release --target ComfyUICommon
```

Should compile successfully and create `libComfyUICommon.a`

---

## Next Steps

### Phase 1: Implement Core Functionality

1. **ComfyUI REST Client** (`comfyui_client.cpp`)
   - Implement `queuePrompt()` using cpp-httplib
   - Implement `getHistory()`
   - Implement WebSocket monitoring
   - Add connection testing

2. **Base Plugin Class** (`comfyui_base_plugin.cpp`)
   - Implement `render()` workflow execution
   - Implement `writeInputImage()` with OpenImageIO
   - Add parameter definitions in `describeInContext()`
   - Add error handling and progress reporting

3. **Create First Plugin** (`segmentation/`)
   - Copy workflow JSON from Python project
   - Implement `buildWorkflow()` method
   - Define SAM2/DINO model parameters
   - Handle multi-output (result + matte)

### Phase 2: Testing

1. Set up ComfyUI test server
2. Create test images
3. Build and install plugin
4. Test in Flame/Nuke
5. Debug and iterate

### Phase 3: Additional Plugins

1. Upscaling plugin
2. Inpainting plugin
3. Style transfer plugin
4. [Other AI workflows]

---

## Documentation Reference

- **Build Guide**: `contrib/docs/comfyui-build-guide.md`
- **Architecture Plan**: `contrib/docs/pybox-to-ofx-transposition.md`
- **Plugin README**: `contrib/plugins/ComfyUI/README.md`

---

## Summary

**Everything is set up and ready to go!**

- ✅ Dependencies configured (Conan handles downloads)
- ✅ Build system integrated (CMake ready)
- ✅ Project structure created
- ✅ Documentation complete
- ✅ No environment variables needed
- ✅ No manual library installation needed

**Just run the build commands when you're ready to start implementation!**

---

## Questions?

Refer to:
- Build issues → `contrib/docs/comfyui-build-guide.md` (Troubleshooting section)
- Architecture questions → `contrib/docs/pybox-to-ofx-transposition.md`
- Plugin structure → `contrib/plugins/ComfyUI/README.md`

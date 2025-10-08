// Copyright OpenFX and contributors to the OpenFX project.
// SPDX-License-Identifier: BSD-3-Clause

# ComfyUI OFX Plugin Implementation - Progress Log

## Session 1: Foundation and Initial Implementation

**Date:** 2025-10-08
**Goal:** Set up infrastructure and begin core implementation

### Completed ✓

#### 1. Dependency Configuration
- ✅ Updated [conanfile.py](../../conanfile.py) with ComfyUI dependencies:
  - `nlohmann_json/3.11.3` - JSON parsing
  - `cpp-httplib/0.15.3` - HTTP client
  - `websocketpp/0.8.2` - WebSocket support
  - `openssl/3.2.1` - HTTPS encryption
  - `boost/1.84.0` - Resolved version conflict
  - `expat/2.6.2` - Updated for compatibility

#### 2. Build System Integration
- ✅ Updated [CMakeLists.txt](../../CMakeLists.txt):
  - Added `BUILD_COMFYUI_PLUGINS` option
  - Configured find_package() for dependencies
  - Integrated with existing Conan workflow

- ✅ Created [contrib/plugins/ComfyUI/CMakeLists.txt](../plugins/ComfyUI/CMakeLists.txt):
  - `ComfyUICommon` static library target
  - Proper include directories
  - Dependency linking

#### 3. ComfyUI REST Client Implementation
- ✅ Implemented [comfyui_client.h](../plugins/ComfyUI/common/comfyui_client.h):
  - PIMPL pattern for encapsulation
  - Clean public API

- ✅ Implemented [comfyui_client.cpp](../plugins/ComfyUI/common/comfyui_client.cpp):
  - **HTTP Methods**:
    - `testConnection()` - Server connectivity test
    - `queuePrompt()` - POST workflow to `/prompt`
    - `getHistory()` - GET results from `/history/{id}`
    - `interruptExecution()` - POST to `/interrupt`
  - **Server Management**:
    - Parse "hostname:port" format
    - Default port 8188
    - Timeout configuration (5-10s)
  - **Error Handling**:
    - Exception-based error reporting
    - HTTP status code validation
    - JSON parsing errors
  - **Client ID Generation**:
    - Random hex string: `ofx_client_<16-hex-digits>`

#### 4. Base Plugin Stub
- ✅ Created [comfyui_base_plugin.h](../plugins/ComfyUI/common/comfyui_base_plugin.h):
  - Inherits from `OFX::ImageEffect`
  - Abstract methods for derived classes
  - State management enum

- ✅ Created [comfyui_base_plugin.cpp](../plugins/ComfyUI/common/comfyui_base_plugin.cpp):
  - Constructor/destructor
  - Clip fetching
  - Stub methods (to be implemented)

#### 5. Project Structure
```
contrib/plugins/ComfyUI/
├── CMakeLists.txt
├── README.md
└── common/
    ├── comfyui_client.h
    ├── comfyui_client.cpp
    ├── comfyui_base_plugin.h
    └── comfyui_base_plugin.cpp
```

#### 6. Documentation
- ✅ [pybox-to-ofx-transposition.md](pybox-to-ofx-transposition.md) - Complete architecture plan
- ✅ [comfyui-build-guide.md](comfyui-build-guide.md) - Build instructions
- ✅ [contrib/plugins/ComfyUI/README.md](../plugins/ComfyUI/README.md) - Plugin overview
- ✅ [SETUP_COMPLETE.md](SETUP_COMPLETE.md) - Setup summary

### In Progress ⏳

#### Conan Dependency Installation
**Issue:** Complex dependency chain causing build time
**Status:** Installing dependencies (running in background)

**Decisions Made:**
- Removed `OpenImageIO` temporarily due to complex dependency chain (opencolorio conflicts)
- Will add simpler EXR I/O library later or implement basic file I/O first
- Focus on getting HTTP client working first

### Next Steps 📋

#### Phase 1A: Test Current Implementation
1. **Complete Conan installation**
   - Wait for current build to finish
   - Verify all packages installed successfully

2. **Test Compilation**
   ```bash
   cmake --preset conan-release -DBUILD_COMFYUI_PLUGINS=ON
   cmake --build build/Release --target ComfyUICommon
   ```

3. **Create Simple Test Program**
   - Standalone executable to test `ComfyUIClient`
   - Test connection to local ComfyUI server
   - Validate HTTP methods work correctly

#### Phase 1B: Complete Base Plugin
1. **Implement Parameter Definitions**
   - Server address/port parameters
   - Shared mount path parameter
   - Project name parameter
   - Add to `describeInContext()`

2. **Implement File I/O** (Simple approach first)
   - Option A: Use PNG/JPEG initially (simpler)
   - Option B: Add lightweight EXR library (OpenEXR direct)
   - Option C: Let ComfyUI handle format conversion

3. **Implement Workflow Execution**
   - `render()` method
   - Workflow JSON building
   - Image exchange
   - Progress reporting

#### Phase 2: First Plugin Implementation
1. **Create Segmentation Plugin**
   - New directory: `contrib/plugins/ComfyUI/segmentation/`
   - Implement derived class
   - Add SAM2 workflow JSON
   - Parameter UI (model selection, threshold, prompt)

2. **Testing**
   - Set up ComfyUI test server
   - Create test images
   - End-to-end test in Flame/Nuke

### Technical Notes 📝

#### ComfyUI REST API Format
Based on Python implementation analysis:

**Queue Prompt:**
```cpp
POST /prompt
{
  "prompt": { /* workflow JSON */ },
  "client_id": "ofx_client_abc123..."
}

Response:
{
  "prompt_id": "uuid-string"
}
```

**Get History:**
```cpp
GET /history/{prompt_id}

Response:
{
  "{prompt_id}": {
    "outputs": {
      /* node outputs */
    },
    "status": { /* execution status */ }
  }
}
```

**Interrupt:**
```cpp
POST /interrupt
{
  "client_id": "ofx_client_abc123..."
}
```

#### Dependency Notes
- **cpp-httplib**: Header-only, simple HTTP/HTTPS client
- **websocketpp**: For real-time status monitoring (not yet implemented)
- **nlohmann/json**: Modern C++ JSON library
- **OpenSSL**: Required for HTTPS support in httplib

#### Design Decisions Log

**2025-10-08: OpenImageIO Removal**
- **Problem**: OpenImageIO has complex dependency chain (50+ packages)
  - Depends on opencolorio → minizip-ng config conflicts
  - Very long build time
- **Decision**: Remove for now, add later or use simpler alternative
- **Impact**: Need to implement EXR I/O separately
- **Options**:
  1. Use OpenEXR library directly (simpler, just EXR)
  2. Start with PNG/JPEG for testing
  3. Let ComfyUI handle all format conversion

**2025-10-08: PIMPL Pattern**
- Used in `ComfyUIClient` to hide httplib implementation details
- Keeps header clean
- Allows changing HTTP library without ABI breaks

**2025-10-08: Exception-Based Error Handling**
- ComfyUI client throws `std::runtime_error` on failures
- OFX plugin layer will catch and convert to OFX error messages
- Allows detailed error messages while maintaining OFX compatibility

### Build Commands Reference

**Install Dependencies:**
```bash
conan install -s build_type=Release \
              -o '&:build_comfyui_plugins=True' \
              -pr:b=default \
              --build=missing .
```

**Configure CMake:**
```bash
cmake --preset conan-release \
      -DBUILD_COMFYUI_PLUGINS=ON
```

**Build:**
```bash
cmake --build build/Release --config Release --parallel
```

**Test Specific Target:**
```bash
cmake --build build/Release --target ComfyUICommon
```

### Known Issues / TODOs

1. ⏳ Conan installation in progress (long build time for boost, etc.)
2. ❌ WebSocket client not yet implemented (needed for progress monitoring)
3. ❌ EXR file I/O not implemented (OpenImageIO removed)
4. ❌ Base plugin render() method incomplete
5. ❌ No parameter definitions yet
6. ❌ No actual plugin implementation (only base classes)
7. ❌ No tests written

### Files Modified This Session

**New Files:**
- `contrib/plugins/ComfyUI/CMakeLists.txt`
- `contrib/plugins/ComfyUI/README.md`
- `contrib/plugins/ComfyUI/common/comfyui_client.h`
- `contrib/plugins/ComfyUI/common/comfyui_client.cpp`
- `contrib/plugins/ComfyUI/common/comfyui_base_plugin.h`
- `contrib/plugins/ComfyUI/common/comfyui_base_plugin.cpp`
- `contrib/docs/pybox-to-ofx-transposition.md`
- `contrib/docs/comfyui-build-guide.md`
- `contrib/docs/SETUP_COMPLETE.md`
- `contrib/docs/PROGRESS_LOG.md` (this file)

**Modified Files:**
- `conanfile.py` - Added ComfyUI dependencies
- `CMakeLists.txt` - Added BUILD_COMFYUI_PLUGINS option

### Metrics

- **Lines of Code Written**: ~500
- **Dependencies Configured**: 6 packages
- **Time Spent on Conan Issues**: ~30 minutes (version conflicts)
- **Documentation Pages**: 5

### Next Session Goals

1. ✅ Complete Conan installation
2. ✅ Successfully compile ComfyUICommon library
3. ✅ Write and run basic HTTP client test
4. ✅ Implement parameter system in base plugin
5. 🎯 Have a minimal working plugin that can connect to ComfyUI

---

## How to Continue

### If Conan Installation Succeeded:
```bash
# 1. Configure CMake
cmake --preset conan-release -DBUILD_COMFYUI_PLUGINS=ON

# 2. Build ComfyUI common library
cmake --build build/Release --target ComfyUICommon

# 3. Check for errors
echo $?  # Should be 0 if successful
```

### If Conan Installation Failed:
1. Check error output
2. May need to adjust dependency versions in `conanfile.py`
3. Consider using prebuilt binaries where available

### To Test HTTP Client:
1. Start ComfyUI server: `python main.py`
2. Create test program in `contrib/plugins/ComfyUI/tests/`
3. Test connection, queue prompt, get history

---

_Last Updated: 2025-10-08_

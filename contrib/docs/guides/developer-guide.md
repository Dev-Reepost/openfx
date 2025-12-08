# ComfyUI OFX Plugin Developer Guide

Complete guide for developing ComfyUI OFX plugins, from foundation to concrete implementations.

## Table of Contents

1. [Overview](#overview)
2. [Architecture](#architecture)
3. [Getting Started](#getting-started)
4. [Creating a New Plugin](#creating-a-new-plugin)
5. [Testing](#testing)
6. [Debugging](#debugging)
7. [Best Practices](#best-practices)
8. [API Reference](#api-reference)

---

## Overview

The ComfyUI OFX plugin system enables integration of AI-powered image processing workflows from ComfyUI into any OFX-compatible host (Flame, Nuke, Resolve, etc.).

### Key Components

- **ComfyUI::Client** - REST + WebSocket client for ComfyUI server communication
- **ComfyUI::BasePlugin** - Abstract base class providing common workflow execution
- **ComfyUI::ImageIO** - EXR file I/O and OFX buffer conversion
- **Concrete Plugins** - Specific implementations (SAM Segmentation, etc.)

### Technology Stack

- **C++17** - Modern C++ features
- **OpenFX API** - Industry-standard plugin specification
- **CMake 3.28+** - Build system
- **Conan 2.1+** - Dependency management
- **nlohmann/json 3.11.3** - JSON parsing
- **cpp-httplib 0.15.3** - HTTP client
- **websocketpp 0.8.2** - WebSocket client
- **TinyEXR 1.0.7** - EXR I/O

---

## Architecture

### System Overview

```
┌─────────────────────────────────────────────────────────────────┐
│                        OFX Host Application                     │
│                   (Flame, Nuke, Resolve, etc.)                  │
└────────────────────────────┬────────────────────────────────────┘
                             │ OFX API
                             │
┌────────────────────────────▼────────────────────────────────────┐
│                      Your Concrete Plugin                       │
│                    (e.g., SAMSegmentation)                      │
│                                                                 │
│  class MyPlugin : public BasePlugin {                           │
│    json buildWorkflow() override {                              │
│      // Build ComfyUI workflow JSON                             │
│    }                                                            │
│  };                                                             │
└────────────────────────────┬────────────────────────────────────┘
                             │ inherits
┌────────────────────────────▼────────────────────────────────────┐
│                      ComfyUI::BasePlugin                        │
│                                                                 │
│  render() → executeWorkflow():                                  │
│    1. writeInputImage()   - OFX → EXR                           │
│    2. buildWorkflow()     - Generate JSON (virtual)             │
│    3. queuePrompt()       - Submit to ComfyUI                   │
│    4. monitorExecution()  - Wait via WebSocket                  │
│    5. getHistory()        - Get results                         │
│    6. readOutputImage()   - EXR → OFX                           │
└────────────────────────────┬────────────────────────────────────┘
                             │ uses
┌────────────────────────────▼────────────────────────────────────┐
│                       ComfyUI::Client                           │
│                                                                 │
│  REST API:     POST /prompt, GET /history, POST /interrupt      │
│  WebSocket:    Real-time event monitoring                       │
│  Threading:    Thread-safe with mutex/cv                        │
└────────────────────────────┬────────────────────────────────────┘
                             │ HTTP/WS
┌────────────────────────────▼────────────────────────────────────┐
│                       ComfyUI Server                            │
│                                                                 │
│  • Receives workflow JSON                                       │
│  • Loads AI models                                              │
│  • Processes images                                             │
│  • Saves results to shared storage                              │
└─────────────────────────────────────────────────────────────────┘
```

### Plugin Lifecycle

```
OFX Host Startup
    ↓
describe() - Define plugin metadata
    ↓
describeInContext() - Define parameters and clips
    ↓
createInstance() - Instantiate plugin
    ↓
Constructor - Fetch parameters, create client
    ↓
[User configures parameters]
    ↓
changedParam() - Handle parameter changes
    ↓
[User applies plugin to clip]
    ↓
render(time) - For each frame:
    ↓
    executeWorkflow()
        1. Write input EXR
        2. Build workflow JSON
        3. Queue to ComfyUI
        4. Monitor via WebSocket
        5. Get history
        6. Read output EXR
    ↓
Destructor - Cleanup
```

---

## Getting Started

### Prerequisites

1. **Development Environment:**

```bash
# macOS
xcode-select --install
brew install cmake conan

# Linux
sudo apt install build-essential cmake python3-pip
pip3 install conan
```

2. **Clone Repository:**

```bash
git clone https://github.com/AcademySoftwareFoundation/openfx.git
cd openfx
```

3. **Install Dependencies:**

```bash
conan install -s build_type=Release \
                -o '&:build_comfyui_plugins=True' \
                -pr:b=default \
                --build=missing .
```

4. **Configure Build:**

   ```bash
   cmake --preset conan-release \
         -DBUILD_COMFYUI_PLUGINS=ON \
         -DBUILD_EXAMPLE_PLUGINS=FALSE
   ```

5. **Build Foundation:**

```bash
cmake --build build/Release --target ComfyUICommon --config Release
```

### Verify Installation

Run the test suite to verify everything is working:

```bash
# Build tests
cmake --build build/Release --target test_comfyui_client --config Release
cmake --build build/Release --target test_image_io --config Release
cmake --build build/Release --target test_base_plugin --config Release

# Run tests (requires ComfyUI server for client tests)
./build/Release/contrib/plugins/ComfyUI/tests/test_comfyui_client --server localhost:8188
./build/Release/contrib/plugins/ComfyUI/tests/test_image_io
./build/Release/contrib/plugins/ComfyUI/tests/test_base_plugin
```

Expected output: All tests passing (21 total).

---

## Creating a New Plugin

### Step 1: Plan Your Workflow

Before coding, design your ComfyUI workflow:

1. Open ComfyUI web interface
2. Create workflow manually
3. Export as API format (Dev Tools → Save API Format)
4. Identify inputs, outputs, and parameters

**Example workflow JSON:**

```json
{
  "1": {
    "inputs": {"image": "input.exr"},
    "class_type": "LoadImage"
  },
  "2": {
    "inputs": {"images": ["1", 0], "scale": 2.0},
    "class_type": "ImageUpscale"
  },
  "3": {
    "inputs": {"images": ["2", 0], "filename_prefix": "output"},
    "class_type": "SaveImage"
  }
}
```

### Step 2: Create Plugin Directory

```bash
mkdir -p contrib/plugins/ComfyUI/my_plugin
cd contrib/plugins/ComfyUI/my_plugin
```

### Step 3: Create Header File

**File:** `my_plugin.h`

```cpp
// Copyright OpenFX and contributors to the OpenFX project.
// SPDX-License-Identifier: BSD-3-Clause

#ifndef COMFYUI_MY_PLUGIN_H
#define COMFYUI_MY_PLUGIN_H

#include "comfyui_base_plugin.h"

namespace ComfyUI {

class MyPlugin : public BasePlugin {
public:
    MyPlugin(OfxImageEffectHandle handle);
    virtual ~MyPlugin();

    // Implement abstract methods
    virtual json buildWorkflow() override;
    virtual std::vector<std::string> getRequiredModels() override;

    // Parameter definition
    static void describeInContext(OFX::ImageEffectDescriptor &desc,
                                  OFX::ContextEnum context);

private:
    // Your parameters
    OFX::DoubleParam *_scaleParam;
    // Add more parameters as needed
};

class MyPluginFactory : public OFX::PluginFactoryHelper<MyPluginFactory> {
public:
    MyPluginFactory();
    virtual void describe(OFX::ImageEffectDescriptor &desc) override;
    virtual void describeInContext(OFX::ImageEffectDescriptor &desc,
                                   OFX::ContextEnum context) override;
    virtual OFX::ImageEffect* createInstance(OfxImageEffectHandle handle,
                                            OFX::ContextEnum context) override;
};

} // namespace ComfyUI

#endif
```

### Step 4: Implement Plugin

**File:** `my_plugin.cpp`

```cpp
#include "my_plugin.h"

namespace ComfyUI {

MyPlugin::MyPlugin(OfxImageEffectHandle handle)
    : BasePlugin(handle)
    , _scaleParam(nullptr)
{
    // Fetch your parameters
    _scaleParam = fetchDoubleParam("scale");
}

MyPlugin::~MyPlugin()
{
}

json MyPlugin::buildWorkflow()
{
    // Get parameter values
    double scale = _scaleParam->getValue();

    // Get storage paths
    std::string mountPath, projectName;
    _sharedMountPath->getValue(mountPath);
    _projectName->getValue(projectName);

    // Build workflow JSON
    json workflow = {
        {"prompt", {
            {"1", {
                {"inputs", {
                    {"image", mountPath + "/" + projectName + "/input_0001.exr"}
                }},
                {"class_type", "LoadImage"}
            }},
            {"2", {
                {"inputs", {
                    {"images", json::array({"1", 0})},
                    {"scale", scale}
                }},
                {"class_type", "ImageUpscale"}
            }},
            {"3", {
                {"inputs", {
                    {"images", json::array({"2", 0})},
                    {"filename_prefix", "output"}
                }},
                {"class_type", "SaveImage"}
            }}
        }},
        {"client_id", _comfyClient->getClientId()}
    };

    return workflow;
}

std::vector<std::string> MyPlugin::getRequiredModels()
{
    return {"RealESRGAN_x2.pth"}; // Example model
}

void MyPlugin::describeInContext(OFX::ImageEffectDescriptor &desc,
                                 OFX::ContextEnum context)
{
    // Add common parameters
    BasePlugin::describeCommonParameters(desc, context);

    // Add your parameters
    OFX::DoubleParamDescriptor *scale = desc.defineDoubleParam("scale");
    scale->setLabel("Scale Factor");
    scale->setHint("Image upscaling factor");
    scale->setDefault(2.0);
    scale->setRange(1.0, 4.0);
    scale->setDisplayRange(1.0, 4.0);
}

// Factory implementation
MyPluginFactory::MyPluginFactory()
    : OFX::PluginFactoryHelper<MyPluginFactory>(
        "com.comfyui.MyPlugin",
        1, 0
    )
{
}

void MyPluginFactory::describe(OFX::ImageEffectDescriptor &desc)
{
    desc.setLabel("ComfyUI My Plugin");
    desc.setPluginDescription("Description of your plugin");

    desc.addSupportedContext(OFX::eContextFilter);
    desc.addSupportedBitDepth(OFX::eBitDepthFloat);

    desc.setSingleInstance(false);
    desc.setHostFrameThreading(false);
    desc.setSupportsMultiResolution(true);
    desc.setSupportsTiles(false);
    desc.setRenderThreadSafety(OFX::eRenderInstanceSafe);
}

void MyPluginFactory::describeInContext(OFX::ImageEffectDescriptor &desc,
                                        OFX::ContextEnum context)
{
    // Define clips
    OFX::ClipDescriptor *srcClip = desc.defineClip(kOfxImageEffectSimpleSourceClipName);
    srcClip->addSupportedComponent(OFX::ePixelComponentRGBA);

    OFX::ClipDescriptor *dstClip = desc.defineClip(kOfxImageEffectOutputClipName);
    dstClip->addSupportedComponent(OFX::ePixelComponentRGBA);

    // Define parameters
    MyPlugin::describeInContext(desc, context);
}

OFX::ImageEffect* MyPluginFactory::createInstance(OfxImageEffectHandle handle,
                                                  OFX::ContextEnum /*context*/)
{
    return new MyPlugin(handle);
}

} // namespace ComfyUI

// Plugin registration
namespace OFX {
namespace Plugin {

void getPluginIDs(OFX::PluginFactoryArray &ids)
{
    static ComfyUI::MyPluginFactory factory;
    ids.push_back(&factory);
}

} // namespace Plugin
} // namespace OFX
```

### Step 5: Create CMakeLists.txt

**File:** `CMakeLists.txt`

```cmake
# Create plugin library
if(APPLE)
    add_library(MyPlugin MODULE my_plugin.cpp)
else()
    add_library(MyPlugin SHARED my_plugin.cpp)
endif()

# Set properties
set_target_properties(MyPlugin PROPERTIES
    SUFFIX ".ofx"
    PREFIX ""
    C_VISIBILITY_PRESET hidden
    CXX_VISIBILITY_PRESET hidden
)

# Force arm64 on macOS to match Conan packages
if(APPLE)
    set_target_properties(MyPlugin PROPERTIES
        OSX_ARCHITECTURES "arm64"
    )
endif()

# Include directories
target_include_directories(MyPlugin PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../common
    ${CMAKE_SOURCE_DIR}/Support/include
)

# Link libraries
target_link_libraries(MyPlugin PRIVATE
    ComfyUICommon
    OfxSupport
    nlohmann_json::nlohmann_json
    httplib::httplib
    websocketpp::websocketpp
    OpenSSL::SSL
    OpenSSL::Crypto
    tinyexr::tinyexr
    miniz::miniz
)

# Create bundle structure on macOS
if(APPLE)
    set(BUNDLE_DIR "${CMAKE_BINARY_DIR}/${CMAKE_BUILD_TYPE}/MyPlugin.ofx.bundle/Contents/MacOS")

    add_custom_command(TARGET MyPlugin POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${BUNDLE_DIR}"
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:MyPlugin> "${BUNDLE_DIR}/"
        COMMENT "Creating MyPlugin.ofx.bundle structure"
    )
endif()

message(STATUS "ComfyUI MyPlugin configured")
```

### Step 6: Register Plugin

Edit `contrib/plugins/ComfyUI/CMakeLists.txt`:

```cmake
# Add your plugin
add_subdirectory(my_plugin)
```

### Step 7: Build and Test

```bash
# Reconfigure
cmake --preset conan-release -DBUILD_COMFYUI_PLUGINS=ON

# Build
cmake --build build/Release --target MyPlugin --config Release

# Install
cp -r build/Release/Release/MyPlugin.ofx.bundle ~/Library/OFX/Plugins/

# Test in OFX host (Flame, Nuke, etc.)
```

---

## Testing

### Unit Testing

Create tests for your workflow logic:

**File:** `contrib/plugins/ComfyUI/tests/test_my_plugin.cpp`

```cpp
#include <iostream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

bool test_workflow_structure() {
    std::cout << "\n[TEST] workflow_structure" << std::endl;

    // Build test workflow
    json workflow = {
        {"prompt", {
            {"1", {
                {"inputs", {{"image", "test.exr"}}},
                {"class_type", "LoadImage"}
            }}
        }}
    };

    // Validate
    if (!workflow.contains("prompt")) {
        std::cerr << "  ✗ Missing prompt" << std::endl;
        return false;
    }

    std::cout << "  ✓ Workflow structure valid" << std::endl;
    return true;
}

int main() {
    int passed = 0;
    int total = 1;

    if (test_workflow_structure()) passed++;

    std::cout << "\nResults: " << passed << "/" << total << " passed\n";
    return (passed == total) ? 0 : 1;
}
```

Add to `contrib/plugins/ComfyUI/tests/CMakeLists.txt`:

```cmake
add_executable(test_my_plugin test_my_plugin.cpp)
target_link_libraries(test_my_plugin PRIVATE
    nlohmann_json::nlohmann_json
)
```

### Integration Testing

Test with live ComfyUI server:

1. **Start ComfyUI:**

   ```bash
   cd ~/ComfyUI
   python main.py --listen 0.0.0.0 --port 8188
   ```

2. **Verify models installed:**

   ```bash
   ls ~/ComfyUI/models/upscale_models/
   ```

3. **Test workflow manually:**

   ```bash
   curl -X POST http://localhost:8188/prompt \
        -H "Content-Type: application/json" \
        -d @test_workflow.json
   ```

4. **Test plugin in OFX host:**
   - Load clip
   - Apply plugin
   - Configure parameters
   - Render single frame
   - Verify output

---

## Debugging

### Enable Verbose Logging

Add debug output to your plugin:

```cpp
#include <iostream>

json MyPlugin::buildWorkflow()
{
    std::cerr << "[DEBUG] Building workflow..." << std::endl;

    double scale;
    _scaleParam->getValue(scale);
    std::cerr << "[DEBUG] Scale: " << scale << std::endl;

    json workflow = /* ... */;

    std::cerr << "[DEBUG] Workflow JSON:\n"
              << workflow.dump(2) << std::endl;

    return workflow;
}
```

### Common Issues

**1. Plugin not appearing in host:**

- Check bundle installed to correct directory
- Verify `getPluginIDs()` registered
- Check OFX host log files

**2. "Failed to connect to server":**

- Verify ComfyUI is running: `curl http://localhost:8188/system_stats`
- Check firewall settings
- Test with `test_comfyui_client`

**3. "Model not found":**

- Verify model installed in ComfyUI
- Check model name matches exactly (case-sensitive)
- Use `getRequiredModels()` for validation

**4. "Failed to find output file":**

- Check ComfyUI logs for errors
- Verify shared storage accessible
- Check file permissions
- Test workflow in ComfyUI web UI first

**5. Architecture errors (x86_64 vs arm64):**

- Add to CMakeLists.txt:

  ```cmake
  if(APPLE)
      set_target_properties(MyPlugin PROPERTIES
          OSX_ARCHITECTURES "arm64"
      )
  endif()
  ```

### Debugging with GDB/LLDB

```bash
# Build with debug symbols
cmake --preset conan-release -DCMAKE_BUILD_TYPE=Debug -DBUILD_COMFYUI_PLUGINS=ON
cmake --build build/Debug --target MyPlugin

# Run under debugger (example with Nuke)
lldb /Applications/Nuke14.0v1/Nuke14.0v1.app/Contents/MacOS/Nuke14.0v1
(lldb) run
# Load clip, apply plugin, trigger error
(lldb) bt  # backtrace
```

---

## Best Practices

### Code Organization

1. **One plugin per directory**

   ```
   my_plugin/
   ├── my_plugin.h
   ├── my_plugin.cpp
   ├── CMakeLists.txt
   └── README.md
   ```

2. **Separate workflow building from parameters**

   ```cpp
   struct WorkflowConfig {
       double scale;
       std::string model;
       int resolution;
   };

   WorkflowConfig getConfig() {
       /* fetch parameters */
   }

   json buildWorkflow() {
       auto config = getConfig();
       /* build workflow */
   }
   ```

3. **Use helper methods**

   ```cpp
   std::string getInputPath(int frame);
   std::string getOutputPath(int frame);
   json createLoadNode(const std::string& path);
   json createSaveNode(const std::string& prefix);
   ```

### Parameter Design

1. **Group related parameters:**

   ```cpp
   OFX::GroupParamDescriptor *group = desc.defineGroupParam("myGroup");
   param->setParent(*group);
   ```

2. **Provide sensible defaults:**

   ```cpp
   threshold->setDefault(0.3);  // Works for most cases
   ```

3. **Add helpful hints:**

   ```cpp
   param->setHint("Higher values = stricter detection. Range: 0.0-1.0");
   ```

4. **Use appropriate ranges:**

   ```cpp
   resolution->setRange(512, 4096);        // Hard limits
   resolution->setDisplayRange(720, 2160); // UI slider range
   ```

### Error Handling

1. **Validate inputs:**

   ```cpp
   if (!_srcClip->isConnected()) {
       setPersistentMessage(OFX::Message::eMessageError, "", "No source connected");
       throwSuiteStatusException(kOfxStatFailed);
   }
   ```

2. **Provide context in errors:**

   ```cpp
   throw std::runtime_error(
       "Failed to load model '" + modelName + "'. "
       "Please install it in ComfyUI's models directory."
   );
   ```

3. **Clean up on error:**

   ```cpp
   try {
       executeWorkflow();
   } catch (const std::exception& e) {
       _state = Error;
       // Cleanup resources
       throw;
   }
   ```

### Performance

1. **Cache expensive operations:**

   ```cpp
   if (_cachedWorkflow.empty()) {
       _cachedWorkflow = buildWorkflow();
   }
   ```

2. **Minimize JSON parsing:**

   ```cpp
   // Build once, reuse
   static const json nodeTemplate = /* ... */;
   ```

3. **Use appropriate resolutions:**

   ```cpp
   // Don't process 4K if 1080p is sufficient
   int resolution = std::min(inputHeight, 1080);
   ```

### Documentation

1. **Document workflow structure:**

   ```cpp
   /**
    * Workflow: LoadImage → Upscale → SaveImage
    *
    * Node 1: LoadImage - Loads input EXR
    * Node 2: Upscale - Scales image by factor
    * Node 3: SaveImage - Saves result
    */
   json buildWorkflow();
   ```

2. **Explain parameters:**

   ```cpp
   // Scale Factor (1.0-4.0)
   // Controls output resolution multiplier.
   // 2.0 = double resolution (1920x1080 → 3840x2160)
   ```

3. **Include examples:**

   ```cpp
   // Example: For 2x upscaling of 1080p:
   // scale = 2.0 → output = 3840x2160
   ```

---

## API Reference

### ComfyUI::BasePlugin

**Abstract Methods (must implement):**

```cpp
virtual json buildWorkflow() = 0;
// Returns ComfyUI workflow JSON
// Called by executeWorkflow() step 2

virtual std::vector<std::string> getRequiredModels() = 0;
// Returns list of required ComfyUI models
// Used for validation (future)
```

**Protected Members:**

```cpp
OFX::Clip *_srcClip;              // Source clip
OFX::Clip *_dstClip;              // Destination clip
OFX::StringParam *_serverAddress;  // ComfyUI server address
OFX::IntParam *_serverPort;        // ComfyUI server port
OFX::StringParam *_sharedMountPath; // Shared storage path
OFX::StringParam *_projectName;    // Project name
OFX::BooleanParam *_enableCache;   // Enable caching
OFX::IntParam *_timeout;           // Timeout (seconds)
std::unique_ptr<Client> _comfyClient; // ComfyUI client
```

**Protected Methods:**

```cpp
void executeWorkflow(const OFX::RenderArguments &args);
// Executes complete workflow (6 steps)
// Throws on error

std::string writeInputImage(OFX::Image* img, int frame);
// Converts OFX buffer → EXR
// Returns path to written file

std::string parseOutputPath(const json& history, int frame);
// Parses ComfyUI history to get output filename
// Returns full path to result EXR
```

**Static Methods:**

```cpp
static void describeCommonParameters(
    OFX::ImageEffectDescriptor &desc,
    OFX::ContextEnum context
);
// Adds common parameters (server, storage, processing)
// Call from your describeInContext()
```

### ComfyUI::Client

**Constructor:**

```cpp
Client(const std::string& serverAddress);
// serverAddress: "hostname:port" or "hostname" (default port 8188)
```

**REST Methods:**

```cpp
bool testConnection();
// Returns: true if server reachable

std::string queuePrompt(const json& workflow, const std::string& clientId);
// Returns: prompt_id for tracking

json getHistory(const std::string& promptId);
// Returns: workflow execution history

void interruptExecution(const std::string& clientId);
// Cancels running workflow
```

**WebSocket Methods:**

```cpp
void monitorExecution(const std::string& promptId, EventCallback callback);
// Blocks until workflow completes
// Callback receives events (Status, Executing, Progress, Error, Cached, Completed)
// Throws on execution error
```

**Getters/Setters:**

```cpp
std::string getClientId() const;
void setTimeout(int seconds);
```

### ComfyUI::ImageIO

**Image I/O:**

```cpp
ImageData readEXR(const std::string& filename);
// Reads EXR file → ImageData (float RGBA)
// Throws on error

void writeEXR(const std::string& filename, const ImageData& image);
// Writes ImageData → EXR file
// Throws on error
```

**Buffer Conversion:**

```cpp
ImageData fromOFXBuffer(
    const void* srcPixels,
    int width, int height,
    int rowBytes,
    int pixelComponents,
    int bitDepth  // 8, 16, or 32
);
// Converts OFX buffer → ImageData
// Handles stride and bit depth conversion

void toOFXBuffer(
    const ImageData& image,
    void* dstPixels,
    int rowBytes,
    int pixelComponents,
    int bitDepth
);
// Converts ImageData → OFX buffer
// Handles stride and bit depth conversion
```

**ImageData Structure:**

```cpp
struct ImageData {
    int width, height, channels;
    std::vector<float> pixels;  // Interleaved RGBA, row-major

    ImageData() : width(0), height(0), channels(0) {}
    ImageData(int w, int h, int c)
        : width(w), height(h), channels(c), pixels(w * h * c) {}
};
```

---

## Appendix

### Example Workflows

**Upscaling (Real-ESRGAN):**

```json
{
  "1": {"inputs": {"image": "input.exr"}, "class_type": "LoadImage"},
  "2": {"inputs": {"model_name": "RealESRGAN_x4plus"}, "class_type": "UpscaleModelLoader"},
  "3": {"inputs": {"upscale_model": ["2", 0], "image": ["1", 0]}, "class_type": "ImageUpscaleWithModel"},
  "4": {"inputs": {"images": ["3", 0], "filename_prefix": "output"}, "class_type": "SaveImage"}
}
```

**Depth Estimation (MiDaS):**

```json
{
  "1": {"inputs": {"image": "input.exr"}, "class_type": "LoadImage"},
  "2": {"inputs": {"model": "dpt_beit_large_512"}, "class_type": "MiDaS-DepthMapPreprocessor"},
  "3": {"inputs": {"image": ["1", 0], "preprocessor": ["2", 0]}, "class_type": "DepthEstimation"},
  "4": {"inputs": {"images": ["3", 0], "filename_prefix": "depth"}, "class_type": "SaveImage"}
}
```

### Useful Resources

- [OpenFX Specification](http://openeffects.org/documentation)
- [ComfyUI API Documentation](https://github.com/comfyanonymous/ComfyUI/wiki)
- [ComfyUI Custom Nodes](https://github.com/ltdrdata/ComfyUI-Manager)
- [TinyEXR Documentation](https://github.com/syoyo/tinyexr)
- [cpp-httplib Documentation](https://github.com/yhirose/cpp-httplib)

### Getting Help

- **Issues:** <https://github.com/AcademySoftwareFoundation/openfx/issues>
- **Discussions:** <https://github.com/AcademySoftwareFoundation/openfx/discussions>
- **ComfyUI Discord:** <https://discord.gg/comfyui>

---

**Last Updated:** 2025-10-09
**Version:** 1.0
**License:** BSD-3-Clause

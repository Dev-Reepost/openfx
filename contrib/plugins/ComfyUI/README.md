# ComfyUI OFX Plugins

OpenFX plugins that integrate with ComfyUI REST server for AI-powered image processing.

## Overview

This directory contains OFX plugins that connect to a ComfyUI server to perform AI operations like:

- Image segmentation (SAM2)
- Image upscaling (Real-ESRGAN)
- Inpainting (Stable Diffusion)
- Style transfer
- And more...

Based on the Flame/Flare PyBox ComfyUI project, these plugins bring the same functionality to **all** OFX-compatible hosts (Flame, Nuke, Resolve, etc.).

## Recent Enhancements (2026-01-21)

### 🎯 JSON Configuration System
External configuration files for plugin defaults - no recompilation needed!
- Configure server address, ports, paths via JSON
- Bundle-based config with graceful fallback to hardcoded defaults
- Perfect for multi-site deployments and environment-specific settings

📖 **[Configuration Guide →](../../docs/CONFIGURATION_SYSTEM.md)**

### ⚡ Adaptive Polling Optimization
90% reduction in idle overhead with faster active response!
- **Fast polling (0.5s)** when jobs are active → 2× faster response
- **Slow polling (5.0s)** when idle → 80% overhead reduction
- Automatic state transitions with logging

📖 **[Adaptive Polling Guide →](../../docs/ADAPTIVE_POLLING.md)**

### 🔧 Other Improvements
- Hidden workflow name parameter for cleaner AnyComfy UI
- Simplified logging (state-change only, 90% verbosity reduction)
- Enhanced job status tracking

📖 **[Session 16 Details →](../../docs/progress/SESSION_16_CONFIGURATION_AND_OPTIMIZATION.md)**

---

## Architecture

### Directory Structure

```bash
ComfyUI/
├── common/                             # Shared code for all plugins
│   ├── comfyui_client.h/.cpp          # REST + WebSocket client
│   ├── comfyui_base_plugin.h/.cpp     # Base plugin class with render()
│   └── comfyui_image_io.h/.cpp        # EXR I/O and OFX buffer conversion
├── tests/                              # Unit tests
│   ├── test_comfyui_client.cpp        # REST/WebSocket tests (10 tests)
│   ├── test_image_io.cpp              # Image I/O tests (4 tests)
│   └── test_base_plugin.cpp           # Workflow tests (7 tests)
├── segmentation/                       # SAM2 segmentation plugin (Phase 2)
├── upscale/                            # Upscaling plugin (Phase 2)
└── CMakeLists.txt                      # Build configuration
```

### Component Architecture

```bash
┌─────────────────────────────────────────────────────────────────────┐
│                         OFX Host Application                        │
│                    (Flame, Nuke, Resolve, etc.)                     │
└────────────────────────────────┬────────────────────────────────────┘
                                 │ OFX API
                                 │
┌────────────────────────────────▼────────────────────────────────────┐
│                         ComfyUI OFX Plugin                          │
│  ┌──────────────────────────────────────────────────────────────┐   │
│  │                     Concrete Plugin                          │   │
│  │              (SAM2Segmentation, Upscale, etc.)               │   │
│  │                                                              │   │
│  │  • Implements buildWorkflow() → JSON workflow                │   │
│  │  • Defines plugin-specific parameters                        │   │
│  │  • Specifies required AI models                              │   │
│  └──────────────────────┬───────────────────────────────────────┘   │
│                         │ inherits                                  │
│  ┌──────────────────────▼───────────────────────────────────────┐   │
│  │                     BasePlugin                               │   │
│  │                                                              │   │
│  │  render() → executeWorkflow() (synchronous with progress):   │   │
│  │    1. writeInputImage()  - OFX buffer → EXR                  │   │
│  │    2. buildWorkflow()    - Generate ComfyUI JSON             │   │
│  │    3. queuePrompt()      - Submit via REST                   │   │
│  │    4. monitorExecution() - Wait via WebSocket + progress     │   │
│  │    5. getHistory()       - Retrieve output info              │   │
│  │    6. readOutputImage()  - EXR → OFX buffer                  │   │
│  │                                                              │   │
│  │  • Common parameters (server, storage, etc.)                 │   │
│  │  • OFX progress reporting (real-time updates)                │   │
│  └──────────────────────┬───────────────────────────────────────┘   │
│                         │ uses                                      │
│  ┌──────────────────────▼───────────────────────────────────────┐   │
│  │                ComfyUI::Client                               │   │
│  │                                                              │   │
│  │  REST API (cpp-httplib):                                     │   │
│  │    • POST /prompt       - Queue workflow                     │   │
│  │    • GET  /history/{id} - Get results                        │   │
│  │    • POST /interrupt    - Cancel execution                   │   │
│  │                                                              │   │
│  │  WebSocket (websocketpp):                                    │   │
│  │    • Real-time event monitoring                              │   │
│  │    • Progress updates                                        │   │
│  │    • Completion detection                                    │   │
│  └──────────────────────┬───────────────────────────────────────┘   │
│                         │                                           │
│  ┌──────────────────────▼───────────────────────────────────────┐   │
│  │              ImageIO (TinyEXR)                               │   │
│  │                                                              │   │
│  │  • readEXR()  - Load EXR from shared storage                 │   │
│  │  • writeEXR() - Save EXR to shared storage                   │   │
│  │  • fromOFXBuffer() - Convert OFX pixels → ImageData          │   │
│  │  • toOFXBuffer()   - Convert ImageData → OFX pixels          │   │
│  │  • Supports 8-bit, 16-bit, 32-bit formats                    │   │
│  └──────────────────────────────────────────────────────────────┘   │
└────────────────────────────────┬────────────────────────────────────┘
                                 │ HTTP/WebSocket + Shared Storage
                                 │
┌────────────────────────────────▼────────────────────────────────────┐
│                         ComfyUI Server                              │
│                                                                     │
│  • Receives workflow JSON via REST API                              │
│  • Loads AI models (SAM2, ESRGAN, etc.)                             │
│  • Processes images using PyTorch                                   │
│  • Saves results to shared storage                                  │
│  • Sends real-time status via WebSocket                             │
└─────────────────────────────────────────────────────────────────────┘
```

### Data Flow

```bash
1. User applies plugin in OFX host
   ↓
2. OFX calls render() with input frame
   ↓
3. Plugin starts progress reporting (progressStart)
   ↓
4. Plugin converts OFX buffer → EXR (ImageIO::fromOFXBuffer + writeEXR)
   ↓ (progress: 10%)
5. Plugin builds ComfyUI workflow JSON (buildWorkflow)
   ↓ (progress: 15%)
6. Plugin queues workflow via REST (Client::queuePrompt)
   ↓ (progress: 20%)
7. Plugin monitors execution via WebSocket (Client::monitorExecution)
   │  • Real-time progress updates from server (30%-80%)
   │  • Event-driven (no polling)
   ↓
8. ComfyUI server completes processing, saves EXR result
   ↓ (progress: 85%)
9. Plugin retrieves output filename (Client::getHistory)
   ↓ (progress: 90%)
10. Plugin reads result EXR (ImageIO::readEXR)
   ↓ (progress: 95%)
11. Plugin converts EXR → OFX buffer (ImageIO::toOFXBuffer)
   ↓ (progress: 100%)
12. Plugin ends progress reporting (progressEnd)
   ↓
13. OFX host displays result frame
```

## Dependencies

All dependencies are managed by **Conan** and automatically downloaded during build:

- **nlohmann/json 3.11.3** - JSON parsing for workflows and API responses
- **cpp-httplib 0.15.3** - HTTP/HTTPS client for REST API communication
- **websocketpp 0.8.2** - WebSocket client for real-time event monitoring
- **tinyexr 1.0.7** - Lightweight EXR image file I/O (header-only)
- **openssl 3.2.1** - HTTPS encryption for secure connections
- **boost 1.84.0** - C++ utilities (for websocketpp)
- **miniz** - Compression (transitive dependency of TinyEXR)

## Building

### Prerequisites

- CMake 3.28+
- Conan 2.1+
- C++17 compiler
- ComfyUI server (for runtime)

### Build Commands

**Option 1: Using build script** (recommended)

```bash
# From OpenFX root directory
./scripts/build-cmake.sh -o build_comfyui_plugins=True Release
```

**Option 2: Manual build**

```bash
# Install dependencies with Conan
conan install -s build_type=Release \
              -o build_comfyui_plugins=True \
              -pr:b=default \
              --build=missing .

# Configure CMake
cmake --preset conan-release \
      -DBUILD_COMFYUI_PLUGINS=ON

# Build
cmake --build build/Release --config Release --parallel

# Install plugins
cmake --build build/Release --target install --config Release
```

### Build Options

- `BUILD_COMFYUI_PLUGINS=ON/OFF` - Enable ComfyUI plugin compilation (default: OFF)

## Installation

Plugins are installed to standard OFX directories:

- **macOS**: `~/Library/OFX/Plugins/`
- **Linux**: `/usr/OFX/Plugins/` or `~/.local/share/OFX/Plugins/`
- **Windows**: `C:\Program Files\Common Files\OFX\Plugins\`

## ComfyUI Server Setup

### Requirements

1. **ComfyUI server** running and accessible
2. **Shared network storage** for image exchange
3. **Required AI models** installed on server

### Configuration

Each plugin requires configuration parameters:

- **Server Address**: Hostname/IP of ComfyUI server (e.g., `localhost` or `192.168.1.100`)
- **Server Port**: ComfyUI port (default: `8188`)
- **Shared Mount Path**: Network path accessible to both OFX host and ComfyUI server
- **Project Name**: Identifier for organizing files

### Directory Structure

```
<shared-mount>/
├── in/<project>/segmentation/       # Input images (written by OFX plugin)
├── out/<project>/segmentation/      # Output images (written by ComfyUI)
└── models/                          # AI models (optional)
```

## Development Status

### Phase 1: Foundation (COMPLETE ✅)

**ComfyUI Client** ✅

- ✅ REST API implementation (POST /prompt, GET /history, POST /interrupt)
- ✅ WebSocket monitoring with event callbacks
- ✅ PIMPL pattern for clean API encapsulation
- ✅ Exception-based error handling
- ✅ Thread-safe execution monitoring
- ✅ 10 unit tests (all passing)

**Image I/O** ✅

- ✅ TinyEXR integration for EXR reading/writing
- ✅ OFX buffer conversion (fromOFXBuffer, toOFXBuffer)
- ✅ Multi-bit-depth support (8-bit, 16-bit, 32-bit)
- ✅ Channel de-interleaving for EXR format
- ✅ 4 unit tests (all passing)

**Base Plugin** ✅

- ✅ Template method pattern (buildWorkflow() abstract)
- ✅ Synchronous render() with OFX progress reporting
- ✅ Real-time progress updates from WebSocket (0-100%)
- ✅ Parameter system (server, storage, processing)
- ✅ Python-compatible filename pattern (basename_layer_frame_version_.exr)
- ✅ Thread-safe concurrent frame rendering
- ✅ Dynamic client creation on parameter changes
- ✅ ComfyUI history parsing
- ✅ Dimension validation
- ✅ 7 workflow tests (all passing)

**Build System** ✅

- ✅ CMake integration with BUILD_COMFYUI_PLUGINS option
- ✅ Conan dependency management
- ✅ Universal binary support (macOS arm64 + x86_64)
- ✅ Test suite configuration

**Documentation** ✅

- ✅ README with architecture diagrams
- ✅ Build guide with troubleshooting
- ✅ Comprehensive inline code documentation
- ✅ Test documentation

**Test Coverage**: 21 tests total (10 client + 4 image I/O + 7 workflow)
**Lines of Code**: ~1,800 across 8 files

### Phase 2: Concrete Plugins (IN PROGRESS)

1. **SAM Segmentation Plugin** ✅ **COMPLETE**
   - ✅ Grounding DINO + SAM workflow implementation
   - ✅ 8 segmentation-specific parameters (prompt, threshold, models, resolution)
   - ✅ Multiple model options (3 SAM models, 2 Grounding DINO models)
   - ✅ Color space conversion support
   - ✅ Full workflow JSON generation (7 nodes)
   - ✅ Builds successfully (arm64 binary, 1.7 MB)
   - ✅ Comprehensive documentation (README + developer guide + user guide)
   - ⏳ Integration testing with live ComfyUI server (pending)
   - See: [segmentation/README.md](segmentation/README.md)

2. **Additional Plugins** (TODO)
   - Upscaling (Real-ESRGAN, GFPGAN)
   - Inpainting (Stable Diffusion)
   - Style Transfer
   - Depth Estimation

3. **Advanced Features**
   - ✅ Progress bar integration (OFX progress reporting with real-time WebSocket updates)
   - ⏳ Abort/Cancel support (TODO)
   - ⏳ Multi-output support (RGB + alpha matte) (TODO)
   - ⏳ Frame caching (avoid reprocessing identical frames) (TODO)
   - ⏳ Model validation (check server has required models) (TODO)

## Testing

### Unit Tests

Three test suites validate core functionality:

**1. ComfyUI Client Tests** (`test_comfyui_client`)

```bash
./build/Release/contrib/plugins/ComfyUI/tests/test_comfyui_client
```

- 10 tests covering REST API and WebSocket monitoring
- Requires live ComfyUI server at 192.168.1.211:8188
- Tests: connection, workflow queuing, execution monitoring, history retrieval

**4. SAM Integration Tests** (`test_sam_integration`)

```bash
./build/Release/contrib/plugins/ComfyUI/tests/test_sam_integration
```

- 3 tests for SAM segmentation workflow end-to-end
- Requires live ComfyUI server with Segment Anything extension
- Tests: server availability, full workflow execution, model variants
- **Note**: Requires shared network storage accessible to both client and server

**2. Image I/O Tests** (`test_image_io`)

```bash
./build/Release/contrib/plugins/ComfyUI/tests/test_image_io
```

- 4 tests for EXR reading/writing and buffer conversion
- Standalone, no server required
- Tests: EXR creation, round-trip, buffer conversion, gradients

**3. Workflow Tests** (`test_base_plugin`)

```bash
./build/Release/contrib/plugins/ComfyUI/tests/test_base_plugin
```

- 7 tests for workflow execution logic
- Standalone, no server required
- Tests: JSON parsing, filename generation, round-trip, validation

**Run All Tests**

```bash
cmake --build build/Release --target test_comfyui_client --config Release
./build/Release/contrib/plugins/ComfyUI/tests/test_comfyui_client

cmake --build build/Release --target test_image_io --config Release
./build/Release/contrib/plugins/ComfyUI/tests/test_image_io

cmake --build build/Release --target test_base_plugin --config Release
./build/Release/contrib/plugins/ComfyUI/tests/test_base_plugin
```

### Integration Tests

Integration testing with live ComfyUI server:

```bash
# 1. Start ComfyUI server
python main.py --listen 0.0.0.0 --port 8188

# 2. Build and install plugin
cmake --build build/Release --config Release
cmake --build build/Release --target install --config Release

# 3. Load plugin in OFX host (e.g., Flame)
# 4. Configure server connection parameters
#    - Server Address: 192.168.1.211 (or localhost)
#    - Server Port: 8188
#    - Shared Mount Path: /path/to/shared/storage
#    - Project Name: test_project
# 5. Apply plugin to test clip and render
```

## Documentation

### For Developers

- **[Developer Guide](../../docs/developer-guide.md)** - Complete guide for creating ComfyUI plugins
  - Architecture overview
  - Getting started tutorial
  - Step-by-step plugin creation
  - API reference
  - Testing and debugging

- **[Transposition Guide](../../docs/pybox-to-ofx-transposition.md)** - Original architecture and implementation plan
- **[Progress Log](../../docs/PROGRESS_LOG.md)** - Detailed implementation history

### For VFX Artists

- **[VFX Artist Guide](../../docs/vfx-artist-guide.md)** - User guide for production workflows
  - Installation instructions
  - Quick start tutorials
  - Plugin reference
  - Workflows and best practices
  - Troubleshooting

- **[SAM Segmentation README](segmentation/README.md)** - SAM plugin user guide

### Technical References

- **[OpenFX Documentation](../../../Documentation/)** - OFX API reference
- **[ComfyUI API](https://github.com/comfyanonymous/ComfyUI)** - ComfyUI server documentation
- **[Build Guide](../../docs/comfyui-build-guide.md)** - Build system documentation

## Contributing

Follow the existing code style and patterns:

- Use SPDX license headers
- Follow OpenFX naming conventions
- Document all public APIs
- Add tests for new functionality

## License

BSD-3-Clause (same as OpenFX)

## Contact

Based on the PyBox ComfyUI projects:

- <https://github.com/Dev-Reepost/flame_comfyui_client>
- <https://github.com/Dev-Reepost/flame_comfyui_pybox>
- <https://github.com/Dev-Reepost/flame_comfyui_segmentation>

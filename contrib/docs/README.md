# ComfyUI OFX Plugin Documentation

Complete documentation for the ComfyUI OpenFX plugin system.

---

## 📚 User Guides → [`guides/`](guides/)

**Start here for practical usage:**
- **[VFX Artist Guide](guides/vfx-artist-guide.md)** - Using plugins in Flame/Nuke/Resolve
- **[Developer Guide](guides/developer-guide.md)** - Creating new plugins
- **[Development Guide](guides/development-guide.md)** - Advanced patterns
- **[Build Guide](guides/comfyui-build-guide.md)** - Compiling from source

See [guides/README.md](guides/README.md) for complete guide index.

---

## 🏗️ Architecture & Design

### Core Concepts
- **[IMPLEMENTATION_SUMMARY.md](IMPLEMENTATION_SUMMARY.md)** - Complete project overview (start here!)
- **[PYBOX_VS_OFX_RENDERING_MODELS.md](PYBOX_VS_OFX_RENDERING_MODELS.md)** - PyBox vs OFX comparison
- **[pybox-to-ofx-transposition.md](pybox-to-ofx-transposition.md)** - Original architecture plan (85KB)

### Technical Investigations
- **[OFX_ASYNC_INVESTIGATION.md](OFX_ASYNC_INVESTIGATION.md)** - Why OFX doesn't support async re-renders
- **[OFX_GPU_ASYNC_INVESTIGATION.md](OFX_GPU_ASYNC_INVESTIGATION.md)** - GPU async analysis (GPUGain example)
- **[OFX_DOCUMENTATION_ANALYSIS.md](OFX_DOCUMENTATION_ANALYSIS.md)** - OFX spec limitations analysis
- **[THREADING_AND_REFERENCE_ANALYSIS.md](THREADING_AND_REFERENCE_ANALYSIS.md)** - Python reference analysis

---

## 📖 Development History → [`progress/`](progress/)

**Complete development timeline with all 10 sessions:**
- **[PROGRESS_LOG.md](progress/PROGRESS_LOG.md)** - All 10 sessions, chronological history (1,700+ lines)

**Phase 1: Foundation (Sessions 1-5)**
- **[SESSION_1_PROJECT_SETUP.md](progress/SESSION_1_PROJECT_SETUP.md)** - Project initialization, dependencies
- **[SESSION_2_CLIENT_IMPLEMENTATION.md](progress/SESSION_2_CLIENT_IMPLEMENTATION.md)** - REST API client (PIMPL pattern)
- **[SESSION_3_WEBSOCKET_IMPLEMENTATION.md](progress/SESSION_3_WEBSOCKET_IMPLEMENTATION.md)** - WebSocket monitoring
- **[SESSION_4_CLIENT_TESTING.md](progress/SESSION_4_CLIENT_TESTING.md)** - Unit tests (10 tests, 100% pass)
- Session 5 in [PROGRESS_LOG.md](progress/PROGRESS_LOG.md) - Image I/O, parameters, render()

**Phase 2: Plugins (Sessions 6-8)**
- Session 6 in [PROGRESS_LOG.md](progress/PROGRESS_LOG.md) - SAM Segmentation plugin
- **[SESSION_7_INTEGRATION_TESTING.md](progress/SESSION_7_INTEGRATION_TESTING.md)** - Live server integration tests
- **[SESSION_8_DIRECTORY_STRUCTURE.md](progress/SESSION_8_DIRECTORY_STRUCTURE.md)** - Production alignment

**Phase 3: Critical Fixes (Sessions 9-10)**
- **[SESSION_9_CRITICAL_FIXES.md](progress/SESSION_9_CRITICAL_FIXES.md)** - Python-compatible naming, thread safety
- **[SESSION_10_SYNCHRONOUS_IMPLEMENTATION.md](progress/SESSION_10_SYNCHRONOUS_IMPLEMENTATION.md)** - Synchronous + progress (final approach)

See [progress/README.md](progress/README.md) for complete session index and timeline.

---

## 📊 Project Status

### Summary
- **Development Time**: 3 days (10 sessions)
- **Lines of Code**: ~2,300
- **Test Coverage**: 24 tests (21 unit + 3 integration)
- **Status**: ✅ Production-ready foundation

### Implementation
- ✅ **Phase 1 Complete** - REST/WebSocket client, image I/O, base plugin
- ✅ **SAM Plugin Complete** - Grounding DINO + SAM2 segmentation
- ✅ **Synchronous + Progress** - Final rendering approach
- ✅ **Python-Compatible** - Exact filename pattern matching
- ✅ **Thread-Safe** - Concurrent frame rendering

---

## 🗂️ Documentation Structure

```
contrib/docs/
├── README.md                              # This file
│
├── guides/                                # User-facing guides
│   ├── README.md                          # Guides index
│   ├── vfx-artist-guide.md               # For artists
│   ├── developer-guide.md                # For developers
│   ├── development-guide.md              # Advanced patterns
│   └── comfyui-build-guide.md            # Build instructions
│
├── IMPLEMENTATION_SUMMARY.md              # Complete overview
├── PROGRESS_LOG.md                        # Complete development history
│
├── Architecture & Design                  # Technical analysis
│   ├── PYBOX_VS_OFX_RENDERING_MODELS.md
│   ├── OFX_ASYNC_INVESTIGATION.md
│   ├── OFX_GPU_ASYNC_INVESTIGATION.md
│   ├── OFX_DOCUMENTATION_ANALYSIS.md
│   ├── THREADING_AND_REFERENCE_ANALYSIS.md
│   └── pybox-to-ofx-transposition.md
│
└── Session Notes                          # Development sessions
    ├── SESSION_1_PROJECT_SETUP.md
    ├── SESSION_2_CLIENT_IMPLEMENTATION.md
    ├── SESSION_3_WEBSOCKET_IMPLEMENTATION.md
    ├── SESSION_4_CLIENT_TESTING.md
    ├── SESSION_7_INTEGRATION_TESTING.md
    ├── SESSION_8_DIRECTORY_STRUCTURE.md
    ├── SESSION_9_CRITICAL_FIXES.md
    └── SESSION_10_SYNCHRONOUS_IMPLEMENTATION.md
```

---

## 🎯 Where to Start

**I want to...**

| Goal | Start Here |
|------|-----------|
| **Use plugins in my VFX workflow** | [VFX Artist Guide](guides/vfx-artist-guide.md) |
| **Create a new plugin** | [Developer Guide](guides/developer-guide.md) |
| **Understand the architecture** | [IMPLEMENTATION_SUMMARY.md](IMPLEMENTATION_SUMMARY.md) |
| **Build from source** | [Build Guide](guides/comfyui-build-guide.md) |
| **Understand PyBox vs OFX** | [PYBOX_VS_OFX_RENDERING_MODELS.md](PYBOX_VS_OFX_RENDERING_MODELS.md) |
| **See development history** | [PROGRESS_LOG.md](PROGRESS_LOG.md) |
| **Understand async limitations** | [OFX_ASYNC_INVESTIGATION.md](OFX_ASYNC_INVESTIGATION.md) |

---

## 📝 Key Technical Decisions

### Why Synchronous + Progress?

**Decision**: Use synchronous rendering with OFX progress reporting instead of async with cache.

**Rationale**:
- OFX has no API to trigger automatic re-renders
- GPU async only works for host-managed streams (not network calls)
- Real-time WebSocket progress provides good UX
- Standard pattern for heavy CPU/network operations

**Details**: See [SESSION_10_SYNCHRONOUS_IMPLEMENTATION.md](SESSION_10_SYNCHRONOUS_IMPLEMENTATION.md)

### Why Python-Compatible Naming?

**Decision**: Use `basename_layer_frame_version_.exr` pattern.

**Rationale**:
- Matches flame_pybox_comfyui reference implementation
- Prevents file collisions across shots/layers
- Enables drop-in replacement for PyBox workflows

**Details**: See [SESSION_9_CRITICAL_FIXES.md](SESSION_9_CRITICAL_FIXES.md)

### Why TinyEXR over OpenImageIO?

**Decision**: Use TinyEXR for image I/O instead of OpenImageIO.

**Rationale**:
- Avoids 50+ transitive dependencies
- Header-only library (simple integration)
- Sufficient for EXR-only workflow
- Fast compilation

**Details**: See [IMPLEMENTATION_SUMMARY.md](IMPLEMENTATION_SUMMARY.md)

---

## 🔗 Related Documentation

**In Plugin Directories:**
- [../plugins/ComfyUI/README.md](../plugins/ComfyUI/README.md) - Main plugin README
- [../plugins/ComfyUI/segmentation/README.md](../plugins/ComfyUI/segmentation/README.md) - SAM plugin docs

**OpenFX Resources:**
- [OpenFX Specification](https://openfx.readthedocs.io/)
- [OpenFX GitHub](https://github.com/AcademySoftwareFoundation/openfx)

**Reference Implementations:**
- [flame_comfyui_client](https://github.com/Dev-Reepost/flame_comfyui_client)
- [flame_comfyui_segmentation](https://github.com/Dev-Reepost/flame_comfyui_segmentation)

---

## 📈 Metrics

**Development Statistics:**
- **Total Sessions**: 10
- **Development Days**: 3
- **Production Code**: ~2,300 lines
- **Test Code**: ~900 lines
- **Documentation**: ~15,000+ lines
- **Test Pass Rate**: 100% (24/24 tests)

**Code Distribution:**
- comfyui_client.cpp: 450 lines
- comfyui_base_plugin.cpp: 450 lines
- comfyui_image_io.cpp: 250 lines
- sam_segmentation_plugin.cpp: 400 lines
- Test suites: ~900 lines

---

## 🐛 Troubleshooting

**Common Issues:**
- **Build errors**: See [Build Guide](guides/comfyui-build-guide.md)
- **Plugin not loading**: See [VFX Artist Guide](guides/vfx-artist-guide.md)
- **Server connection**: See [SESSION_7_INTEGRATION_TESTING.md](SESSION_7_INTEGRATION_TESTING.md)
- **Performance issues**: See [IMPLEMENTATION_SUMMARY.md](IMPLEMENTATION_SUMMARY.md)

---

**Last Updated**: 2025-10-10
**Status**: ✅ Production-Ready

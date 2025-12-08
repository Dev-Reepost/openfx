# ComfyUI OFX Plugin - User Guides

This directory contains user-facing guides for the ComfyUI OFX plugin system.

---

## For End Users (VFX Artists)

### [VFX Artist Guide](vfx-artist-guide.md)
Complete guide for artists using ComfyUI plugins in their host applications (Flame, Nuke, Resolve).

**Contents:**
- Installation instructions
- Quick start tutorials
- Plugin reference (parameters, usage)
- Production workflows
- Troubleshooting
- Tips & best practices

**Start here if you're**: Using ComfyUI plugins in your VFX workflow.

---

## For Developers

### [Developer Guide](developer-guide.md)
Comprehensive guide for developers creating new ComfyUI OFX plugins.

**Contents:**
- Architecture overview
- Getting started (environment setup)
- Creating your first plugin
- Plugin lifecycle and API
- Testing and debugging
- Best practices
- Complete API reference

**Start here if you're**: Building new plugins or extending existing ones.

### [Development Guide](development-guide.md)
Additional development resources and patterns.

**Contents:**
- Plugin development patterns
- Advanced techniques
- Code organization
- Architecture patterns

**Start here if you're**: Looking for design patterns and advanced topics.

---

## For System Administrators

### [ComfyUI Build Guide](comfyui-build-guide.md)
Build system documentation and setup instructions.

**Contents:**
- Prerequisites (CMake, Conan, compilers)
- Build instructions (all platforms)
- Configuration options
- Troubleshooting build issues
- Dependency management

**Start here if you're**: Building the plugins from source or setting up build infrastructure.

---

## Quick Reference

| Guide | Audience | Purpose |
|-------|----------|---------|
| [vfx-artist-guide.md](vfx-artist-guide.md) | Artists | Using plugins in production |
| [developer-guide.md](developer-guide.md) | Developers | Creating new plugins |
| [development-guide.md](development-guide.md) | Developers | Advanced patterns |
| [comfyui-build-guide.md](comfyui-build-guide.md) | Build engineers | Compiling from source |

---

## Related Documentation

**In Parent Directory** (`../`):

### Architecture & Design
- [PYBOX_VS_OFX_RENDERING_MODELS.md](../PYBOX_VS_OFX_RENDERING_MODELS.md) - PyBox vs OFX comparison
- [OFX_ASYNC_INVESTIGATION.md](../OFX_ASYNC_INVESTIGATION.md) - Async rendering investigation
- [OFX_GPU_ASYNC_INVESTIGATION.md](../OFX_GPU_ASYNC_INVESTIGATION.md) - GPU async analysis
- [OFX_DOCUMENTATION_ANALYSIS.md](../OFX_DOCUMENTATION_ANALYSIS.md) - OFX spec analysis
- [IMPLEMENTATION_SUMMARY.md](../IMPLEMENTATION_SUMMARY.md) - Complete project summary

### Session Notes
- [SESSION_1_PROJECT_SETUP.md](../SESSION_1_PROJECT_SETUP.md) - Initial setup
- [SESSION_2_CLIENT_IMPLEMENTATION.md](../SESSION_2_CLIENT_IMPLEMENTATION.md) - REST client
- [SESSION_3_WEBSOCKET_IMPLEMENTATION.md](../SESSION_3_WEBSOCKET_IMPLEMENTATION.md) - WebSocket
- [SESSION_4_CLIENT_TESTING.md](../SESSION_4_CLIENT_TESTING.md) - Testing
- [SESSION_7_INTEGRATION_TESTING.md](../SESSION_7_INTEGRATION_TESTING.md) - Integration tests
- [SESSION_8_DIRECTORY_STRUCTURE.md](../SESSION_8_DIRECTORY_STRUCTURE.md) - Production alignment
- [SESSION_9_CRITICAL_FIXES.md](../SESSION_9_CRITICAL_FIXES.md) - Python reference fixes
- [SESSION_10_SYNCHRONOUS_IMPLEMENTATION.md](../SESSION_10_SYNCHRONOUS_IMPLEMENTATION.md) - Final approach

### Project Documentation
- [PROGRESS_LOG.md](../PROGRESS_LOG.md) - Complete development history
- [pybox-to-ofx-transposition.md](../pybox-to-ofx-transposition.md) - Original architecture plan

---

## Getting Help

**Questions?**
- Check the relevant guide above
- Review session notes for technical decisions
- Check [PROGRESS_LOG.md](../PROGRESS_LOG.md) for development history
- See [IMPLEMENTATION_SUMMARY.md](../IMPLEMENTATION_SUMMARY.md) for overview

**Issues?**
- VFX Artists: See [vfx-artist-guide.md](vfx-artist-guide.md) → Troubleshooting
- Developers: See [developer-guide.md](developer-guide.md) → Testing & Debugging
- Build Issues: See [comfyui-build-guide.md](comfyui-build-guide.md) → Troubleshooting

---

**Last Updated**: 2025-10-10

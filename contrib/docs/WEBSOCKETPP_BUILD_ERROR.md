# WebSocket++ Build Error - Boost ASIO Compatibility

## Error Summary

The ComfyUI plugin build is failing with websocketpp/boost ASIO compatibility errors:

```
error: no type named 'io_service' in namespace 'websocketpp::lib::asio'
error: no member named 'expires_from_now' in 'boost::asio::basic_waitable_timer<std::chrono::steady_clock>'
```

## Root Cause

WebSocket++ 0.8.2 (from Conan) is incompatible with Boost 1.84.0. Boost deprecated `io_service` (renamed to `io_context`) and changed the timer API.

## Affected File

`contrib/plugins/ComfyUI/common/comfyui_client.cpp` - This file uses websocketpp for WebSocket connections to ComfyUI server.

## NOT Affected

The logging changes we just made in `comfyui_base_plugin.cpp` are **NOT the cause** of this error. The build fails before it even gets to that file.

## Solutions

### Option 1: Downgrade Boost (Quick Fix)

Change `conanfile.py`:
```python
self.requires("boost/1.82.0", override=True)  # Instead of 1.84.0
```

### Option 2: Upgrade WebSocket++ (Better Fix)

WebSocket++ main branch has fixes for newer Boost, but Conan only has 0.8.2.

Would need to either:
- Build websocketpp from source
- Find a newer Conan package
- Apply patches to 0.8.2

### Option 3: Use Different WebSocket Library (Long-term)

Replace websocketpp with a more actively maintained library:
- **websocket**pp-asio
- **Boost.Beast** (part of Boost)
- **libwebsockets**
- **ixwebsocket**

### Option 4: Build Without WebSocket (Testing Only)

Temporarily disable websocket functionality to test the logging:
- Comment out websocket includes in comfyui_client.cpp
- Build only the base plugin
- Test property logging

## Immediate Workaround for Testing

Since the **logging code is in `comfyui_base_plugin.cpp`** and the error is in `comfyui_client.cpp`, we can:

1. **Use an existing working build** (if available)
2. **Fix the websocket issue** then rebuild
3. **Test the logging separately** by compiling just base_plugin.cpp

## Recommendation

**Downgrade Boost to 1.82.0** as a quick fix:

```bash
# Edit conanfile.py
sed -i '' 's/boost\/1.84.0/boost\/1.82.0/' conanfile.py

# Reinstall dependencies
conan install . -s build_type=Release -pr:b=default --build=missing -o build_comfyui_plugins=True

# Rebuild
./contrib/dev-tools/build-plugin.sh contrib/plugins/ComfyUI SAMSegmentation
```

## Status

- ✅ **Logging code is correct** and ready
- ❌ **Build is blocked** by websocketpp/boost incompatibility
- 🔧 **Fix needed** before testing

## Related

- [Issue #1](https://github.com/zaphoyd/websocketpp/issues/1070) - websocketpp boost 1.70+ compatibility
- [Boost 1.70 Migration](https://www.boost.org/doc/libs/1_70_0/doc/html/boost_asio/history.html) - io_service deprecation

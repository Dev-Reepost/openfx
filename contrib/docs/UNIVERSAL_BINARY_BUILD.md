# Quick Guide: Building Universal Binary Plugins

## The Problem

Your SAMSegmentation plugin works on M1 (arm64) Mac but not on x86_64 Intel Mac running Flame because it's compiled for arm64 only.

## The Solution

Build the plugin on **both** architectures and combine them using `lipo`.

## Quick Steps

### 1. On Apple Silicon Mac (M1/M2/M3) - Build arm64

```bash
cd ~/src/openfx
./scripts/build-cmake.sh Release -DBUILD_COMFYUI_PLUGINS=ON
```

Binary location:
```
build/Release/Release/SAMSegmentation.ofx.bundle/Contents/MacOS/SAMSegmentation.ofx
```

### 2. On Intel Mac (x86_64) - Build x86_64

Transfer your openfx source to the Intel Mac, then:

```bash
cd /path/to/openfx
./scripts/build-cmake.sh Release -DBUILD_COMFYUI_PLUGINS=ON
```

Create tarball for transfer:
```bash
tar czf SAMSegmentation-x86_64.tar.gz \
    build/Release/Release/SAMSegmentation.ofx.bundle
```

### 3. Transfer to M1 Mac

```bash
# On Intel Mac
scp SAMSegmentation-x86_64.tar.gz user@m1-mac:/tmp/

# On M1 Mac
cd /tmp
tar xzf SAMSegmentation-x86_64.tar.gz
```

### 4. Combine into Universal Binary

On M1 Mac:

```bash
cd ~/src/openfx

./contrib/dev-tools/combine-universal-binary.sh \
    SAMSegmentation \
    build/Release/Release/SAMSegmentation.ofx.bundle/Contents/MacOS/SAMSegmentation.ofx \
    /tmp/build/Release/Release/SAMSegmentation.ofx.bundle/Contents/MacOS/SAMSegmentation.ofx \
    --install
```

This will create and install:
```
~/Library/OFX/Plugins/SAMSegmentation-universal.ofx.bundle
```

### 5. Verify

```bash
lipo -info ~/Library/OFX/Plugins/SAMSegmentation-universal.ofx.bundle/Contents/MacOS/SAMSegmentation.ofx
```

Should output:
```
Architectures in the fat file: ... are: x86_64 arm64
```

## Why This Approach?

The ComfyUI plugins use Homebrew dependencies (websocketpp, tinyexr, OpenSSL). Homebrew provides:
- **arm64 only** on Apple Silicon Macs
- **x86_64 only** on Intel Macs

Therefore, each architecture must be built on its native platform.

## Detailed Documentation

See [universal-binary-guide.md](guides/universal-binary-guide.md) for:
- Complete explanations
- Troubleshooting
- Alternative approaches
- CI/CD pipeline setup

## Tools Provided

1. **[combine-universal-binary.sh](../dev-tools/combine-universal-binary.sh)** - Combines arm64 + x86_64 binaries
2. **[build-macos-universal-plugin.sh](../dev-tools/build-macos-universal-plugin.sh)** - Automated build (requires Conan dependencies for both architectures)

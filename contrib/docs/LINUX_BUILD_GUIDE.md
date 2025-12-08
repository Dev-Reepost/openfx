# Linux Build Guide

This guide explains how to build OpenFX ComfyUI plugins for Linux from macOS using Docker.

## Prerequisites

- Docker Desktop for Mac installed and running
- At least 10GB free disk space for Docker images and builds

### Docker Troubleshooting

If you encounter "exec format error" when running Docker containers:

1. **Restart Docker Desktop**: Quit Docker Desktop completely and restart it
2. **Reset Docker**: In Docker Desktop settings, choose "Troubleshoot" → "Reset to factory defaults"
3. **Check Docker is running**: `docker ps` should work without errors
4. **Test basic container**: `docker run --rm alpine echo "test"` should work

If problems persist, this indicates a Docker Desktop issue unrelated to the build scripts.

## Build Options

### Option 1: ARM64 Linux Build (Recommended)

Build for ARM64 Linux systems (most modern cloud instances, Raspberry Pi 4+, etc.):

```bash
./contrib/dev-tools/build-linux-plugin.sh \
    -p SAMSegmentation \
    -t SAMSegmentation \
    --install
```

**Output:** `build/linux/contrib/plugins/ComfyUI/segmentation/SAMSegmentation.ofx`
**Architecture:** ARM aarch64
**Build time:** ~5-10 minutes

### Option 2: x86_64 Linux Build

For x86_64 Linux builds from macOS, you have two options:

#### A. Build on an actual x86_64 Linux machine (Recommended)

Transfer the source code to a Linux machine and build natively:

```bash
# On Linux x86_64 machine
sudo apt-get install build-essential cmake python3-pip pkg-config libgl1-mesa-dev
pip3 install conan

# Build
./contrib/dev-tools/build-plugin.sh \
    contrib/plugins/ComfyUI/segmentation \
    SAMSegmentation \
    --install-dir ~/.OFX/Plugins
```

#### B. Use QEMU emulation (Slow)

Build x86_64 from macOS using QEMU emulation:

```bash
# Setup QEMU (one-time)
docker run --rm --privileged multiarch/qemu-user-static --reset -p yes

# Build multi-platform image
docker buildx create --name multiarch --driver docker-container --use
docker buildx build --platform linux/amd64,linux/arm64 \
    -t openfx-linux-builder:latest \
    -f contrib/dev-tools/Dockerfile.linux \
    contrib/dev-tools/ \
    --load

# Build plugin
./contrib/dev-tools/build-linux-universal-plugin.sh \
    -p SAMSegmentation \
    -t SAMSegmentation \
    --install
```

**Warning:** x86_64 builds via emulation are 5-10x slower (~30-60 minutes).

## Deployment

### Installing on Linux

**System-wide installation:**
```bash
sudo cp -r /path/to/SAMSegmentation.ofx.bundle /usr/OFX/Plugins/
```

**User installation:**
```bash
mkdir -p ~/.OFX/Plugins
cp -r /path/to/SAMSegmentation.ofx.bundle ~/.OFX/Plugins/
```

### Verifying the Binary

```bash
# Check architecture
file SAMSegmentation.ofx.bundle/Contents/Linux-x86-64/SAMSegmentation.ofx

# Expected output for ARM64:
# ELF 64-bit LSB shared object, ARM aarch64, version 1 (GNU/Linux)

# Expected output for x86_64:
# ELF 64-bit LSB shared object, x86-64, version 1 (GNU/Linux)
```

## Troubleshooting

### Docker not running
```
Error: Cannot connect to the Docker daemon
```
**Solution:** Start Docker Desktop

### Out of disk space
```
Error: No space left on device
```
**Solution:** Clean Docker: `docker system prune -a`

### Build fails with missing dependencies
```
Error: Could not find package 'xyz'
```
**Solution:** The Dockerfile may need additional packages. Edit `contrib/dev-tools/Dockerfile.linux` and add them to the `apt-get install` line.

## Build Architecture Summary

| Build Method | Command | Architecture | Build Time | Use Case |
|--------------|---------|--------------|------------|----------|
| **Native ARM64** | `build-linux-plugin.sh` | ARM aarch64 | 5-10 min | Modern Linux, cloud |
| **Native x86_64** | Build on Linux machine | x86-64 | 5-10 min | Traditional Linux servers |
| **Emulated x86_64** | `build-linux-universal-plugin.sh` | x86-64 | 30-60 min | Testing only |

**Recommendation:** Build ARM64 version from macOS, and build x86_64 version natively on a Linux x86_64 machine for production use.

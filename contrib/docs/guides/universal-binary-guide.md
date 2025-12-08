# Building Universal Binary OFX Plugins

This guide explains how to create universal binary (arm64 + x86_64) OFX plugins for macOS.

## Background

OpenFX plugins on macOS need to support both architectures:
- **arm64**: Apple Silicon (M1, M2, M3, etc.)
- **x86_64**: Intel-based Macs

Professional applications like Flame may run on either architecture, so universal binaries ensure compatibility.

## Why You Need Both Builds

The ComfyUI plugins depend on Homebrew packages (tinyexr, websocketpp, OpenSSL, etc.). Homebrew provides:
- **arm64 only** on Apple Silicon Macs
- **x86_64 only** on Intel Macs

Therefore, you **must** build each architecture on its native platform.

## Step-by-Step Process

### Step 1: Build arm64 Version (On Apple Silicon Mac)

```bash
# On M1/M2/M3 Mac
cd /path/to/openfx
./scripts/build-cmake.sh Release -DBUILD_COMFYUI_PLUGINS=ON
```

The binary will be at:
```
build/Release/Release/SAMSegmentation.ofx.bundle/Contents/MacOS/SAMSegmentation.ofx
```

Verify it's arm64:
```bash
file build/Release/Release/SAMSegmentation.ofx.bundle/Contents/MacOS/SAMSegmentation.ofx
# Should output: Mach-O 64-bit bundle arm64
```

### Step 2: Build x86_64 Version (On Intel Mac)

```bash
# On Intel Mac
cd /path/to/openfx
./scripts/build-cmake.sh Release -DBUILD_COMFYUI_PLUGINS=ON
```

The binary will be at the same relative path:
```
build/Release/Release/SAMSegmentation.ofx.bundle/Contents/MacOS/SAMSegmentation.ofx
```

Verify it's x86_64:
```bash
file build/Release/Release/SAMSegmentation.ofx.bundle/Contents/MacOS/SAMSegmentation.ofx
# Should output: Mach-O 64-bit bundle x86_64
```

### Step 3: Transfer x86_64 Binary

Copy the x86_64 binary from the Intel Mac to your Apple Silicon Mac:

```bash
# On Intel Mac - create a tarball
cd /path/to/openfx
tar czf SAMSegmentation-x86_64.tar.gz \
    build/Release/Release/SAMSegmentation.ofx.bundle

# Transfer to Apple Silicon Mac (via scp, USB drive, etc.)
scp SAMSegmentation-x86_64.tar.gz user@m1-mac:/tmp/
```

### Step 4: Combine into Universal Binary

On your Apple Silicon Mac, use the provided script:

```bash
cd /path/to/openfx

# Extract the x86_64 bundle
cd /tmp
tar xzf SAMSegmentation-x86_64.tar.gz

# Run the combine script
./contrib/dev-tools/combine-universal-binary.sh \
    SAMSegmentation \
    build/Release/Release/SAMSegmentation.ofx.bundle/Contents/MacOS/SAMSegmentation.ofx \
    /tmp/build/Release/Release/SAMSegmentation.ofx.bundle/Contents/MacOS/SAMSegmentation.ofx
```

### Step 5: Install Universal Plugin

```bash
# Install to Flame's plugin directory
cp -r build/Release/SAMSegmentation-universal.ofx.bundle \
    ~/Library/OFX/Plugins/
```

## Verification

Verify the universal binary contains both architectures:

```bash
lipo -info ~/Library/OFX/Plugins/SAMSegmentation-universal.ofx.bundle/Contents/MacOS/SAMSegmentation.ofx
# Should output: Architectures in the fat file: ... are: x86_64 arm64
```

## Automated Script

Use [combine-universal-binary.sh](../../dev-tools/combine-universal-binary.sh) to automate Step 4:

```bash
./contrib/dev-tools/combine-universal-binary.sh --help
```

## Troubleshooting

### "Library not found" errors on Intel Mac

Make sure Homebrew packages are installed on the Intel Mac:
```bash
brew install websocketpp openssl@3
```

### Different dependency versions

Ensure both Macs have matching versions of dependencies. Check with:
```bash
brew list --versions websocketpp openssl
```

### Bundle structure differences

Both bundles must have identical Info.plist and directory structures. The script handles this automatically.

## Alternative: Rosetta 2

If you only have an Apple Silicon Mac and the Intel Mac is unavailable temporarily:

1. Build arm64 version as normal
2. Test on x86_64 Mac using **Rosetta 2** (translation layer)
3. Performance will be slightly slower but functionality should work

**Note**: Professional users expect native performance, so ship universal binaries for production use.

## CI/CD Pipeline

For automated builds, set up:
1. arm64 builder (GitHub Actions `macos-14` runner)
2. x86_64 builder (GitHub Actions `macos-13` runner)
3. Combine step using `lipo`
4. Upload universal bundle as artifact

See `.github/workflows/build-universal.yml` example (coming soon).

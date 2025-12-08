<!-- SPDX-License-Identifier: CC-BY-4.0 -->
<!-- Copyright OpenFX and contributors to the OpenFX project. -->

# OpenFX Development Tools

Enhanced development tools for OpenFX plugin development.

## Overview

This directory contains build and development tools for OpenFX plugins. The tools are organized by purpose:

**Plugin Development:**

- `create-plugin.sh` - Generate plugin templates
- `setup-env.sh` - Setup development environment

**Building Plugins:**

- `build-plugin.sh` - Build for current platform/architecture (native)
- `build-macos-universal-plugin.sh` - Build macOS universal binaries (arm64 + x86_64)
- `build-linux-plugin.sh` - Cross-compile for Linux using Docker
- `combine-universal-binary.sh` - Combine pre-built arm64 + x86_64 binaries

**See [BUILD_WORKFLOWS.md](../docs/BUILD_WORKFLOWS.md) for detailed build documentation.**

## Available Tools

### create-plugin.sh - Plugin Template Generator

Generate complete plugin templates with customizable options.

**Usage:**
```bash
./contrib/dev-tools/create-plugin.sh <plugin-name> [options]
```

**Options:**
- `-t, --type TYPE` - Plugin type: filter, generator, transition (default: filter)
- `-c, --category CAT` - Plugin category (default: "Color")
- `-d, --description DESC` - Plugin description
- `-a, --author AUTHOR` - Plugin author (default: "Your Name")
- `-h, --help` - Show help

**Examples:**
```bash
# Create a simple filter plugin
./contrib/dev-tools/create-plugin.sh MyColorEffect

# Create a generator plugin with custom settings
./contrib/dev-tools/create-plugin.sh NoiseGenerator -t generator -c "Generators" -a "John Doe"

# Create a transition plugin
./contrib/dev-tools/create-plugin.sh CrossFade -t transition -d "Custom transition effect"
```

### build-plugin.sh - Native Plugin Builder

Build plugins for your current platform and architecture. This is the **fastest and simplest** option for day-to-day development.

**Use this when:**

- Developing and testing on your current machine
- Quick iterative builds during development
- You don't need cross-architecture support

**Usage:**

```bash
./contrib/dev-tools/build-plugin.sh <plugin-directory> [target-name] [options]
```

**Options:**

- `-d, --debug` - Debug build
- `-c, --clean` - Clean build (removes build directory first)
- `-v, --verbose` - Verbose output
- `--bundle-name NAME` - Custom bundle name
- `--install-dir DIR` - Custom installation directory

**Examples:**

```bash
# Build plugin with automatic detection
./contrib/dev-tools/build-plugin.sh contrib/plugins/MyPlugin

# Build with specific target name
./contrib/dev-tools/build-plugin.sh contrib/plugins/MyPlugin MyPlugin-support

# Debug build with verbose output
./contrib/dev-tools/build-plugin.sh contrib/plugins/MyPlugin MyPlugin-support -d -v
```

**Build time:** 2-5 minutes

**Output:** Native architecture binary (arm64 on M1/M2/M3 Mac, x86_64 on Intel Mac)

### build-macos-universal-plugin.sh - macOS Universal Binary Builder

Build macOS universal binaries that work on both Apple Silicon and Intel Macs. Use this when preparing plugins for distribution.

**Use this when:**

- Preparing plugins for distribution to end users
- Need to support both Apple Silicon and Intel Macs
- Building final release versions

**Usage:**

```bash
./contrib/dev-tools/build-macos-universal-plugin.sh [options]
```

**Options:**

- `-p, --plugin NAME` - Plugin name (default: SAMSegmentation)
- `-t, --target NAME` - CMake target name (default: SAMSegmentation)
- `-c, --clean` - Clean build directories before building
- `-i, --install` - Install to ~/Library/OFX/Plugins after building
- `--install-dir DIR` - Install to custom directory

**Examples:**

```bash
# Build SAMSegmentation as universal binary
./contrib/dev-tools/build-macos-universal-plugin.sh

# Build and install to user plugins directory
./contrib/dev-tools/build-macos-universal-plugin.sh -i

# Build custom plugin
./contrib/dev-tools/build-macos-universal-plugin.sh -p MyPlugin -t MyPlugin-support

# Clean build and install
./contrib/dev-tools/build-macos-universal-plugin.sh --clean --install
```

**Build time:** 30-40 minutes (first build), 5-10 minutes (cached)

**Output:** Universal binary (arm64 + x86_64)

**Requirements:** macOS, Conan 2.x, CMake 3.28+

### build-linux-plugin.sh - Linux Cross-Compilation Builder

Build Linux binaries from macOS using Docker. Use this when you need to test or distribute Linux versions of your plugins.

**Use this when:**

- Building Linux versions from macOS
- Testing cross-platform compatibility
- Preparing Linux distribution packages

**Usage:**

```bash
./contrib/dev-tools/build-linux-plugin.sh [options]
```

**Options:**

- `-p, --plugin NAME` - Plugin name (default: SAMSegmentation)
- `-t, --target NAME` - CMake target name (default: SAMSegmentation)
- `-o, --output DIR` - Output directory (default: build/linux)
- `--install` - Copy to output directory after build

**Examples:**

```bash
# Build Linux version of SAMSegmentation
./contrib/dev-tools/build-linux-plugin.sh

# Build and install custom plugin
./contrib/dev-tools/build-linux-plugin.sh -p MyPlugin -t MyPlugin --install
```

**Build time:** 15-20 minutes

**Output:** Linux ELF binary (x86_64)

**Requirements:** Docker Desktop for Mac

### combine-universal-binary.sh - Universal Binary Combiner

Low-level utility to combine pre-built arm64 and x86_64 binaries into a universal binary. Only needed in special circumstances.

**Use this when:**

- You already have pre-built arm64 and x86_64 binaries from different sources
- Building on separate machines and combining results
- Advanced CI/CD workflows

**Most users should use `build-macos-universal-plugin.sh` instead.**

**Usage:**

```bash
./contrib/dev-tools/combine-universal-binary.sh <plugin-name> <arm64-binary> <x86_64-binary> [options]
```

**Options:**

- `-o, --output DIR` - Output directory (default: build/Release)
- `-i, --install` - Install to ~/Library/OFX/Plugins after combining

**Example:**

```bash
# Combine binaries from two different builds
./contrib/dev-tools/combine-universal-binary.sh SAMSegmentation \
    build/arm64/SAMSegmentation.ofx \
    build/x86_64/SAMSegmentation.ofx \
    --install
```

### setup-env.sh - Environment Setup

Automated setup of OpenFX development environment (macOS only).

**Usage:**
```bash
./contrib/dev-tools/setup-env.sh
```

**Features:**
- Installs Conan package manager
- Sets up plugin directories
- Configures development environment
- Builds OpenFX framework

## Quick Start Workflows

### For Development (Recommended)

Fast iterative development on your current machine:

1. **Setup environment** (first time only):

   ```bash
   ./contrib/dev-tools/setup-env.sh
   ```

2. **Create a new plugin**:

   ```bash
   ./contrib/dev-tools/create-plugin.sh MyEffect -t filter -c "Color" -d "My custom effect"
   ```

3. **Build and install** (native architecture):

   ```bash
   ./contrib/dev-tools/build-plugin.sh contrib/plugins/MyEffect MyEffect-support
   ```

4. **Test in your OpenFX host** (Flame, Nuke, etc.)

### For Distribution

Building plugins for end users (universal binaries):

1. **Build universal macOS binary**:

   ```bash
   ./contrib/dev-tools/build-macos-universal-plugin.sh -p MyEffect -t MyEffect-support -i
   ```

2. **Build Linux binary** (if needed):

   ```bash
   ./contrib/dev-tools/build-linux-plugin.sh -p MyEffect -t MyEffect-support --install
   ```

3. **Package and distribute** to users

## Which Build Script Should I Use?

| Scenario | Script to Use | Why |
|----------|--------------|-----|
| **Daily development** | `build-plugin.sh` | Fast (2-5 min), builds for your current machine |
| **Testing on both Mac architectures** | `build-macos-universal-plugin.sh` | Works on Apple Silicon AND Intel Macs |
| **Distributing to Mac users** | `build-macos-universal-plugin.sh` | Ensures compatibility with all Macs |
| **Distributing to Linux users** | `build-linux-plugin.sh` | Cross-compiles Linux binaries from macOS |
| **CI/CD with separate builds** | `combine-universal-binary.sh` | Combines binaries from different build machines |

**In doubt? Use `build-plugin.sh` for development, `build-macos-universal-plugin.sh` for distribution.**

## Directory Structure

Created plugins follow this structure:
```
contrib/plugins/MyEffect/
├── myeffect.cpp          # Plugin source code
├── CMakeLists.txt        # Build configuration
└── README.md             # Plugin documentation
```

Built plugins are installed to:
- **Development**: `~/OFX/Plugins/`
- **User**: `~/Library/OFX/Plugins/` (for host applications)

## Integration

All plugins in `contrib/plugins/` are automatically discovered by the build system. No manual CMakeLists.txt editing required.

## Troubleshooting

### Common Issues

**Build fails with missing dependencies:**
```bash
# Run setup script to install dependencies
./contrib/dev-tools/setup-env.sh
```

**Plugin not found in host application:**
```bash
# Check installation directories
ls ~/Library/OFX/Plugins/
ls ~/OFX/Plugins/
```

**CMake configuration errors:**
```bash
# Clean build and try again
./contrib/dev-tools/build-plugin.sh MyPlugin MyPlugin-support --clean
```

For more help, see the main [OpenFX documentation](../../Documentation/) or [CLAUDE.md](../../CLAUDE.md).
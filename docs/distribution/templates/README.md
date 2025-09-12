# KGroundControl Distribution Packages

Welcome to KGroundControl! This directory contains all available distribution packages for Linux x86_64 systems.

## Available Installation Methods

Choose the installation method that best fits your needs:

### 🚀 AppImage (Recommended for most users)
**Best for:** Quick testing, portable usage, or when you don't want to modify your system.

- **File:** `AppImage/KGroundControl-d577689-x86_64.AppImage`
- **Size:** ~30 MB
- **Installation:** No installation required - just run it!
- **Dependencies:** Self-contained, includes all required libraries
- **Perfect for:** Testing, development, or users who prefer portable applications

[📖 Detailed AppImage Instructions](./AppImage/README.md)

### 📦 DEB Package (Recommended for Debian/Ubuntu)
**Best for:** Native system integration on Debian-based distributions.

- **File:** `DEB/KGroundControl-1.3.deb`
- **Size:** ~XX MB (includes all resources)
- **Installation:** Native package installation
- **Dependencies:** Automatically managed by package manager
- **Perfect for:** Production use, system administrators, Ubuntu/Debian users

[📖 Detailed DEB Instructions](./DEB/README.md)

### 📁 Tarball (Minimal Installation)
**Best for:** Custom installations, minimal footprint, or when you want full control.

- **File:** `Tarball/KGroundControl-1.3-x86_64.tar.gz`
- **Size:** ~XX MB (minimal, executable only)
- **Installation:** Manual extraction and setup
- **Dependencies:** Must be installed separately
- **Perfect for:** Advanced users, custom deployments, or minimal systems

[📖 Detailed Tarball Instructions](./Tarball/README.md)

### 🔧 Standalone Executable
**Best for:** Quick testing or when you have all dependencies installed.

- **File:** `bin/KGroundControl`
- **Size:** Minimal
- **Installation:** Just run it (if dependencies are met)
- **Dependencies:** Must be installed on system
- **Perfect for:** Development, testing, or when you already have Qt6 installed

## Quick Start Guide

### For New Users (AppImage)
```bash
# Navigate to the AppImage directory
cd AppImage/

# Make executable and run
chmod +x KGroundControl-d577689-x86_64.AppImage
./KGroundControl-d577689-x86_64.AppImage
```

### For Ubuntu/Debian Users (DEB)
```bash
# Navigate to DEB directory
cd DEB/

# Install the package
sudo apt install ./KGroundControl-1.3.deb

# Launch the application
kgroundcontrol
```

### For Advanced Users (Tarball)
```bash
# Navigate to Tarball directory
cd Tarball/

# Extract and install
tar -xzf KGroundControl-1.3-x86_64.tar.gz
sudo mv bin/KGroundControl /usr/local/bin/
sudo chmod +x /usr/local/bin/KGroundControl

# Launch the application
kgroundcontrol
```

## System Requirements

### Minimum Requirements
- **OS:** Linux x86_64
- **RAM:** 512 MB
- **Storage:** 100 MB free space
- **Display:** X11 or Wayland

### Recommended Requirements
- **OS:** Ubuntu 20.04+, Fedora 35+, or similar modern Linux distribution
- **RAM:** 1 GB
- **Storage:** 500 MB free space
- **Display:** Modern graphics with OpenGL support

## Dependencies by Installation Method

| Dependency | AppImage | DEB | Tarball | Standalone |
|------------|----------|-----|---------|------------|
| Qt6 Core | ✅ Included | ✅ Auto | ❌ Manual | ❌ Manual |
| Qt6 GUI | ✅ Included | ✅ Auto | ❌ Manual | ❌ Manual |
| Qt6 Widgets | ✅ Included | ✅ Auto | ❌ Manual | ❌ Manual |
| SDL2 | ✅ Included | ✅ Auto | ❌ Manual | ❌ Manual |
| OpenGL | ✅ Included | ✅ Auto | ❌ Manual | ❌ Manual |
| System libs | ✅ Included | ✅ Auto | ❌ Manual | ❌ Manual |

## Troubleshooting

### Common Issues

**Application won't start:**
- Check system requirements
- Ensure executable permissions: `chmod +x <filename>`
- Check system logs: `journalctl -f`

**Graphics/display issues:**
- Try running with software rendering: `LIBGL_ALWAYS_SOFTWARE=1 <command>`
- Check graphics drivers
- Ensure X11/Wayland is properly configured

**Permission issues:**
- Use `sudo` for system-wide installations
- Check file permissions: `ls -la <file>`

**Missing dependencies (Tarball/Standalone):**
- Install required packages for your distribution
- Use `ldd <executable>` to check missing libraries

### Getting Help
- Check the specific README in each package directory
- Review system logs for error messages
- Ensure your system meets the minimum requirements

## Build Information
- **Version:** 1.3
- **Architecture:** x86_64
- **Qt Version:** 6.9.2
- **Build Date:** September 11, 2025
- **Build Type:** Release

## Support
For issues or questions:
1. Check the troubleshooting section above
2. Review the detailed instructions in each package directory
3. Check system compatibility and dependencies

---
*Choose the installation method that best fits your use case and system configuration.*</content>
<parameter name="filePath">/media/jack/ca3fc60d-388d-401b-b35a-8819e69248a5/jack/GitHub/KGroundControl/DistributionKit/x86_64/README.md
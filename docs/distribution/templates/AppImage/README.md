# KGroundControl AppImage Installation

## What is an AppImage?
AppImage is a portable application format that runs on most Linux distributions without installation.

## Installation & Usage

### Method 1: Direct Execution (Recommended)
1. Make the AppImage executable:
   ```bash
   chmod +x KGroundControl-d577689-x86_64.AppImage
   ```

2. Run the application:
   ```bash
   ./KGroundControl-d577689-x86_64.AppImage
   ```

### Method 2: System Integration (Optional)
For better desktop integration, you can integrate the AppImage with your system:

1. Install AppImageLauncher (recommended):
   ```bash
   # On Ubuntu/Debian:
   sudo apt install appimagelauncher

   # On Fedora:
   sudo dnf install appimagelauncher

   # On Arch Linux:
   sudo pacman -S appimagelauncher
   ```

2. Double-click the AppImage file or run it through AppImageLauncher

## Features
- ✅ Portable - no installation required
- ✅ Self-contained - includes all dependencies
- ✅ Runs on most Linux distributions
- ✅ Can be run from any location

## System Requirements
- Linux x86_64 architecture
- X11 or Wayland display server
- Compatible with most modern Linux distributions

## Troubleshooting
- If the AppImage doesn't run, ensure it's executable: `chmod +x *.AppImage`
- For graphics issues, try running with: `./KGroundControl-d577689-x86_64.AppImage --no-sandbox`
- Check system logs with: `journalctl -f` while running the application

---
**Build Information:**
- Architecture: x86_64
- Qt Version: 6.9.2
- Build Date: September 11, 2025</content>
<parameter name="filePath">/media/jack/ca3fc60d-388d-401b-b35a-8819e69248a5/jack/GitHub/KGroundControl/DistributionKit/x86_64/AppImage/README.md
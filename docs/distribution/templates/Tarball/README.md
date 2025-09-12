# KGroundControl Tarball Installation

## What is a Tarball?
This is a compressed archive (.tar.gz) containing only the essential files needed to run KGroundControl. It's designed for manual installation on any Linux distribution.

## Installation Methods

### Method 1: System-wide Installation (Recommended)
```bash
# Extract the archive
tar -xzf KGroundControl-1.3-x86_64.tar.gz

# Create installation directory
sudo mkdir -p /opt/kgroundcontrol

# Move files to installation directory
sudo mv bin/KGroundControl /opt/kgroundcontrol/
sudo mv ARCHITECTURE.txt /opt/kgroundcontrol/

# Make executable
sudo chmod +x /opt/kgroundcontrol/KGroundControl

# Create symlink in /usr/local/bin (optional)
sudo ln -sf /opt/kgroundcontrol/KGroundControl /usr/local/bin/kgroundcontrol
```

### Method 2: User Installation (No Root Required)
```bash
# Extract to home directory
tar -xzf KGroundControl-1.3-x86_64.tar.gz

# Move to user applications directory
mkdir -p ~/Applications
mv bin/KGroundControl ~/Applications/
mv ARCHITECTURE.txt ~/Applications/

# Make executable
chmod +x ~/Applications/KGroundControl

# Add to PATH (optional - add to ~/.bashrc or ~/.zshrc)
export PATH="$HOME/Applications:$PATH"
```

### Method 3: Temporary/Portable Usage
```bash
# Extract anywhere
tar -xzf KGroundControl-1.3-x86_64.tar.gz

# Make executable
chmod +x bin/KGroundControl

# Run directly
./bin/KGroundControl
```

## After Installation

### Launch the Application
```bash
# If installed system-wide
kgroundcontrol
# or
/opt/kgroundcontrol/KGroundControl

# If installed in user directory
~/Applications/KGroundControl

# If running from extracted directory
./bin/KGroundControl
```

## Desktop Integration (Optional)

### Create Desktop Entry
Create a file `~/.local/share/applications/kgroundcontrol.desktop`:

```desktop
[Desktop Entry]
Name=KGroundControl
Comment=Ground Control Application
Exec=/opt/kgroundcontrol/KGroundControl
Icon=kgroundcontrol
Terminal=false
Type=Application
Categories=Utility;Development;
```

### Create Launcher Script (Optional)
Create `/usr/local/bin/kgroundcontrol`:

```bash
#!/bin/bash
/opt/kgroundcontrol/KGroundControl "$@"
```

Make it executable:
```bash
sudo chmod +x /usr/local/bin/kgroundcontrol
```

## Uninstallation

### System-wide Installation
```bash
# Remove installation directory
sudo rm -rf /opt/kgroundcontrol

# Remove symlink (if created)
sudo rm -f /usr/local/bin/kgroundcontrol

# Remove desktop entry (if created)
rm -f ~/.local/share/applications/kgroundcontrol.desktop
```

### User Installation
```bash
# Remove from user directory
rm -rf ~/Applications/KGroundControl
rm -f ~/Applications/ARCHITECTURE.txt
```

## Features
- ✅ Minimal installation - only essential files
- ✅ Works on any Linux distribution
- ✅ No dependency conflicts
- ✅ Portable - can be moved or copied
- ✅ Customizable installation location

## System Requirements
- Linux x86_64 architecture
- X11 or Wayland display server
- Compatible with most Linux distributions
- **Note:** You may need to install system dependencies manually

## Required Dependencies
This tarball contains only the KGroundControl executable. You may need to install these system dependencies:

### Ubuntu/Debian
```bash
sudo apt update
sudo apt install libqt6core6 libqt6gui6 libqt6widgets6 libqt6network6 \
                 libqt6opengl6 libqt6multimedia6 libqt6serialport6 \
                 libsdl2-2.0-0 libgl1-mesa-glx
```

### Fedora
```bash
sudo dnf install qt6-qtbase qt6-qtmultimedia SDL2 mesa-libGL
```

### Arch Linux
```bash
sudo pacman -S qt6-base qt6-multimedia sdl2
```

## Troubleshooting

### Missing Libraries
If you get library errors, check what's missing:
```bash
ldd /opt/kgroundcontrol/KGroundControl
```

### Graphics Issues
```bash
# Try running with software rendering
LIBGL_ALWAYS_SOFTWARE=1 /opt/kgroundcontrol/KGroundControl
```

### Permission Issues
```bash
# Ensure executable permissions
chmod +x /opt/kgroundcontrol/KGroundControl
```

### PATH Issues
```bash
# Check if in PATH
which kgroundcontrol

# Add to PATH if needed
export PATH="/opt/kgroundcontrol:$PATH"
```

---
**Package Contents:**
- `bin/KGroundControl` - Main executable
- `ARCHITECTURE.txt` - Build information

**Build Information:**
- Architecture: x86_64
- Qt Version: 6.9.2
- Build Date: September 11, 2025
- Package Type: Minimal tarball (executable only)</content>
<parameter name="filePath">/media/jack/ca3fc60d-388d-401b-b35a-8819e69248a5/jack/GitHub/KGroundControl/DistributionKit/x86_64/Tarball/README.md
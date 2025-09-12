# KGroundControl DEB Package Installation

## What is a DEB Package?
DEB is the native package format for Debian-based Linux distributions (Ubuntu, Debian, Linux Mint, etc.).

## Installation Methods

### Method 1: Using dpkg (Recommended)
```bash
sudo dpkg -i KGroundControl-1.3.deb
```

### Method 2: Using apt (Alternative)
```bash
sudo apt install ./KGroundControl-1.3.deb
```

### Method 3: Using gdebi (If available)
```bash
sudo gdebi KGroundControl-1.3.deb
```

## After Installation

### Launch the Application
- **From terminal:** `kgroundcontrol`
- **From applications menu:** Search for "KGroundControl"
- **From desktop:** Look for the KGroundControl icon

### Verify Installation
```bash
# Check if package is installed
dpkg -l | grep kgroundcontrol

# Check installation location
dpkg -L kgroundcontrol
```

## Uninstallation

### Remove the Package
```bash
sudo apt remove kgroundcontrol
# or
sudo dpkg -r kgroundcontrol
```

### Remove with Configuration Files
```bash
sudo apt purge kgroundcontrol
# or
sudo dpkg -P kgroundcontrol
```

## Features
- ✅ Native system integration
- ✅ Automatic dependency management
- ✅ Easy updates through package manager
- ✅ Follows system conventions
- ✅ Includes desktop integration

## System Requirements
- Debian-based Linux distribution (Ubuntu, Debian, Linux Mint, etc.)
- x86_64 architecture
- Compatible with most modern Debian derivatives

## Troubleshooting

### Dependency Issues
If you encounter dependency errors:
```bash
# Fix broken dependencies
sudo apt --fix-broken install

# Install missing dependencies
sudo apt install -f
```

### Permission Issues
```bash
# Ensure you have sudo privileges
sudo -l
```

### Package Database Issues
```bash
# Update package database
sudo apt update

# Clean package cache
sudo apt clean && sudo apt autoclean
```

### Desktop Integration Issues
```bash
# Update desktop database
sudo update-desktop-database

# Update icon cache
sudo gtk-update-icon-cache /usr/share/icons/hicolor/
```

---
**Package Information:**
- Package Name: kgroundcontrol
- Version: 1.3
- Architecture: x86_64
- Build Date: September 11, 2025</content>
<parameter name="filePath">/media/jack/ca3fc60d-388d-401b-b35a-8819e69248a5/jack/GitHub/KGroundControl/DistributionKit/x86_64/DEB/README.md
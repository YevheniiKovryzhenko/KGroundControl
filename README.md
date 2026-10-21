# KGroundControl (KGC)

<p align="center">
  <img src="resources/Images/Logo/KGC_Logo.png" alt="KGroundControl logo" width="220">
</p>

**Cross-platform ground control, MAVLink routing, and monitoring for UAV research.**

KGroundControl combines serial and UDP communication, multi-device data forwarding, motion capture, joystick control, visualization, and logging in one application. It runs independently and can also work alongside QGroundControl.

[Download the latest release](https://github.com/YevheniiKovryzhenko/KGroundControl/releases/latest) · [Watch demonstrations](https://www.youtube.com/playlist?list=PLgxIoIw6ONulUfYvzxfoZNM6QXrcRqKkV) · [Report an issue](https://github.com/YevheniiKovryzhenko/KGroundControl/issues)

## Features

- **MAVLink communication:** serial and UDP connections, routing between ports, and communication with multiple devices through a single connection.
- **Motion capture:** OptiTrack streaming to multiple UAVs; designed for Motive 2.3.0.
- **Remote control:** multiple joysticks and configurable control-message assignments.
- **Visualization:** live 2D and 3D data plotting.
- **Logging:** MAVLink data capture and PX4 ULog writing.
- **Updates:** in-application update checks and downloads.

## Install KGroundControl

**To use KGC, download a published release. You do not need to clone the repository, install Qt, or set up a compiler or IDE.**

1. Open the [latest published release](https://github.com/YevheniiKovryzhenko/KGroundControl/releases/latest).
2. Under **Assets**, choose the executable or deployment package matching your operating system, Linux distribution where specified, and processor architecture.
3. If you downloaded an archive, extract the entire package before launching it. Keep any accompanying DLLs and plugin folders beside the executable.

GitHub's automatically generated **Source code** archives are for developers; they are not runnable application packages. If the latest release has no asset for your platform, check [all releases](https://github.com/YevheniiKovryzhenko/KGroundControl/releases) or follow the [development setup](#development-setup).

### Windows

Download the Windows x64 asset and run its `.exe` after extracting the package. Keep the supplied runtime DLLs and plugin folders in place. If Windows reports a missing runtime DLL, follow the release's dependency instructions rather than downloading individual DLLs from third-party sites.

### Linux

Download the asset matching your distribution and architecture and extract it if necessary. On **Ubuntu 24.04**, install the runtime libraries below, including on minimal desktop installations:

```bash
sudo apt-get update
sudo apt-get install --no-install-recommends \
  ca-certificates fonts-dejavu-core libssl3t64 libicu74 \
  libstdc++6 libudev1 libglib2.0-0t64 libdouble-conversion3 \
  libpcre2-16-0 libzstd1 zlib1g libbrotli1 libgssapi-krb5-2 \
  libfontconfig1 libharfbuzz0b libpng16-16t64 \
  libxkbcommon-x11-0 libx11-xcb1 libxcb-cursor0 libxcb-icccm4 \
  libxcb-keysyms1 libxcb-randr0 libxcb-sync1 libxcb-xfixes0 libxcb-shape0 libsm6 \
  libgl1 libegl1 libasound2t64 libpulse0
```

These are runtime packages; Qt/SDL development packages and build tools are not needed. APT installs their required dependencies and skips packages already installed. Ubuntu's **Universe** repository must be enabled for packages such as [`libxcb-cursor0`](https://packages.ubuntu.com/noble/libxcb-cursor0), which can be missing on minimal systems. Package names differ between Ubuntu releases; this command is for the Ubuntu 24.04 build, not a way to make it compatible with older distributions.

KGC needs a graphical desktop or remote display supporting X11. For a Wayland desktop, install `xwayland` if it is absent. The runtime command does not install a full desktop environment. SDL sound support uses the system's audio devices/services; ALSA and PulseAudio client libraries are included above.

Mark the downloaded executable as runnable, then launch it. Replace the filename below with the one you downloaded:

```bash
chmod +x ./KGroundControl_vX.Y.Z_Ubuntu_24.04_x64
./KGroundControl_vX.Y.Z_Ubuntu_24.04_x64
```

Run KGC as your normal user. By default, it installs a copy to `~/.local/bin/KGroundControl` and creates an application-menu entry on first launch. This behavior can be disabled in the application settings.

Static Qt builds still depend on Linux system libraries and a compatible OpenSSL runtime. Use a package intended for your distribution; a newer Ubuntu binary may not run on an older Ubuntu installation. Deployment packages include `runtime-dependencies.txt` when available. To identify a missing linked library, run `ldd` against the downloaded executable.

For serial access on Ubuntu, your account may need membership in the `dialout` group:

```bash
sudo usermod -aG dialout "$USER"
```

Sign out and back in for group membership changes to take effect.

### Platforms and package verification

| Platform | Architecture | Availability |
| --- | --- | --- |
| Windows 11 | x86-64 | Use the matching published Windows asset. |
| Ubuntu | x86-64 | Choose an asset for the indicated distribution version; the current native build setup targets Ubuntu 24.04. |
| Raspberry Pi OS / Linux | ARM64 | Build from source with a matching ARM64 Qt SDK and toolchain when no compatible release asset is available. |

Release assets determine which prebuilt platforms are available. When a deployment package includes `SHA256SUMS`, you can verify its extracted files on Linux with `sha256sum -c SHA256SUMS` from the package directory.

## Development setup

**This section is only needed if you want to modify KGC or compile it yourself.** VS Code is the recommended starting point; Qt Creator and other CMake-compatible IDEs can also use the project.

### 1. Install development tools

| Platform | Tools |
| --- | --- |
| Ubuntu 24.04 | GCC/G++ with C++20 support, CMake 3.22+, Ninja, Git, and GDB for debugging. The Qt setup script installs the build dependencies; install VS Code and GDB separately if needed. |
| Windows | Visual Studio 2022 with **Desktop development with C++** and a Windows SDK, CMake 3.22+, Git, and VS Code. The supplied presets target MSVC x64. |

Clone the repository and open its root folder in VS Code:

```bash
git clone https://github.com/YevheniiKovryzhenko/KGroundControl.git
cd KGroundControl
code .
```

Install the workspace's recommended **CMake Tools** and **C/C++** extensions when prompted.

### 2. Set up Qt

Use a Qt SDK matching your compiler and processor architecture. The current Linux build has been verified with **static Qt 6.12.0**. Required modules include Widgets, Network, SerialPort, and Location.

For a native Ubuntu SDK, run the included installer from the repository root:

```bash
bash tools/ubuntu/install_Qt.sh
```

The script defaults to static Qt 6.12.0 under `~/Qt/Static/v6.12.0/install`, installs dependencies through `sudo`, and verifies the installed SDK. Building Qt can take substantial time and disk space. See the [Ubuntu Qt installer guide](tools/ubuntu/README.md) for options, retries, and shared builds; `bash tools/ubuntu/install_Qt.sh --help` lists its arguments.

On Windows, use the [Qt source-build script](tools/windows/install_Qt.ps1) or install a matching MSVC x64 SDK with the [Qt installer](https://doc.qt.io/qt-6/get-and-install-qt.html). The Windows script has its own Qt version and build-type settings; review those before running it.

CMake detects SDKs in the standard Qt installation folders, preferring static builds. Use an SDK matching the selected compiler and architecture; custom SDKs and toolchains can be selected with a [local preset](#custom-sdks-and-command-line-builds).

### 3. Configure, build, and debug

1. Run **CMake: Select Configure Preset** in VS Code, or click the configure-preset selector in the bottom status bar.
2. Choose a build profile from the table below. Only presets for your current operating system are shown.
3. Select the matching build preset if prompted, then use **Build** or **Ctrl+Shift+B**. The first configuration needs internet access to download external dependencies and MAVLink headers.
4. To debug, choose **Debug**, select `KGroundControl` as the CMake launch target, and use the CMake **Debug** button. F5 is also available through the active-preset launch entry for your operating system.

| Build profile | Use it for |
| --- | --- |
| **Debug** | Development and debugging, with debug symbols and AddressSanitizer on Linux. No deployment package is generated. |
| **Release (Speed Optimized)** | Speed-focused production builds with supported link-time optimization and unused-code removal. Generates a deployment package. |
| **Minimum Size Release** | Smaller executable code with size-focused optimization, supported link-time optimization, and unused-code removal. Generates a deployment package. |

Minimum Size Release targets executable size; it does not guarantee lower CPU time, heap RAM, or stack use. Both optimized profiles use the same installed Qt SDK. Choose between them using measurements from your workload.

If preset selectors are missing, reload VS Code and confirm that CMake Tools is enabled. After changing Qt SDKs, use **CMake: Delete Cache and Reconfigure**.

### Custom SDKs and command-line builds

For a custom Qt installation, create a Git-ignored `CMakeUserPresets.json` in the repository root. For example:

```json
{
  "version": 3,
  "configurePresets": [
    {
      "name": "local-debug",
      "inherits": "Debug (Linux)",
      "binaryDir": "${sourceDir}/build/local-debug",
      "cacheVariables": {
        "CMAKE_PREFIX_PATH": "/path/to/Qt/install"
      }
    }
  ],
  "buildPresets": [
    { "name": "local-debug", "configurePreset": "local-debug" }
  ]
}
```

On Windows, inherit from `Debug (Windows)` and use an MSVC-compatible SDK. The supplied presets discover Visual Studio 2022 x64 without requiring a developer shell. CMake also accepts explicit `Qt6_DIR` or the `QTDIR` environment variable for SDK selection; `QT_TARGET_VERSION` selects a version during automatic discovery. Clear the configuration cache when changing SDKs.

The same presets work from a terminal:

```bash
cmake --preset "Release (Linux)"
cmake --build --preset "Release (Linux)" --parallel 8
```

Use `MinSizeRel (Linux)` for **Minimum Size Release** or `Debug (Linux)` for **Debug**. Windows preset identifiers use `(Windows)` instead of `(Linux)`; the IDE display names omit the OS suffix.

For ARM64 or Raspberry Pi, use a matching native Qt SDK or a cross-compilation toolchain and target sysroot. The Ubuntu installer builds natively. See the [Qt Raspberry Pi cross-compilation documentation](https://wiki.qt.io/Cross-Compile_Qt_6_for_Raspberry_Pi) for target-specific setup, and keep local SDK/toolchain paths in a user preset.

## Build output and deployment

The executable is placed in `build/<platform>/<configuration>/bin`. Optimized builds also produce a versioned deployment folder:

```text
DistributionKit/<configuration>/<OS>/<Static-or-Shared>/v<version>/
```

Each package contains the executable, README, license, build information, and `SHA256SUMS`. Linux packages also include a runtime-dependency report. Linux deployment executables have symbols stripped; the build-tree executable retains its symbols for diagnosis. Packages are intended for compatible target systems, and static Qt does not eliminate every operating-system dependency.

To refresh a package without forcing a recompile, select the **deployment** build target in your IDE, or run:

```bash
cmake --build --preset "Release (Linux)" --target deployment
```

Use the corresponding preset for Minimum Size Release or Windows. A failed packaging run preserves the previous deployment package.

For shared Qt on Windows, packaging uses the selected SDK's `windeployqt`. MSVC runtime files are included when needed; keep the application and its dependencies compatible with the Qt SDK's compiler runtime. Set `KGC_OPENSSL_RUNTIME_DIR` if OpenSSL DLLs are installed outside the standard locations, or `KGC_DEPLOY_RUNTIME_FILES` for additional runtime files as a semicolon-separated list. Linux shared Qt builds require the matching Qt runtime and plugins on the target system.

Optional CMake settings include `ASAN=OFF` to disable Linux Debug AddressSanitizer, `KGC_ENABLE_IPO=OFF` to disable release link-time optimization, `FORCE_QT_STATIC=ON` to require a static Qt SDK, and `KGC_STRIP_DEPLOYMENT=OFF` to retain symbols in Linux/MinGW deployment copies.

Before publishing, verify `SHA256SUMS` and test the package on the target OS, including startup, HTTPS updates, and the device features you use.

### Publishing releases and updates

Use stable release tags in the form `v<major>.<minor>.<patch>`. Build from the tagged commit so the executable and deployment package use that version, then publish a GitHub release with the generated executable for each supported platform. Preserve the generated executable filenames; the updater uses them to identify the platform and architecture. Deployment archives can be attached alongside the executables.

CMake uses an exact stable tag on the current commit, or the highest stable tag reachable from the current branch for an untagged development build. Fetch tags when cloning an existing release history. Source archives without Git metadata require an explicit `KGC_VERSION` CMake setting. Reconfigure after creating the release tag and confirm the executable's `--version` before publishing; use a new patch tag rather than replacing a published version.

Platforms can be published independently, either on the same release or different releases. The update feed selects the highest stable version with a matching executable for each platform and architecture, regardless of release publication order. Drafts and prereleases are excluded. Users retain the startup-check preference and the prompts to download and install an update.

GitHub Actions runs `tools/release/update_feed.py` automatically when releases are published or edited, and checks periodically for assets uploaded later. **No local Python command or manual version edit is required.** The script's `--self-test` examples are synthetic test data; production versions come from GitHub release tags. Feed publishing does not create commits on development or release branches.

Repository administrators must configure **Settings → Pages → Source → GitHub Actions** once and allow the `github-pages` environment to deploy from the default branch and release tags. The **Publish platform update feed** workflow can also be run manually to refresh the feed immediately after a delayed asset upload.

## Support and contributions

Use [GitHub Issues](https://github.com/YevheniiKovryzhenko/KGroundControl/issues) for bug reports and feature requests. Include your KGC version, operating system, architecture, connection type, and steps to reproduce the issue. For build problems, include the selected preset, Qt version, and relevant CMake or compiler output.

For other questions, contact Yevhenii (Jack) Kovryzhenko at [yzk0058@auburn.edu](mailto:yzk0058@auburn.edu).

## License

KGroundControl is licensed under the [GNU Affero General Public License v3.0](LICENSE).

## Credit
This work started during my Ph.D. at [ACELAB](https://etaheri0.wixsite.com/acelabauburnuni) at Auburn University, under the supervision of Dr. Ehsan Taheri. Part of this work has been used during my participation in the STTR Phase II project with Transcend Air Co. to support the ground station and related activities. For more details, check out my [PX4 Simulink IO Framework](https://github.com/YevheniiKovryzhenko/PX4_SIMULINK_IO_Framework.git) and [RRTV TiltWing](https://github.com/YevheniiKovryzhenko/RRTV_TiltWing.git) repositories that were all part of this project.

I am still in the process of publishing journal papers that have directly used this work, so I will keep this section actively updated. Feel free to credit [me](https://scholar.google.com/citations?user=P812qiUAAAAJ&hl=en) by citing any of my relevant works.

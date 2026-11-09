# Native Qt SDK for Windows

Run the installer from PowerShell in the directory containing the script, as your normal user. With no arguments it builds a native x64 static release SDK from the Qt 6 release pinned in the script. The default SDK layout is `C:\Qt\Static\v<version>\install`.

```powershell
$installer = '.\install.ps1' # Use '.\install_Qt.ps1' in KGroundControl.
& $installer
# For unattended use, after reviewing Qt's open-source license terms:
& $installer -AcceptLicense
# Use another writable SDK root:
& $installer -Root D:\Qt
```

If PowerShell reports that running scripts is disabled, launch the installer in a separate PowerShell process with a temporary execution policy.

```powershell
powershell.exe -NoProfile -ExecutionPolicy RemoteSigned -File $installer
```

Use `Get-ExecutionPolicy -List` if the error persists. A policy set by Group Policy takes precedence over the process setting.

Use `-Version` to select a different Qt 6 release tag. The installer uses a separate directory for each version and prints the exact install path.

## Dependencies and build

The installer checks for Git, CMake 3.25+, Ninja, Python 3, and Visual Studio 2022 with the x64 C++ toolchain, ATL, and Windows SDK 10.0.22000.0 or newer. Older SDKs lack the rounded-corner declarations used by Qt's modern Windows style. When the required SDK is missing, setup adds Windows 11 SDK 10.0.26100.0 to the Visual Studio installation. The script reuses compatible tools and installs missing packages through WinGet, which comes with Microsoft's App Installer. Dependency installers run in silent mode, though Windows may ask for administrator approval. If setup requires a reboot, rerun the script afterward. The installer loads the x64 developer environment for the duration of the build.

Choose a short, writable local drive path without spaces and allow enough disk space for the source, build, and installed SDK. The build defaults to one parallel job per available logical CPU. Set `-Jobs 6` to limit parallelism if memory is tight.

The installer checks out the selected tag and builds the release modules supported by Windows and the available dependencies. The default build excludes WebEngine, its bundled Qt PDF sources, examples, and tests. Static Qt uses the dynamic MSVC runtime by default. After installation, the script builds and runs a consumer using Core, Gui, Widgets, Network, SerialPort, and Location. It also checks that the Windows platform plugin and Schannel TLS initialize. A successful run prints `SDK installed and consumer verified`.

Use `-Shared` for a separate shared release SDK, and `-Shared -WithWebEngine` to include WebEngine and PDF sources after installing their additional build dependencies. Pass extra Qt options as an array, for example `-ConfigureArgs @('-openssl-runtime', '--', '-DOPENSSL_ROOT_DIR=C:/OpenSSL')` with compatible OpenSSL headers installed. Inspect `build/config.summary` and `installed-modules.txt` for the optional modules and features that built.

## Retries

After verification, an interactive run asks separately whether to delete `build` and `src`. Press Enter to delete or enter `n` to keep a directory. Unattended runs, including runs with `-AcceptLicense`, delete both after verification. A failed install keeps both. Cleanup preserves the installed SDK, logs, and module inventory.

Rerun with the same settings to resume an interrupted build. The installer keeps source and build files, records build inputs, and prevents concurrent runs against the same SDK. It keeps logs and a submodule commit inventory. If inputs change before an install exists, the installer renames its old build directory and configures a fresh one from the existing checkout. If an install already exists, use a fresh root or move both build and install aside before changing inputs. `-SkipClone` reuses an already initialized checkout without fetching. `-SkipPrompts` requires `-AcceptLicense`.

## Use the SDK in a project

Select the SDK from a VS 2022 x64 developer shell when configuring a fresh Ninja build directory in another project.

```powershell
$sdk = Read-Host 'Paste the SDK install path printed by the installer'
cmake -S . -B build/qt-static -G Ninja -DCMAKE_BUILD_TYPE=Release "-DCMAKE_PREFIX_PATH=$sdk"
cmake --build build/qt-static --parallel
```

- [Qt for Windows build prerequisites](https://doc.qt.io/qt-6/windows-building.html)
- [Qt configure options](https://doc.qt.io/qt-6/configure-options.html)
- [Qt WebEngine build prerequisites](https://doc.qt.io/qt-6/qtwebengine-platform-notes.html)
- [WinGet and App Installer](https://learn.microsoft.com/en-us/windows/package-manager/winget/)

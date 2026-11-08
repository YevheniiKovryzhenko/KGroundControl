# Validate KGroundControl with static Qt on Windows

Run the Qt installer from PowerShell at the repository root. It reuses existing
build tools and uses WinGet to install missing ones, including VS 2022 Build
Tools with MSVC x64, ATL, and a Windows SDK. Windows may ask for administrator
approval during dependency setup. The script defaults to a static release
SDK from the Qt 6 version pinned in the script and verifies the Qt modules
required by KGroundControl.

```powershell
.\tools\windows\install_Qt.ps1
```

If PowerShell reports that running scripts is disabled, launch the installer in
a separate PowerShell process with a temporary execution policy.

```powershell
powershell.exe -NoProfile -ExecutionPolicy RemoteSigned -File ".\tools\windows\install_Qt.ps1"
```

Use `Get-ExecutionPolicy -List` if the error persists. A policy set by Group
Policy takes precedence over the process setting.

After verification, interactive runs offer to delete `build` and `src`
separately. Press Enter to delete or enter `n` to keep a directory.
Unattended runs delete both after verification. Failed installs keep both.

The Qt installer uses all available logical CPUs by default. Pass `-Jobs 6`
to limit parallelism.

Configure and build KGroundControl using its Windows Release preset, an
explicit SDK path, and a separate build directory. The configure step can
download project dependencies.

```powershell
$sdk = Read-Host 'Paste the SDK install path printed by the installer'
cmake --preset 'Release (Windows)' -B build/windows/qt-static-check -DFORCE_QT_STATIC=ON "-DCMAKE_PREFIX_PATH=$sdk"
cmake --build build/windows/qt-static-check --config Release --target KGroundControl --parallel
```

The configure output should show the selected Qt version and `STATIC_LIBRARY`.

- [Qt for Windows build prerequisites](https://doc.qt.io/qt-6/windows-building.html)

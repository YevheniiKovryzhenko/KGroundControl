# PowerShell script to install Qt from source for static (release) build on Windows
#
# Parameters:
#   -SkipPrompts   : Skip all user prompts (useful for automated builds)
#   -SkipClone    : Skip cloning and initialization (assumes source already exists)

param(
    [switch]$SkipPrompts,
    [switch]$SkipClone
)

# Helper: prompt a yes/no question with immediate single-keypress response (y/Y or n/N).
# Default is used when -SkipPrompts is set or when Enter is pressed without a key.
# Returns $true for yes, $false for no.
function Read-YesNo {
    param(
        [string]$Prompt,
        [bool]$Default = $false
    )
    if ($SkipPrompts) { return $Default }
    $defaultHint = if ($Default) { "[Y/n]" } else { "[y/N]" }
    Write-Host -NoNewline "$Prompt $defaultHint "
    while ($true) {
        $key = $host.UI.RawUI.ReadKey("NoEcho,IncludeKeyDown")
        $ch  = $key.Character.ToString().ToLower()
        if ($ch -eq 'y') { Write-Host "y"; return $true  }
        if ($ch -eq 'n') { Write-Host "n"; return $false }
        # Enter key (char 13) → use default
        if ($key.VirtualKeyCode -eq 13) {
            Write-Host $(if ($Default) { "y" } else { "n" })
            return $Default
        }
    }
}

# Ensure scripts can run by checking and setting execution policy if needed
$executionPolicy = Get-ExecutionPolicy -Scope CurrentUser
if ($executionPolicy -eq "Restricted") {
    Write-Host "Setting execution policy to RemoteSigned for current user..."
    Set-ExecutionPolicy -ExecutionPolicy RemoteSigned -Scope CurrentUser -Force
}

# Capture the original working directory to restore at the end
$OriginalLocation = Get-Location

# Helper function to check for missing executables and add their install directory to PATH
function Add-ExecutableToPath {
    param(
        [string]$ExeName,
        [string[]]$CommonDirs
    )
    if (-not (Get-Command $ExeName -ErrorAction SilentlyContinue)) {
        foreach ($dir in $CommonDirs) {
            $exePath = Get-ChildItem -Path $dir -Filter $ExeName -ErrorAction SilentlyContinue | Select-Object -First 1
            if ($exePath) {
                $env:Path += ";$($exePath.DirectoryName)"
                Write-Host "Added $ExeName to PATH: $($exePath.DirectoryName)"
                break
            }
        }
        if (-not (Get-Command $ExeName -ErrorAction SilentlyContinue)) {
            Write-Host "WARNING: $ExeName not found in PATH or common install locations."
        }
    }
}

# Function to restore original location and exit
function Exit-WithCleanup {
    param([int]$ExitCode = 0)
    Set-Location $OriginalLocation
    exit $ExitCode
}

# Set up trap to ensure directory is restored on any error or interruption
trap {
    Write-Host "Script interrupted or error occurred. Returning to original directory..."
    Write-Host "Error details: $_"
    Set-Location $OriginalLocation
    break
}

#--------------------------------------------------------------
# Step 0: Check if running as administrator
#--------------------------------------------------------------
if (([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    Write-Host "ERROR: This script should not be run as an administrator. Please run it as a normal user."
    Exit-WithCleanup 1
}

#--------------------------------------------------------------
# Step 0b: Upfront questions — gather all decisions before any long work begins
#--------------------------------------------------------------
# Hardcoded parameters (adjust here):
$QtVersion = "v6.10.2"           # Tag/branch to use
$BuildType = "Static"           # Options: "Static" (default) or "Shared"

switch ($BuildType) {
  'Static' { $qtSubdir = 'Qt\Static'; $configureFlags = '-static -release' }
  'Shared' { $qtSubdir = 'Qt\Shared'; $configureFlags = '-debug' }
  default  { Write-Host "ERROR: Unknown BuildType='$BuildType'. Use 'Static' or 'Shared'."; Exit-WithCleanup 1 }
}
$QtRootDir = "C:\$qtSubdir\$QtVersion"

Write-Host ""
Write-Host "==============================================="
Write-Host " Qt $QtVersion $BuildType build"
Write-Host " Install path: $QtRootDir"
Write-Host "==============================================="

# Decision: reclone if the root dir already exists
$doReclone = $false
if (-not $SkipClone) {
    if (Test-Path $QtRootDir) {
        $doReclone = Read-YesNo "$QtRootDir already exists. Delete and re-clone?" -Default $false
        if ($doReclone) {
            Write-Host "Will delete $QtRootDir and clone fresh."
        } else {
            Write-Host "Will keep existing source."
        }
    }
}

# Decision: delete build dir automatically after successful install (saves disk space)
$deleteBuildAfter = Read-YesNo "Delete build directory automatically after successful install?" -Default $true

# Decision: delete src dir after successful install (can always re-clone later)
$deleteSrcAfter = Read-YesNo "Delete source directory after successful install? (keep by default)" -Default $false

Write-Host ""

#--------------------------------------------------------------
# Step 1: Install Required Packages
#--------------------------------------------------------------
Write-Host "==============================================="
Write-Host "Installing required packages..."
Write-Host "==============================================="

# Check if winget is available
if (Get-Command winget -ErrorAction SilentlyContinue) {
    # Add GnuWin32 bin to PATH if exists
    $gnuWin32Bin = "C:\Program Files (x86)\GnuWin32\bin"
    if (Test-Path $gnuWin32Bin) {
        $env:Path += ";$gnuWin32Bin"
    }
    # Check and install CMake
    if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
        winget install --id Kitware.CMake --accept-source-agreements --accept-package-agreements
        $env:Path = [System.Environment]::GetEnvironmentVariable("Path", "Machine") + ";" + [System.Environment]::GetEnvironmentVariable("Path", "User")
        Add-ExecutableToPath "cmake.exe" @("C:\Program Files\CMake\bin", "C:\Program Files (x86)\CMake\bin")
    } else {
        Write-Host "CMake already installed"
    }
    
    # Check and install Git
    if (-not (Get-Command git -ErrorAction SilentlyContinue)) {
        winget install --id Git.Git --accept-source-agreements --accept-package-agreements
        $env:Path = [System.Environment]::GetEnvironmentVariable("Path", "Machine") + ";" + [System.Environment]::GetEnvironmentVariable("Path", "User")
        Add-ExecutableToPath "git.exe" @("C:\Program Files\Git\bin", "C:\Program Files (x86)\Git\bin")
    } else {
        Write-Host "Git already installed"
    }
    
    # Check and install Python - must validate it's real Python, not Windows App Store stub
    $pythonValid = $false
    
    # First, check standard installation directories directly (doesn't rely on PATH)
    $pythonDirectories = @(
        "C:\Users\$env:USERNAME\AppData\Local\Programs\Python",
        "C:\Python*",
        "C:\Program Files\Python*"
    )
    
    foreach ($searchDir in $pythonDirectories) {
        if (Test-Path $searchDir) {
            $pythonExes = @(Get-ChildItem -Path "$searchDir\python.exe" -Recurse -ErrorAction SilentlyContinue | Select-Object -First 5)
            if ($pythonExes.Count -gt 0) {
                foreach ($pyExe in $pythonExes) {
                    try {
                        $pythonPath = $pyExe.FullName
                        $output = & $pythonPath --version 2>&1
                        if ($output -match "Python") {
                            Write-Host "Found valid Python: $pythonPath"
                            $pythonValid = $true
                            break
                        }
                    } catch {
                        # Continue to next Python
                    }
                }
                if ($pythonValid) { break }
            }
        }
    }
    
    # If no valid Python found in directories, check PATH as fallback
    if (-not $pythonValid) {
        $pythonCmd = Get-Command python -ErrorAction SilentlyContinue
        if ($pythonCmd) {
            $pythonPath = $pythonCmd.Source
            # Reject Windows App Store stub (located in WindowsApps)
            if ($pythonPath -notlike "*WindowsApps*") {
                try {
                    $output = & python --version 2>&1
                    if ($output -match "Python") {
                        $pythonValid = $true
                        Write-Host "Found valid Python in PATH: $pythonPath"
                    }
                } catch {
                    Write-Host "Python command in PATH failed validation, will reinstall"
                }
            }
        }
    }
    
    if (-not $pythonValid) {
        Write-Host "Installing real Python via winget..."
        $wingetSuccess = $false
        
        # Try multiple Python package IDs in winget
        $pythonPackageIds = @("Python.Python.3", "Python.Python.3.12", "Python.Python.3.11")
        foreach ($packageId in $pythonPackageIds) {
            Write-Host "Trying package: $packageId"
            winget install --id $packageId --accept-source-agreements --accept-package-agreements 2>&1
            if ($LASTEXITCODE -eq 0) {
                $wingetSuccess = $true
                break
            }
        }
        
        if ($wingetSuccess) {
            $env:Path = [System.Environment]::GetEnvironmentVariable("Path", "Machine") + ";" + [System.Environment]::GetEnvironmentVariable("Path", "User")
            Add-ExecutableToPath "python.exe" @("C:\Python39", "C:\Python310", "C:\Users\$env:USERNAME\AppData\Local\Programs\Python\Python39", "C:\Users\$env:USERNAME\AppData\Local\Programs\Python\Python310")
            Start-Sleep -Seconds 2
        } else {
            Write-Host "Warning: winget Python installation failed. Will attempt direct download from python.org..."
            # Fallback: Try to download Python directly if winget fails
            $pythonDownloadUrl = "https://www.python.org/ftp/python/3.12.0/python-3.12.0-amd64.exe"
            $pythonInstaller = "$env:TEMP\python-3.12.0-amd64.exe"
            Write-Host "Downloading Python from $pythonDownloadUrl..."
            try {
                $ProgressPreference = 'SilentlyContinue'
                Invoke-WebRequest -Uri $pythonDownloadUrl -OutFile $pythonInstaller
                Write-Host "Installing Python..."
                & $pythonInstaller /quiet PrependPath=1
                $env:Path = [System.Environment]::GetEnvironmentVariable("Path", "Machine") + ";" + [System.Environment]::GetEnvironmentVariable("Path", "User")
                Start-Sleep -Seconds 3
                Remove-Item $pythonInstaller -Force -ErrorAction SilentlyContinue
            } catch {
                Write-Host "Warning: Direct Python download also failed. Python will not be available."
            }
        }
    }
    
    # Check and install Bison
    if (-not (Get-Command bison -ErrorAction SilentlyContinue)) {
        winget install --id GnuWin32.Bison --accept-source-agreements --accept-package-agreements
        $env:Path = [System.Environment]::GetEnvironmentVariable("Path", "Machine") + ";" + [System.Environment]::GetEnvironmentVariable("Path", "User")
        Add-ExecutableToPath "bison.exe" @("C:\Program Files (x86)\GnuWin32\bin")
    } else {
        Write-Host "Bison already installed"
    }
    
    # Check and install Flex
    if (-not (Get-Command flex -ErrorAction SilentlyContinue)) {
        winget install --id GnuWin32.Flex --accept-source-agreements --accept-package-agreements
        $env:Path = [System.Environment]::GetEnvironmentVariable("Path", "Machine") + ";" + [System.Environment]::GetEnvironmentVariable("Path", "User")
        Add-ExecutableToPath "flex.exe" @("C:\Program Files (x86)\GnuWin32\bin")
    } else {
        Write-Host "Flex already installed"
    }
    
    # Check and install Ninja
    if (-not (Get-Command ninja -ErrorAction SilentlyContinue)) {
        winget install --id Ninja-build.Ninja --accept-source-agreements --accept-package-agreements
        $env:Path = [System.Environment]::GetEnvironmentVariable("Path", "Machine") + ";" + [System.Environment]::GetEnvironmentVariable("Path", "User")
        # Add Ninja path explicitly to override pyenv
        $env:PATH = "C:\Program Files\Ninja\bin;" + $env:PATH
        Add-ExecutableToPath "ninja.exe" @("C:\Program Files\Ninja\bin", "C:\Program Files (x86)\Ninja\bin")
    } else {
        Write-Host "Ninja already installed"
    }
    
    # Check and install Visual Studio Build Tools if compiler not found
    if (-not (Get-Command cl -ErrorAction SilentlyContinue)) {
        $setupExePath = "C:\Program Files (x86)\Microsoft Visual Studio\Installer\vs_installer.exe"
        if (Test-Path $setupExePath) {
            Write-Host "Visual Studio Installer found. Installing Desktop C++ workload and required components..."
            Start-Process -FilePath $setupExePath -ArgumentList "modify --installPath `"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools`" --add Microsoft.VisualStudio.Workload.NativeDesktop --add Microsoft.VisualStudio.Component.VC.Tools.x86.x64 --add Microsoft.VisualStudio.Component.Windows10SDK.19041 --passive --norestart" -Verb RunAs -Wait
            # Wait a moment for installer to finish cleanup
            Start-Sleep -Seconds 5
        } else {
            Write-Host "Installing Visual Studio Build Tools with Desktop C++ workload and required components..."
            winget install --id Microsoft.VisualStudio.2022.BuildTools --override "--add Microsoft.VisualStudio.Workload.NativeDesktop --add Microsoft.VisualStudio.Component.VC.Tools.x86.x64 --add Microsoft.VisualStudio.Component.Windows10SDK.19041 --passive --norestart" --accept-source-agreements --accept-package-agreements
            Start-Sleep -Seconds 5
        }
        # Always check for cl.exe in common locations and add to PATH if found
        $clPath = Get-ChildItem -Path "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools" -Recurse -Filter cl.exe -ErrorAction SilentlyContinue | Select-Object -First 1
        if (-not $clPath) {
            $clPath = Get-ChildItem -Path "C:\Program Files\Microsoft Visual Studio\2022\BuildTools" -Recurse -Filter cl.exe -ErrorAction SilentlyContinue | Select-Object -First 1
        }
            # Also check other common Visual Studio editions
            if (-not $clPath) {
                $vsEditions = @("Community", "Professional", "Enterprise")
                foreach ($edition in $vsEditions) {
                    $clPath = Get-ChildItem -Path "C:\Program Files\Microsoft Visual Studio\2022\$edition" -Recurse -Filter cl.exe -ErrorAction SilentlyContinue | Select-Object -First 1
                    if ($clPath) { break }
                }
            }
        if ($clPath) {
            $clDir = Split-Path $clPath.FullName
            if ($env:Path -notlike "*$clDir*") {
                $env:Path += ";$clDir"
                Write-Host "Added cl.exe directory to PATH: $clDir"
                    # Persistently add to user PATH
                    $currentUserPath = [System.Environment]::GetEnvironmentVariable("Path", "User")
                    if ($currentUserPath -notlike "*$clDir*") {
                        [Environment]::SetEnvironmentVariable("Path", "$currentUserPath;$clDir", "User")
                        Write-Host "Persistently added cl.exe directory to user PATH. Restart your terminal to apply changes."
                    }
            }
        } else {
            Write-Host "WARNING: cl.exe not found immediately after install. It may still be installing in the background."
            Write-Host "Please wait 1-2 minutes and restart your terminal, or manually open Visual Studio Installer to verify Desktop C++ workload was added."
        }
    } else {
        Write-Host "Visual Studio compiler found"
    }
} else {
    Write-Host "ERROR: winget not found. Please install winget first from https://github.com/microsoft/winget-cli/releases"
    Exit-WithCleanup 1
}

#--------------------------------------------------------------
# Step 2: Setup Qt Directory and Clone Source
#--------------------------------------------------------------
Write-Host "==============================================="
Write-Host "Setting up Qt $QtVersion installation directory in $QtRootDir..."
Write-Host "==============================================="

if ($SkipClone) {
    Write-Host "Skipping clone - using existing source at $QtRootDir\src"
    # Ensure root dir exists even when skipping clone (path may differ from old install)
    if (-not (Test-Path $QtRootDir)) {
        New-Item -ItemType Directory -Path $QtRootDir -Force | Out-Null
        Write-Host "Created directory: $QtRootDir"
    }
    if (-not (Test-Path "$QtRootDir\src")) {
        Write-Host "ERROR: Source directory not found at $QtRootDir\src. Cannot use -SkipClone without existing source."
        Exit-WithCleanup 1
    }
} elseif ($doReclone) {
    Write-Host "Deleting $QtRootDir..."
    cmd /c "rmdir /s /q `"$QtRootDir`"" 2>&1 | Out-Null
    Start-Sleep -Milliseconds 500
    New-Item -ItemType Directory -Path $QtRootDir -Force | Out-Null
    Write-Host "Cloning Qt into $QtRootDir\src"
    git clone https://code.qt.io/qt/qt5.git "$QtRootDir\src"
    if ($LASTEXITCODE -ne 0) { Write-Host "ERROR: Git clone failed."; Exit-WithCleanup 1 }
    Set-Location "$QtRootDir\src"
    git checkout $QtVersion
    if ($LASTEXITCODE -ne 0) { Write-Host "ERROR: Git checkout failed."; Exit-WithCleanup 1 }
    & "C:\Program Files\Git\bin\bash.exe" .\init-repository --module-subset=default,-qtwebengine
    if ($LASTEXITCODE -ne 0) { Write-Host "ERROR: Init repository failed."; Exit-WithCleanup 1 }
    Set-Location $QtRootDir
} elseif (-not (Test-Path $QtRootDir)) {
    New-Item -ItemType Directory -Path $QtRootDir -Force
    Write-Host "Created directory: $QtRootDir"
    Write-Host "Cloning Qt into $QtRootDir\src"
    git clone https://code.qt.io/qt/qt5.git "$QtRootDir\src"
    if ($LASTEXITCODE -ne 0) { Write-Host "ERROR: Git clone failed."; Exit-WithCleanup 1 }
    Set-Location "$QtRootDir\src"
    git checkout $QtVersion
    if ($LASTEXITCODE -ne 0) { Write-Host "ERROR: Git checkout failed."; Exit-WithCleanup 1 }
    & "C:\Program Files\Git\bin\bash.exe" .\init-repository --module-subset=default,-qtwebengine
    if ($LASTEXITCODE -ne 0) { Write-Host "ERROR: Init repository failed."; Exit-WithCleanup 1 }
    Set-Location $QtRootDir
} else {
    Write-Host "Using existing source at $QtRootDir\src"
}

#--------------------------------------------------------------
# Step 3: Clean Existing Build Files and Start Build Process
#--------------------------------------------------------------
Set-Location $QtRootDir

# Only clean build files if not skipping clone (i.e., doing fresh build)
if ($SkipClone) {
    Write-Host "Skipping build/install folder cleanup - continuing from previous build..."
} else {
    # Clean build directory and install directory for fresh build
    if (Test-Path "$QtRootDir\build") {
        Write-Host "==============================================="
        Write-Host "Cleaning existing build directory..."
        Write-Host "==============================================="
        cmd /c "rmdir /s /q `"$QtRootDir\build`"" 2>&1 | Out-Null
    }

    if (Test-Path "$QtRootDir\install") {
        Write-Host "==============================================="
        Write-Host "Deleting existing install folder..."
        Write-Host "==============================================="
        cmd /c "rmdir /s /q `"$QtRootDir\install`"" 2>&1 | Out-Null
    }
}

# Step 4: Configure the Build
Write-Host "==============================================="
Write-Host "Configuring build settings..."
Write-Host "==============================================="

# Always clean build folder before configuring to prevent stale PCH (precompiled header) files
# This is necessary even when resuming failed builds, as stale PCH files cause compilation failures
$BuildDir = "$QtRootDir\build"
if (Test-Path $BuildDir) {
    Write-Host "Cleaning existing build directory to prevent stale precompiled headers..."
    cmd /c "rmdir /s /q `"$BuildDir`"" 2>&1 | Out-Null
    Start-Sleep -Milliseconds 500
}

# Create fresh build directory
if (-not (Test-Path $BuildDir)) {
    New-Item -ItemType Directory -Path $BuildDir -Force
}
Set-Location $BuildDir

# Check for configure.bat in the source directory before running configure
if (-not (Test-Path "$QtRootDir\src\configure.bat")) {
    Write-Host "ERROR: Qt source directory or configure.bat is missing!"
    Write-Host "Please ensure the repository is properly cloned and initialized at $QtRootDir\src."
    Write-Host "You may need to delete $QtRootDir and allow the script to re-clone."
    Exit-WithCleanup 1
}

# Locate Visual Studio vcvarsall.bat dynamically
$vcVarsCandidates = @(
    'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat',
    'C:\Program Files\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat',
    'C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat',
    'C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvarsall.bat',
    'C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvarsall.bat',
    'C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvarsall.bat',
    'C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvarsall.bat',
    'C:\Program Files\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvarsall.bat',
    'C:\Program Files\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvarsall.bat'
)
$vcVars = $vcVarsCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $vcVars) {
    Write-Host "ERROR: Could not find vcvarsall.bat. Ensure Visual Studio Build Tools (or VS with C++ workload) is installed."
    Exit-WithCleanup 1
}

# Determine Ninja path if available for CMake
$NinjaExe = (Get-Command ninja -ErrorAction SilentlyContinue).Source
if (-not $NinjaExe) {
    foreach ($p in @('C:\Program Files\Ninja\bin\ninja.exe','C:\Program Files (x86)\Ninja\bin\ninja.exe')) {
        if (Test-Path $p) { $NinjaExe = $p; break }
    }
}
$cmakeMakeArg = ''
if ($NinjaExe) { $cmakeMakeArg = " -- -DCMAKE_MAKE_PROGRAM=`"$NinjaExe`"" }

# Clean PATH and configure Qt with Visual Studio environment
$env:Path = $env:Path -replace 'C:\\Users\\[^;]*\\.pyenv[^;]*;?', ''
$env:Path = $env:Path -replace 'C:\\Qt[^;]*;?', ''

# Explicitly detect Python and add to temp environment for configure
$pythonExe = $null

# Search for real Python installations in standard locations (NOT from PATH which can be a stub)
$pythonSearchDirs = @(
    "C:\Users\$env:USERNAME\AppData\Local\Programs\Python",
    "C:\Python*",
    "C:\Program Files\Python*"
)

foreach ($dir in $pythonSearchDirs) {
    if (Test-Path $dir) {
        $pythonFiles = @(Get-ChildItem -Path "$dir\python.exe" -Recurse -ErrorAction SilentlyContinue)
        if ($pythonFiles.Count -gt 0) {
            # Use the first valid Python found
            foreach ($py in $pythonFiles) {
                $version = & "$($py.FullName)" --version 2>&1
                if ($version -match "Python" -and $version -notmatch "stub") {
                    $pythonExe = $py.FullName
                    Write-Host "Found real Python: $pythonExe ($version)"
                    break
                }
            }
            if ($pythonExe) { break }
        }
    }
}

# If still not found, skip qtdeclarative (requires Python)
if (-not $pythonExe) {
    Write-Host "WARNING: No real Python installation found at standard locations."
    Write-Host "qtdeclarative requires Python and will be skipped."
}

# Use cmd to ensure proper Visual Studio environment setup and configure from build directory
$pythonArg = ""
# Skip modules with genuine build failures on Windows static builds:
# - qtwebengine: requires Chromium, too complex
# - qtwayland: Linux-only compositor protocol
# - qtspeech: optional TTS module
# - qtconnectivity: WinRT GUID/winrt::guid type mismatch in Qt 6.10.2 with Windows SDK 10.0.19041+
#   (qlowenergycontroller_winrt.cpp: cannot convert GUID to winrt::guid)
$skipModules = "qtwebengine,qtwayland,qtspeech,qtconnectivity"

if ($pythonExe) {
    $pythonArg = " -DPython_EXECUTABLE=`"$pythonExe`""
    Write-Host "CMake will use Python: $pythonExe"
} else {
    Write-Host "WARNING: Python not found. qtdeclarative and dependent modules will be skipped."
    $skipModules += ",qtdeclarative,qtquicktimeline,qtquick3d,qtquickeffectmaker,qtquick3dphysics,qtgraphs,qtlottie,qtscxml,qtremoteobjects"
}

cmd /c "`"$vcVars`" x64 && cd /d `"$BuildDir`" && `"$QtRootDir\src\configure.bat`" $configureFlags -opensource -prefix `"$QtRootDir\install`" -nomake examples -nomake tests -no-feature-accessibility -skip $skipModules$cmakeMakeArg$pythonArg"

if ($LASTEXITCODE -ne 0) {
    Write-Host "ERROR: Qt configure failed."
    Exit-WithCleanup 1
}

#--------------------------------------------------------------
# Step 5: Compile Qt
#--------------------------------------------------------------
Write-Host "==============================================="
Write-Host "Compiling $QtVersion..."
Write-Host "==============================================="
# Build from the dedicated build directory
Set-Location $BuildDir
cmd /c "`"$vcVars`" x64 && cd /d `"$BuildDir`" && cmake --build . --parallel"

if ($LASTEXITCODE -ne 0) {
    Write-Host "ERROR: Qt compilation failed."
    Exit-WithCleanup 1
}

#--------------------------------------------------------------
# Step 6: Install Qt
#--------------------------------------------------------------
Write-Host "==============================================="
Write-Host "Installing $QtVersion..."
Write-Host "==============================================="
# Install from the dedicated build directory where build files are located
$BuildDir = "$QtRootDir\build"
Set-Location $BuildDir
cmd /c "`"$vcVars`" x64 && cd /d `"$BuildDir`" && cmake --install ."

if ($LASTEXITCODE -ne 0) {
    Write-Host "ERROR: Qt installation failed."
    Exit-WithCleanup 1
}

#--------------------------------------------------------------
# Final Step: Completion Message and Cleanup
#--------------------------------------------------------------
Write-Host "==============================================="
Write-Host "Qt $QtVersion installation and setup completed successfully!"
Write-Host "Installed to: $QtRootDir\install"
Write-Host "==============================================="

# Delete build directory (large, not needed after install)
if ($deleteBuildAfter) {
    if (Test-Path "$QtRootDir\build") {
        Write-Host "Deleting build directory to free disk space..."
        cmd /c "rmdir /s /q `"$QtRootDir\build`"" 2>&1 | Out-Null
        Write-Host "Build directory deleted."
    }
}

# Optionally delete source directory
if ($deleteSrcAfter) {
    if (Test-Path "$QtRootDir\src") {
        Write-Host "Deleting source directory..."
        cmd /c "rmdir /s /q `"$QtRootDir\src`"" 2>&1 | Out-Null
        Write-Host "Source directory deleted."
    }
}

# Restore original working directory and exit cleanly
Exit-WithCleanup 0

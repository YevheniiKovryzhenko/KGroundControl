<#
.SYNOPSIS
Builds a native MSVC x64 Qt SDK from a release tag.
.DESCRIPTION
Installs missing build tools using WinGet. The default is Qt 6.12.0 static
release. After verification, interactive runs ask whether to remove build
and source separately. Press Enter to delete or enter N to keep a directory.
Unattended runs remove both after verification.
See the Windows Qt README for prerequisites and consumer build commands.
.PARAMETER AcceptLicense
Passes -confirm-license to Qt and skips the interactive cleanup prompts.
.PARAMETER SkipPrompts
Requires explicit license acceptance for unattended use.
.PARAMETER SkipClone
Uses a previously initialized source checkout without fetching submodules.
.PARAMETER Jobs
Parallel build jobs. Defaults to the available logical processor count.
.PARAMETER ConfigureArgs
Additional Qt configure arguments as an array, including '--' before CMake options.
#>
[CmdletBinding()]
param(
    [ValidatePattern('^v?6\.[0-9]+\.[0-9]+$')]
    [string]$Version = '6.12.0',
    [string]$Root = 'C:\Qt',
    [ValidateRange(1, 1024)]
    [int]$Jobs = [Environment]::ProcessorCount,
    [switch]$Shared,
    [switch]$WithWebEngine,
    [switch]$AcceptLicense,
    [switch]$SkipPrompts,
    [switch]$SkipClone,
    [string[]]$ConfigureArgs = @()
)

$ErrorActionPreference = 'Stop'

function Invoke-Checked {
    param([string]$Executable, [string[]]$Arguments)
    & $Executable @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "$Executable failed with exit code $LASTEXITCODE."
    }
}

function Test-SdkCleanupPath {
    param(
        [string]$SdkDir,
        [ValidateSet('build', 'src')]
        [string]$Directory
    )
    $target = Join-Path $SdkDir $Directory
    try {
        $item = Get-Item -LiteralPath $target -Force
        if (-not $item.PSIsContainer -or $item.FullName -ine $target -or
            $item.Parent.FullName -ine $SdkDir) {
            throw 'The cleanup target is not the expected SDK subdirectory.'
        }
        for ($parent = $item; $parent; $parent = $parent.Parent) {
            if ($parent.Attributes -band [IO.FileAttributes]::ReparsePoint) {
                throw "Cleanup path contains a reparse point: $($parent.FullName)"
            }
        }
        if (Get-ChildItem -LiteralPath $target -Force -Recurse -Attributes ReparsePoint | Select-Object -First 1) {
            throw 'The cleanup directory contains a reparse point.'
        }
        return $true
    } catch {
        Write-Warning "Keeping ${target}: $_"
        return $false
    }
}

# Keep the developer environment local to this installer invocation.
$savedEnvironment = @{}
function Set-BuildEnvironment {
    param([string]$Name, [string]$Value)
    if (-not $savedEnvironment.ContainsKey($Name)) {
        $savedEnvironment[$Name] = [Environment]::GetEnvironmentVariable($Name, 'Process')
    }
    [Environment]::SetEnvironmentVariable($Name, $Value, 'Process')
}

function Update-BuildPath {
    $entries = @($env:Path)
    foreach ($scope in 'Machine', 'User') {
        $entries += [Environment]::GetEnvironmentVariable('Path', $scope)
    }
    $entries += "$env:LOCALAPPDATA\Microsoft\WinGet\Links"
    $entries += Join-Path $env:ProgramFiles 'WinGet\Links'
    if ($env:VSINSTALLDIR) {
        $cmakeTools = Join-Path $env:VSINSTALLDIR 'Common7\IDE\CommonExtensions\Microsoft\CMake'
        $entries += (Join-Path $cmakeTools 'CMake\bin'), (Join-Path $cmakeTools 'Ninja')
    }
    Set-BuildEnvironment Path (($entries -split ';' | Where-Object { $_ } |
        ForEach-Object { [Environment]::ExpandEnvironmentVariables($_) } |
        Select-Object -Unique) -join ';')
}

function Find-BuildTool {
    param([string]$Name)
    $candidates = @((Get-Command "$Name.exe" -CommandType Application -All -ErrorAction SilentlyContinue).Source)
    if ($Name -eq 'cmake') {
        foreach ($hive in 'HKCU:', 'HKLM:') {
            $key = "$hive\Software\Microsoft\Windows\CurrentVersion\App Paths\cmake.exe"
            $registration = Get-Item -LiteralPath $key -ErrorAction SilentlyContinue
            if ($registration) { $candidates += $registration.GetValue('') }
        }
        foreach ($base in $env:ProgramW6432, $env:ProgramFiles, ${env:ProgramFiles(x86)}, "$env:LOCALAPPDATA\Programs") {
            if ($base) { $candidates += Join-Path $base 'CMake\bin\cmake.exe' }
        }
    }
    foreach ($candidate in ($candidates | Where-Object { $_ } | Select-Object -Unique)) {
        if ($Name -eq 'cmake') {
            if (-not (Test-Path -LiteralPath $candidate -PathType Leaf)) { continue }
            $banner = @(& $candidate --version 2>$null)[0]
            if ($LASTEXITCODE -ne 0 -or $banner -notmatch '^cmake version (\d+\.\d+\.\d+)' -or
                [version]$Matches[1] -lt [version]'3.25') { continue }
        }
        if ($Name -eq 'python') {
            # Avoid launching the Store placeholder when Python is absent.
            if ($candidate -like '*\Microsoft\WindowsApps\*') { continue }
            & $candidate -c 'import sys; sys.exit(sys.version_info.major != 3)' 2>$null
            if ($LASTEXITCODE -ne 0) { continue }
        }
        return $candidate
    }
}

function Install-BuildPackage {
    param([string]$Id, [string[]]$Options = @())
    $winget = Get-Command winget.exe -CommandType Application -ErrorAction SilentlyContinue
    if (-not $winget) {
        throw "WinGet is required to install $Id. Install or update Microsoft's App Installer and rerun."
    }
    Write-Host "Installing $Id"
    $arguments = @('install', '--id', $Id, '--exact', '--source', 'winget', '--silent',
        '--disable-interactivity', '--accept-source-agreements', '--accept-package-agreements') + $Options
    $PSNativeCommandUseErrorActionPreference = $false
    & $winget.Source @arguments
    # WinGet can recognize a package that is missing from PATH. The caller rechecks usability.
    if ($LASTEXITCODE -ne 0 -and $LASTEXITCODE -ne -1978335189) {
        throw "WinGet failed to install $Id (exit code $LASTEXITCODE)."
    }
    Update-BuildPath
}

function Get-VsInstallation {
    param([string[]]$Options)
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (Test-Path -LiteralPath $vswhere) {
        Invoke-Checked $vswhere (@('-version', '[17.0,18.0)', '-property', 'installationPath') + $Options)
    }
}

function Import-VsEnvironment {
    if ($env:VisualStudioVersion -eq '17.0' -and $env:VSCMD_ARG_TGT_ARCH -eq 'x64' -and
        $env:WindowsSdkDir -and (Find-BuildTool cl)) { return $true }

    $installations = Get-VsInstallation @('-products', '*', '-requires',
        'Microsoft.VisualStudio.Component.VC.Tools.x86.x64')
    foreach ($installation in $installations) {
        $devCmd = Join-Path $installation 'Common7\Tools\VsDevCmd.bat'
        if (-not (Test-Path -LiteralPath $devCmd)) { continue }
        # Pass cmd's quoted command line directly, bypassing PowerShell's native argument quoting.
        $startInfo = New-Object System.Diagnostics.ProcessStartInfo
        $startInfo.FileName = $env:ComSpec
        $startInfo.Arguments = '/d /s /c ""{0}" -arch=x64 -host_arch=x64 >nul && set"' -f $devCmd
        $startInfo.UseShellExecute = $false
        $startInfo.CreateNoWindow = $true
        $startInfo.RedirectStandardOutput = $true
        $process = [Diagnostics.Process]::Start($startInfo)
        try {
            $settings = $process.StandardOutput.ReadToEnd() -split '\r?\n'
            $process.WaitForExit()
            if ($process.ExitCode -ne 0) { continue }
        } finally {
            $process.Dispose()
        }
        $environment = @{}
        foreach ($entry in $settings) {
            if ($entry -match '^([^=]+)=(.*)$') { $environment[$Matches[1]] = $Matches[2] }
        }
        if (-not ($environment.WindowsSdkDir -and $environment.VCToolsInstallDir -and
            $environment.VSCMD_ARG_TGT_ARCH -eq 'x64')) { continue }
        foreach ($key in $environment.Keys) {
            if ([Environment]::GetEnvironmentVariable($key, 'Process') -cne $environment[$key]) {
                Set-BuildEnvironment $key $environment[$key]
            }
        }
        return $true
    }
    return $false
}

function Test-VsAtl {
    $env:VCToolsInstallDir -and
        (Test-Path -LiteralPath (Join-Path $env:VCToolsInstallDir 'atlmfc\include\atlbase.h')) -and
        (Test-Path -LiteralPath (Join-Path $env:VCToolsInstallDir 'atlmfc\lib\x64\atls.lib'))
}

function Initialize-BuildTools {
    Update-BuildPath
    $hasToolchain = Import-VsEnvironment
    if (-not $hasToolchain -or -not (Test-VsAtl)) {
        $components = '--add Microsoft.VisualStudio.Component.VC.ATL'
        if ($hasToolchain) {
            # Repair the selected installation, including a full Visual Studio IDE.
            $existing = $env:VSINSTALLDIR.TrimEnd('\')
        } else {
            $components += ' --add Microsoft.VisualStudio.Workload.VCTools --add Microsoft.VisualStudio.Component.VC.Tools.x86.x64 --add Microsoft.VisualStudio.Component.Windows11SDK.26100'
            $existing = Get-VsInstallation @('-products', 'Microsoft.VisualStudio.Product.BuildTools', '-latest')
        }
        if ($existing) {
            Write-Host 'Installing missing Visual Studio 2022 C++ components'
            $setup = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\setup.exe'
            $process = Start-Process -FilePath $setup -ArgumentList "modify --installPath `"$existing`" --quiet --norestart $components" -Verb RunAs -Wait -PassThru -WindowStyle Hidden
            if ($process.ExitCode -eq 3010) { throw 'Restart Windows to finish setting up Visual Studio, then rerun.' }
            if ($process.ExitCode -ne 0) { throw "Visual Studio setup failed with exit code $($process.ExitCode)." }
        } else {
            Install-BuildPackage 'Microsoft.VisualStudio.2022.BuildTools' @('--override', "--quiet --wait --norestart $components") | Out-Host
        }
        if (-not (Import-VsEnvironment)) {
            throw 'Visual Studio setup did not yield an x64 compiler and Windows SDK. Check Visual Studio Installer for pending changes or a required reboot.'
        }
        if (-not (Test-VsAtl)) {
            throw 'ATL is missing from the active x64 MSVC toolset. Check Visual Studio Installer for the matching v143 ATL component or a pending reboot.'
        }
    }
    # An already open developer shell may predate the ATL installation.
    $atlInclude = Join-Path $env:VCToolsInstallDir 'atlmfc\include'
    $atlLib = Join-Path $env:VCToolsInstallDir 'atlmfc\lib\x64'
    if (($env:INCLUDE -split ';') -notcontains $atlInclude) {
        Set-BuildEnvironment INCLUDE "$env:INCLUDE;$atlInclude"
    }
    if (($env:LIB -split ';') -notcontains $atlLib) {
        Set-BuildEnvironment LIB "$env:LIB;$atlLib"
    }
    Update-BuildPath

    $packages = [ordered]@{
        git = 'Git.Git'
        cmake = 'Kitware.CMake'
        ninja = 'Ninja-build.Ninja'
        python = 'Python.Python.3.13'
    }
    $tools = @{ cl = Find-BuildTool cl }
    if (-not $tools.cl) { throw 'cl.exe is missing after loading the Visual Studio x64 developer environment.' }
    foreach ($name in $packages.Keys) {
        $tool = Find-BuildTool $name
        if (-not $tool) {
            Install-BuildPackage $packages[$name] | Out-Host
            $tool = Find-BuildTool $name
            if (-not $tool) {
                throw "WinGet finished, but no usable $name.exe was found. Check the $($packages[$name]) installation and add its executable directory to PATH."
            }
        }
        $tools[$name] = $tool
        # Qt's batch scripts resolve tools from PATH themselves.
        $directory = Split-Path -Parent $tool
        Set-BuildEnvironment Path "$directory;$env:Path"
    }
    return $tools
}

if ($WithWebEngine -and -not $Shared) {
    throw 'Qt WebEngine requires a shared build. Use -Shared -WithWebEngine.'
}
if ($SkipPrompts -and -not $AcceptLicense) {
    throw 'Use -AcceptLicense for unattended builds after reviewing the Qt licenses.'
}
# Qt's batch wrappers expand arguments through cmd.exe.
if ($Root -notmatch '^[A-Za-z]:[\\/][A-Za-z0-9_.\\/-]+$' -or
    $ConfigureArgs -match '[%!"&|<>^()\r\n]') {
    throw 'Use a short absolute drive path without spaces for -Root and omit cmd.exe metacharacters from -ConfigureArgs.'
}

$versionNumber = $Version.TrimStart('v')
$tag = "v$versionNumber"
$kind = if ($Shared) { 'Shared' } else { 'Static' }
$sdkDir = Join-Path ([IO.Path]::GetFullPath($Root)) "$kind\$tag"
$sourceDir = Join-Path $sdkDir 'src'
$buildDir = Join-Path $sdkDir 'build'
$installDir = Join-Path $sdkDir 'install'
$verifyDir = Join-Path $sdkDir 'verify'
$logDir = Join-Path $sdkDir 'logs'
$subset = if ($WithWebEngine) { 'all' } else { 'all,-qtwebengine' }

New-Item -ItemType Directory -Path $sdkDir -Force | Out-Null
try {
    $lock = [IO.File]::Open((Join-Path $sdkDir '.installer.lock'), 'OpenOrCreate', 'ReadWrite', 'None')
} catch {
    throw "Could not lock $sdkDir. Check write access and whether another installer is running. $_"
}

$transcribing = $false
try {
    New-Item -ItemType Directory -Path $logDir -Force | Out-Null
    Start-Transcript -Path (Join-Path $logDir "install-$(Get-Date -Format 'yyyyMMdd-HHmmss-fff').log") | Out-Null
    $transcribing = $true
    $tools = Initialize-BuildTools
    $cmakeVersion = @(Invoke-Checked $tools.cmake @('--version'))[0]
    $ninjaVersion = Invoke-Checked $tools.ninja @('--version')
    Write-Host "Qt $versionNumber $kind release, $Jobs parallel jobs, prefix $installDir"

    if (-not (Test-Path -LiteralPath $sourceDir)) {
        if ($SkipClone) { throw "No checkout at $sourceDir for -SkipClone." }
        Invoke-Checked $tools.git @('clone', '--branch', $tag, '--single-branch',
            'https://code.qt.io/qt/qt5.git', $sourceDir)
    }

    if (-not (Test-Path -LiteralPath (Join-Path $sourceDir '.git'))) {
        throw "Expected a Qt Git checkout at $sourceDir."
    }
    $gitArgs = @('-C', $sourceDir)
    $commit = Invoke-Checked $tools.git ($gitArgs + @('rev-parse', 'HEAD'))
    $tagCommit = Invoke-Checked $tools.git ($gitArgs + @('rev-parse', "refs/tags/$tag^{commit}"))
    if ($commit -ne $tagCommit) { throw "Source HEAD differs from $tag. Use a separate -Root." }

    $trackedChanges = Invoke-Checked $tools.git ($gitArgs + @('status', '--porcelain',
        '--untracked-files=no', '--ignore-submodules=all'))
    $moduleChanges = Invoke-Checked $tools.git ($gitArgs + @('submodule', 'foreach', '--quiet',
        '--recursive', 'git status --porcelain --untracked-files=no --ignore-submodules=all'))
    $moduleStatus = @(Invoke-Checked $tools.git ($gitArgs + @('submodule', 'status', '--recursive')))
    if ($trackedChanges -or $moduleChanges -or ($moduleStatus -match '^[+U]')) {
        throw 'Qt sources contain tracked changes, conflicting submodules, or mismatched commits.'
    }

    $flags = @('-release', '-opensource', '-prefix', $installDir, '-cmake-generator', 'Ninja',
        '-nomake', 'examples', '-nomake', 'tests', '-schannel')
    $flags += if ($Shared) { '-shared' } else { '-static' }
    if ($AcceptLicense) { $flags += '-confirm-license' }
    if (-not $WithWebEngine) { $flags += @('-skip', 'qtwebengine') }
    $flags += $ConfigureArgs

    $requestFile = Join-Path $buildDir '.installer-request'
    $installedRequest = Join-Path $sdkDir '.installed-request'
    $atlInclude = Join-Path $env:VCToolsInstallDir 'atlmfc\include'
    $atlLib = Join-Path $env:VCToolsInstallDir 'atlmfc\lib\x64'
    $includePaths = (($env:INCLUDE -split ';') -ne $atlInclude) -join ';'
    $libraryPaths = (($env:LIB -split ';') -ne $atlLib) -join ';'
    # License confirmation controls the prompt only. It is safe to change it on retry.
    $request = [ordered]@{
        Schema = 1
        Commit = $commit
        ModuleSubset = $subset
        Configure = @($flags | Where-Object { $_ -ne '-confirm-license' })
        Tools = @($tools.git, $tools.cmake, $tools.ninja, $tools.cl, $tools.python,
            $cmakeVersion, $ninjaVersion, (Get-Item -LiteralPath $tools.cl).VersionInfo.FileVersion)
        Environment = @($includePaths, $libraryPaths, $env:WindowsSDKVersion,
            $env:CC, $env:CXX, $env:CFLAGS, $env:CXXFLAGS, $env:LDFLAGS, $env:CL, $env:_CL_,
            $env:LINK, $env:CMAKE_PREFIX_PATH, $env:CMAKE_TOOLCHAIN_FILE, $env:CMAKE_GENERATOR)
    } | ConvertTo-Json -Depth 4 -Compress
    if (Test-Path -LiteralPath $requestFile) {
        if ((Get-Content -LiteralPath $requestFile -Raw).TrimEnd() -cne $request) {
            throw 'The SDK build inputs changed. Use a fresh -Root or relocate build and install together.'
        }
    } elseif ((Test-Path -LiteralPath (Join-Path $buildDir 'CMakeCache.txt')) -or
              ((Test-Path -LiteralPath $installDir) -and
               (-not (Test-Path -LiteralPath $installedRequest) -or
                (Get-Content -LiteralPath $installedRequest -Raw).TrimEnd() -cne $request))) {
        throw 'An existing SDK has no matching installer record. Use a fresh -Root or relocate build and install together.'
    }

    if (-not $SkipClone) {
        Push-Location -LiteralPath $sourceDir
        try {
            Invoke-Checked (Join-Path $sourceDir 'init-repository.bat') @('-f', "--module-subset=$subset")
        } finally {
            Pop-Location
        }
        $moduleStatus = @(Invoke-Checked $tools.git ($gitArgs + @('submodule', 'status', '--recursive')))
    }
    $moduleStatus | Set-Content -LiteralPath (Join-Path $sdkDir 'source-modules.txt')
    if ($moduleStatus -match '^[+U]') { throw 'Inspect source-modules.txt for mismatched submodules.' }
    foreach ($module in 'qtbase', 'qtserialport', 'qtlocation') {
        if (-not (Test-Path -LiteralPath (Join-Path $sourceDir "$module\CMakeLists.txt"))) {
            throw "Missing $module sources. Run without -SkipClone to initialize the checkout."
        }
    }

    New-Item -ItemType Directory -Path $buildDir -Force | Out-Null
    $request | Set-Content -LiteralPath $requestFile -Encoding UTF8
    $configured = Join-Path $buildDir '.installer-configured'
    if (-not (Test-Path -LiteralPath $configured) -or
        -not (Test-Path -LiteralPath (Join-Path $buildDir 'build.ninja'))) {
        Push-Location -LiteralPath $buildDir
        try {
            Invoke-Checked (Join-Path $sourceDir 'configure.bat') $flags
        } finally {
            Pop-Location
        }
        New-Item -ItemType File -Path $configured -Force | Out-Null
    }
    Invoke-Checked $tools.cmake @('--build', $buildDir, '--parallel', "$Jobs")
    Invoke-Checked $tools.cmake @('--install', $buildDir, '--config', 'Release')

    # Configure and link through the public SDK, then initialize the platform and TLS plugins.
    New-Item -ItemType Directory -Path $verifyDir -Force | Out-Null
    @'
cmake_minimum_required(VERSION 3.25)
project(QtSdkCheck LANGUAGES CXX)
find_package(Qt6 ${EXPECTED_VERSION} EXACT REQUIRED COMPONENTS Core Gui Widgets Network SerialPort Location
    PATHS "${SDK_PREFIX}" NO_DEFAULT_PATH)
if(MSVC AND QT_FEATURE_static_runtime)
    set(CMAKE_MSVC_RUNTIME_LIBRARY MultiThreaded)
endif()
get_target_property(core_type Qt6::Core TYPE)
if(NOT core_type STREQUAL EXPECTED_CORE_TYPE)
    message(FATAL_ERROR "Expected ${EXPECTED_CORE_TYPE}, got ${core_type}")
endif()
qt_add_executable(qt-sdk-check main.cpp)
target_link_libraries(qt-sdk-check PRIVATE Qt6::Widgets Qt6::Network Qt6::SerialPort Qt6::Location)
'@ | Set-Content -LiteralPath (Join-Path $verifyDir 'CMakeLists.txt') -Encoding ASCII
    @'
#include <QApplication>
#include <QSerialPort>
#include <QSslSocket>
#include <QStringList>

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    QSerialPort port;
    return app.platformName() == QStringLiteral("windows")
        && QSslSocket::setActiveBackend(QStringLiteral("schannel"))
        && QSslSocket::supportsSsl() ? 0 : 1;
}
'@ | Set-Content -LiteralPath (Join-Path $verifyDir 'main.cpp') -Encoding ASCII
    $expectedCore = if ($Shared) { 'SHARED_LIBRARY' } else { 'STATIC_LIBRARY' }
    $verifyBuild = Join-Path $verifyDir 'build'
    Invoke-Checked $tools.cmake @('--fresh', '-S', $verifyDir, '-B', $verifyBuild, '-G', 'Ninja',
        '-DCMAKE_BUILD_TYPE=Release', "-DSDK_PREFIX=$installDir",
        "-DQt6_DIR=$installDir/lib/cmake/Qt6", "-DEXPECTED_VERSION=$versionNumber",
        "-DEXPECTED_CORE_TYPE=$expectedCore")
    Invoke-Checked $tools.cmake @('--build', $verifyBuild, '--parallel', "$Jobs")
    Set-BuildEnvironment Path "$installDir\bin;$env:Path"
    Invoke-Checked (Join-Path $verifyBuild 'qt-sdk-check.exe') @('-platform', 'windows')

    Get-ChildItem -LiteralPath (Join-Path $installDir 'lib\cmake') -Recurse -Filter 'Qt6*Config.cmake' |
        Select-Object -ExpandProperty FullName | Sort-Object |
        Set-Content -LiteralPath (Join-Path $sdkDir 'installed-modules.txt')
    Write-Host "SDK installed and consumer verified: $installDir"
    Write-Host "See $sdkDir\installed-modules.txt and $buildDir\config.summary for module and feature coverage."
    $promptForCleanup = -not $AcceptLicense -and $Host.Name -eq 'ConsoleHost' -and
        -not [Console]::IsInputRedirected
    foreach ($directory in 'build', 'src') {
        $target = Join-Path $sdkDir $directory
        if ($promptForCleanup) {
            try {
                $answer = Read-Host "Delete $target? [Y/n]"
                if ($null -eq $answer -or $answer -notmatch '^\s*(y(es)?)?\s*$') { continue }
            } catch [System.Management.Automation.PSInvalidOperationException] {
                # A noninteractive console host can reject Read-Host.
                $promptForCleanup = $false
            }
        }
        if (-not (Test-SdkCleanupPath $sdkDir $directory)) { continue }
        try {
            if ($directory -eq 'build') {
                Copy-Item -LiteralPath $requestFile -Destination $installedRequest
            }
            Remove-Item -LiteralPath $target -Recurse -Force
            Write-Host "Removed $target"
        } catch {
            Write-Warning "Could not finish removing $directory. The SDK is installed. $_"
        }
    }
} finally {
    foreach ($key in $savedEnvironment.Keys) {
        [Environment]::SetEnvironmentVariable($key, $savedEnvironment[$key], 'Process')
    }
    try {
        if ($transcribing) { Stop-Transcript | Out-Null }
    } finally {
        $lock.Dispose()
    }
}

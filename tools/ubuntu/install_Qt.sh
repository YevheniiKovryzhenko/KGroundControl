#!/usr/bin/env bash
# Build a native Ubuntu Qt SDK; Qt is static by default, system libraries may be shared.
# See README.md for scope, optional dependencies, and project setup.
set -Eeuo pipefail
trap 'printf "ERROR: line %s: %s\n" "$LINENO" "$BASH_COMMAND" >&2' ERR

version=6.12.0
build_type=Static
qt_root=${HOME}/Qt
jobs=${JOBS:-}
install_deps=true
creator=false
accept_license=false
with_webengine=false
extra_configure=()
usage() {
    cat <<'HELP'
Usage: install.sh [options] [-- extra Qt configure arguments]
  --version VERSION     Qt 6 release tag (default: 6.12.0; optional v prefix)
  --shared              Build shared release libraries instead of static
  --root PATH           SDK parent (default: $HOME/Qt)
  --jobs N              Parallel jobs (default: min(CPUs, available RAM / 3 GiB))
  --no-deps             Do not run apt (dependencies must already be installed)
  --creator             Also install Ubuntu's Qt Creator package
  --with-webengine      Include WebEngine/PDF sources (shared builds only)
  -y, --accept-license  Accept Qt's open-source license without configure prompt
  -h, --help            Show help
Existing sources/builds are retained for incremental retries. No automatic deletion.
Changed configuration requires a new --root or manual relocation of the old build.
HELP
}
die() { printf 'ERROR: %s\n' "$*" >&2; exit 1; }
while (($#)); do
    case "$1" in
        --version|--root|--jobs)
            (($# >= 2)) || die "Missing value for $1"
            case "$1" in
                --version) version=${2#v} ;;
                --root) qt_root=$2 ;;
                --jobs) jobs=$2 ;;
            esac
            shift 2 ;;
        --shared) build_type=Shared; shift ;;
        --no-deps) install_deps=false; shift ;;
        --creator) creator=true; shift ;;
        --with-webengine) with_webengine=true; shift ;;
        -y|--accept-license) accept_license=true; shift ;;
        -h|--help) usage; exit 0 ;;
        --) shift; extra_configure=("$@"); break ;;
        *) die "Unknown option: $1 (use --help)" ;;
    esac
done
[[ $version =~ ^6\.[0-9]+\.[0-9]+$ ]] || die 'Expected a Qt 6 release such as 6.12.0'
[[ -z $jobs || $jobs =~ ^[1-9][0-9]*$ ]] || die '--jobs must be a positive integer'
[[ $qt_root = /* && $qt_root != / ]] || die '--root must be an absolute directory other than /'
if [[ $build_type = Static ]] && $with_webengine; then
    die 'Qt WebEngine does not support static builds; use --shared --with-webengine'
fi
((EUID != 0)) || die 'Run as a normal user; only package installation uses sudo'
if ! $accept_license && [[ ! -t 0 ]]; then
    die 'Noninteractive builds require --accept-license after reviewing the Qt licenses'
fi

# Explicit package names: no shell/apt wildcards and no unrelated system upgrade.
if $install_deps; then
    command -v apt-get >/dev/null || die 'apt-get is required; use --no-deps on other distributions'
    sudo apt-get update
    packages=(
        build-essential cmake ninja-build git perl python3 python3-html5lib
        bison flex gperf pkg-config clang libclang-dev llvm-dev ca-certificates
        libfontconfig1-dev libfreetype-dev libgtk-3-dev libatspi2.0-dev
        libx11-dev libx11-xcb-dev libxext-dev libxfixes-dev libxi-dev libxrender-dev
        libxcb1-dev libxcb-cursor-dev libxcb-glx0-dev libxcb-keysyms1-dev
        libxcb-image0-dev libxcb-shm0-dev libxcb-icccm4-dev libxcb-sync-dev
        libxcb-xfixes0-dev libxcb-shape0-dev libxcb-randr0-dev libxcb-render0-dev
        libxcb-render-util0-dev libxcb-util-dev libxcb-xinerama0-dev libxcb-xkb-dev
        libxkbcommon-dev libxkbcommon-x11-dev libsm-dev libice-dev
        libgl-dev libegl-dev libglu1-mesa-dev libgbm-dev libdrm-dev libvulkan-dev
        libwayland-dev wayland-protocols libinput-dev libudev-dev libmtdev-dev
        libssl-dev libdbus-1-dev libicu-dev libglib2.0-dev zlib1g-dev
        libjpeg-dev libpng-dev libtiff-dev libwebp-dev libmng-dev
        libharfbuzz-dev libdouble-conversion-dev libpcre2-dev libzstd-dev
        libsqlite3-dev libpq-dev default-libmysqlclient-dev unixodbc-dev
        libasound2-dev libpulse-dev libsndfile1-dev libpipewire-0.3-dev
        libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev libavcodec-dev
        libavformat-dev libavutil-dev libswresample-dev libswscale-dev
        libbluetooth-dev libpcsclite-dev libusb-1.0-0-dev libsdl2-dev
        libspeechd-dev flite1-dev libhunspell-dev libprotobuf-dev
        protobuf-compiler libgrpc++-dev protobuf-compiler-grpc
    )
    if $with_webengine; then
        packages+=(nodejs libnss3-dev libnspr4-dev libxcomposite-dev libxcursor-dev
            libxdamage-dev libxrandr-dev libxshmfence-dev libxss-dev libxtst-dev
            libxkbfile-dev libcups2-dev libcap-dev libpci-dev)
    fi
    if $creator; then packages+=(qtcreator); fi
    sudo apt-get install -y "${packages[@]}"
elif $creator; then
    die '--creator requires package installation; remove --no-deps'
fi
for tool in cmake ninja git perl python3 pkg-config c++; do
    command -v "$tool" >/dev/null || die "Missing build tool: $tool"
done
if [[ -z $jobs ]]; then
    cpu_jobs=$(getconf _NPROCESSORS_ONLN)
    memory_jobs=$(awk '/MemAvailable:/ {n=int($2/3145728); print (n>0?n:1)}' /proc/meminfo)
    jobs=$((cpu_jobs < memory_jobs ? cpu_jobs : memory_jobs))
fi
sdk_root="$qt_root/$build_type/v$version"
mkdir -p "$sdk_root"
sdk_root=$(cd "$sdk_root" && pwd -P)
src_dir="$sdk_root/src"
build_dir="$sdk_root/build"
install_dir="$sdk_root/install"
# Serialize runs targeting the same SDK (flock comes with Ubuntu's util-linux).
exec 9>"$sdk_root/.installer.lock"
flock -n 9 || die "Another installer is using $sdk_root"
mkdir -p "$sdk_root/logs"
exec > >(tee -a "$sdk_root/logs/install-$(date +%Y%m%d-%H%M%S).log") 2>&1
printf 'Building Qt %s (%s), jobs=%s, prefix=%s\n' "$version" "$build_type" "$jobs" "$install_dir"

if [[ ! -e $src_dir ]]; then
    git clone --branch "v$version" --single-branch https://code.qt.io/qt/qt5.git "$src_dir"
fi
[[ -e $src_dir/.git ]] || die "Source directory is not a Git checkout: $src_dir (relocate it and retry)"
# Never reset, clean, or force-switch a user's sources.
actual_commit=$(git -C "$src_dir" rev-parse HEAD)
expected_commit=$(git -C "$src_dir" rev-parse "refs/tags/v$version^{commit}")
[[ $actual_commit = "$expected_commit" ]] || die 'Source HEAD differs from requested tag; use a separate --root'
[[ -z $(git -C "$src_dir" status --porcelain --untracked-files=no --ignore-submodules=all) ]] || die 'Tracked source files have local changes'
# Check existing submodules before initialization can update their worktrees.
[[ -z $(git -C "$src_dir" submodule foreach --quiet --recursive 'git status --porcelain --untracked-files=no') ]] || die 'Submodules have tracked local modifications'
# all includes add-ons and preview/deprecated modules omitted by default.
subset=all
if ! $with_webengine; then subset+=,-qtwebengine; fi
# -f is required by Qt for repeat initialization, including interrupted runs.
# Reject changed checked-out commits before asking its initializer to proceed.
if git -C "$src_dir" submodule status --recursive | grep -Eq '^[+U]'; then
    die 'Existing submodules differ from release commits or have conflicts'
fi
(cd "$src_dir" && ./init-repository -f --module-subset="$subset")
git -C "$src_dir" submodule status --recursive > "$sdk_root/source-modules.txt"
if grep -Eq '^[+U]' "$sdk_root/source-modules.txt"; then
    die 'Submodules differ from release commits or have conflicts; inspect source-modules.txt'
fi
[[ -z $(git -C "$src_dir" submodule foreach --quiet --recursive 'git status --porcelain --untracked-files=no') ]] || die 'Submodules have tracked local modifications'

flags=(-release -opensource -prefix "$install_dir" -nomake examples -nomake tests)
if [[ $build_type = Static ]]; then flags+=(-static); else flags+=(-shared); fi
if $accept_license; then flags+=(-confirm-license); fi
if ! $with_webengine; then flags+=(-skip qtwebengine); fi
# Require desktop X11 and TLS support instead of silently omitting these plugins.
flags+=(-xcb -openssl-runtime)
flags+=("${extra_configure[@]}")
mkdir -p "$build_dir"
# Refuse stale cache reuse when source/configuration/toolchain inputs change.
request=$(printf '%s\n' 'installer-schema=2' "$actual_commit" "$subset" "${flags[@]}" \
    "CC=${CC:-}" "CXX=${CXX:-}" "CFLAGS=${CFLAGS:-}" "CXXFLAGS=${CXXFLAGS:-}" \
    "LDFLAGS=${LDFLAGS:-}" "PATH=$PATH" "CMAKE_PREFIX_PATH=${CMAKE_PREFIX_PATH:-}" \
    "PKG_CONFIG_PATH=${PKG_CONFIG_PATH:-}" "$(cmake --version)" "$(c++ --version)" | sha256sum)
if [[ -f $build_dir/CMakeCache.txt ]]; then
    [[ -f $build_dir/.installer-request && $(cat "$build_dir/.installer-request") = "$request" ]] ||
        die "Build configuration changed or cache is from an older installer. Relocate $build_dir (and use a fresh install prefix) or choose another --root."
fi
# Always configure/build/install: CMake/Ninja handle incremental work and interrupted runs.
printf '%s\n' "$request" > "$build_dir/.installer-request"
(cd "$build_dir" && "$src_dir/configure" "${flags[@]}")
cmake --build "$build_dir" --parallel "$jobs"
cmake --install "$build_dir"

# A real consumer configure verifies module packages, static transitive dependencies,
# and plugin targets against the installed prefix instead of trusting marker files.
mkdir -p "$sdk_root/verify"
cat > "$sdk_root/verify/CMakeLists.txt" <<'CMAKE'
cmake_minimum_required(VERSION 3.22)
project(QtSdkCheck LANGUAGES CXX)
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network SerialPort Location)
foreach(plugin QXcbIntegrationPlugin QTlsBackendOpenSSLPlugin)
    if(NOT TARGET Qt6::${plugin})
        message(FATAL_ERROR "Desktop SDK validation requires Qt6::${plugin}")
    endif()
endforeach()
get_target_property(core_type Qt6::Core TYPE)
if(EXPECT_STATIC AND NOT core_type STREQUAL "STATIC_LIBRARY")
    message(FATAL_ERROR "Expected static Qt Core, got ${core_type}")
endif()
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/main.cpp" "#include <QApplication>\n#include <QSslSocket>\n#include <QSerialPort>\nint main(int argc,char**argv){QApplication app(argc,argv);QSerialPort port;return QSslSocket::supportsSsl()?0:1;}\n")
qt_add_executable(qt-sdk-check "${CMAKE_CURRENT_BINARY_DIR}/main.cpp")
target_link_libraries(qt-sdk-check PRIVATE Qt6::Widgets Qt6::Network Qt6::SerialPort Qt6::Location)
if(EXPECT_STATIC)
    qt_import_plugins(qt-sdk-check NO_DEFAULT INCLUDE Qt6::QXcbIntegrationPlugin Qt6::QTlsBackendOpenSSLPlugin)
endif()
CMAKE
expect_static=OFF
if [[ $build_type = Static ]]; then expect_static=ON; fi
cmake -S "$sdk_root/verify" -B "$sdk_root/verify/build" -G Ninja \
    -DCMAKE_PREFIX_PATH="$install_dir" -DQt6_DIR="$install_dir/lib/cmake/Qt6" \
    -DEXPECT_STATIC="$expect_static"
cmake --build "$sdk_root/verify/build" --parallel "$jobs"
find "$install_dir" -name 'Qt6*Config.cmake' -print | sort > "$sdk_root/installed-modules.txt"
printf '\nSDK installed and consumer link verified: %s\nModule inventory: %s\n' "$install_dir" "$sdk_root/installed-modules.txt"
printf 'Review %s/config.summary for optional features disabled by dependency detection.\n' "$build_dir"

#!/bin/bash
#--------------------------------------------------------------
# Step 0: Check if the script is being run as a superuser
#--------------------------------------------------------------
if [ "$(id -u)" -eq 0 ]; then
    echo "ERROR: This script should not be run as a superuser (root). Please run it as a normal user."
    exit 1
fi

#--------------------------------------------------------------
# Check for the -y flag to skip all package prompts (kept for apt)
#--------------------------------------------------------------
skip_prompts=false
while getopts "y" opt; do
    case ${opt} in
        y ) skip_prompts=true;;
        \? ) echo "Usage: $0 [-y]"; exit 1;;
    esac
done

# --- Prompt helpers (single-keypress y/n, no enter required) ---
skip_prompts=false
delete_build_after=false
delete_src_after=false

prompt_yes_no() {
    # Usage: prompt_yes_no "Prompt text" default_yes/no
    local prompt="$1"
    local default="$2"
    local dchar="N"
    local hint="[y/N]"
    if [ "$default" = "yes" ]; then
        dchar="Y"
        hint="[Y/n]"
    fi
    if $skip_prompts; then
        [ "$default" = "yes" ] && return 0 || return 1
    fi
    printf "%s %s " "$prompt" "$hint"
    while IFS= read -r -n1 -s key; do
        case "$key" in
            y|Y) echo "Y"; return 0 ;;
            n|N) echo "N"; return 1 ;;
            "") echo "$dchar"; [ "$default" = "yes" ] && return 0 || return 1 ;;
        esac
    done
}

# Parse -y flag for non-interactive
while getopts "y" opt; do
    case ${opt} in
        y ) skip_prompts=true;;
        \? ) echo "Usage: $0 [-y]"; exit 1;;
    esac
done

# --- Upfront prompts: ask all questions before long install ---
if prompt_yes_no "Delete build directory automatically after successful install?" yes; then
    delete_build_after=true
fi
if prompt_yes_no "Delete source directory after successful install? (keep by default)" no; then
    delete_src_after=true
fi
#--------------------------------------------------------------
# Step 1: Update and Upgrade the System
#--------------------------------------------------------------
echo "==============================================="
echo "Updating and Upgrading System Packages..."
echo "==============================================="
sudo_prompted=false
if ! sudo -n true 2>/dev/null; then
    echo "Sudo password required for package installation."
    sudo_prompted=true
    sudo true || { echo "ERROR: Sudo authentication failed."; exit 1; }
fi
sudo apt-get update && sudo apt-get upgrade -y
echo

#--------------------------------------------------------------
# Step 2: Install Required Packages
#--------------------------------------------------------------
echo "==============================================="
echo "Installing required packages..."
echo "==============================================="
sudo apt-get install -y make cmake build-essential libclang-dev clang ninja-build gcc git bison python3 gperf pkg-config \
libfontconfig1-dev libfreetype6-dev libx11-dev libx11-xcb-dev libxext-dev libxfixes-dev libxi-dev libxrender-dev \
libxcb1-dev libxcb-glx0-dev libxcb-keysyms1-dev libxcb-image0-dev libxcb-shm0-dev libxcb-icccm4-dev libxcb-sync-dev \
libxcb-xfixes0-dev libxcb-shape0-dev libxcb-randr0-dev libxcb-render-util0-dev libxcb-util-dev libxcb-xinerama0-dev \
libxcb-xkb-dev libxkbcommon-dev libxkbcommon-x11-dev libatspi2.0-dev libgl1-mesa-dev libglu1-mesa-dev freeglut3-dev \
libsdl2-dev g++ pkg-config libgl1-mesa-dev libxcb*-dev libfontconfig1-dev libxkbcommon-x11-dev libgtk-3-dev \
libssl-dev
echo

#--------------------------------------------------------------
# Step 3: Setup Qt Version, Build Type, and Directories
#--------------------------------------------------------------
# Hardcoded parameters (adjust here):
Qt_version="v6.10.2"   # Tag/branch to use
Build_type="Static"    # Options: "Static" (default) or "Shared"

# Resolve target subdirectory and configure flags based on Build_type
case "$Build_type" in
    Static)
        qt_subdir="Static"
        configure_flags=(
            -static -release
            -no-feature-accessibility
            -opensource
            -prefix
        )
        ;;
    Shared)
        qt_subdir="Shared"
        configure_flags=(
            -debug
            -no-feature-accessibility
            -opensource
            -prefix
        )
        ;;
    *)
        echo "ERROR: Unknown Build_type='$Build_type'. Use 'Static' or 'Shared'."
        exit 1
        ;;
esac

Qt_root_dir="$HOME/Qt/${qt_subdir}/$Qt_version"
src_dir="${Qt_root_dir}/src"
build_dir="${Qt_root_dir}/build"
install_dir="${Qt_root_dir}/install"

echo "==============================================="
echo "Preparing Qt $Qt_version ($Build_type) in $Qt_root_dir"
echo "==============================================="
mkdir -p "$Qt_root_dir"

# --- New: checkpoint/state helpers ----------------------------------------
STATE_DIR="$Qt_root_dir/.install_state"
mkdir -p "$STATE_DIR"

mark_step() {
    # mark a step as completed: mark_step cloned|initialized|configured|built|installed
    mkdir -p "$STATE_DIR"
    touch "$STATE_DIR/$1"
}

step_done() {
    [[ -f "$STATE_DIR/$1" ]]
}

prompt_continue() {
    # usage: prompt_continue "question"
    if $skip_prompts; then
        return 0
    fi
    read -r -p "$1 [y/N]: " resp
    case "$resp" in
        [yY]) return 0 ;;
        *) return 1 ;;
    esac
}

#--------------------------------------------------------------
# Step 4: Smart clone/switch of source (with checkpoints)
#--------------------------------------------------------------
source_changed=false

# Decide whether we need to reclone - use checkpoint when present
need_reclone=false
# If a successful clone checkpoint exists and the git dir is present, we can reuse it.
if step_done cloned && [ -d "$src_dir/.git" ]; then
    echo "Found previous successful clone checkpoint; reusing existing repository."
    need_reclone=false
else
    # No checkpoint -> assume previous attempt failed. Do not prompt: remove and reclone.
    if [ -d "$src_dir" ] && [ ! -d "$src_dir/.git" ]; then
        echo "Found non-git directory at $src_dir without checkpoint. Removing and will reclone."
        rm -rf "$src_dir"
    elif [ -d "$src_dir/.git" ] && ! step_done cloned; then
        echo "Found git repository at $src_dir but no 'cloned' checkpoint. Assuming failed attempt; removing and will reclone."
        rm -rf "$src_dir"
    fi
    need_reclone=true
fi

# If we decided to reuse (need_reclone=false) ensure repo exists and attempt to switch/init
if [ -d "$src_dir/.git" ] && ! $need_reclone; then
    echo "Using existing repository at $src_dir"
    (
        set -e
        cd "$src_dir"
        git rev-parse --is-inside-work-tree >/dev/null 2>&1 || { echo "Not a git repo"; exit 1; }
        git fetch --tags -p origin || true
        current_ref=$(git describe --tags --exact-match 2>/dev/null || git symbolic-ref --short -q HEAD || echo "unknown")
        if [ "$current_ref" != "$Qt_version" ]; then
            echo "Switching Qt sources from '$current_ref' to '$Qt_version'..."
            git reset --hard
            git clean -fdx
            git checkout -f "$Qt_version"
            source_changed=true
        else
            echo "Qt sources already at $Qt_version; no switch needed."
        fi

        # Ensure submodules are initialized; filter out qtwebengine before init
        # remove_webengine_submodules
        if ./init-repository --module-subset=default,-qtwebengine -f; then
            # basic verification: qtbase (essential) should exist
            if [ -d "qtbase" ] || [ -d "$src_dir/qtbase" ]; then
                mark_step cloned || true
                mark_step initialized || true
                echo "Submodules initialized successfully."
            else
                echo "Warning: init-repository claimed success but expected submodule 'qtbase' missing."
                exit 1
            fi
        else
            echo "ERROR: init-repository failed for existing repo."
            exit 1
        fi
    ) || {
        echo "Existing repo handling failed -> will attempt fresh reclone."
        need_reclone=true
    }
fi

# Fresh clone path (unchanged in intent) — runs when need_reclone is true
if $need_reclone; then
    echo "Re-cloning Qt sources at $Qt_version to recover or first-time setup..."
    rm -rf "$src_dir"
    git clone --branch "${Qt_version}" https://code.qt.io/qt/qt5.git "$src_dir" || {
        echo "ERROR: Failed to clone Qt repository."
        exit 1
    }
    # Mark clone checkpoint
    mark_step cloned

    (
        set -e
        cd "$src_dir"
        # filter out qtwebengine-related submodules before initialization
        # remove_webengine_submodules
        if ./init-repository --module-subset=default,-qtwebengine -f; then
            # verify basic submodule presence
            if [ -d "qtbase" ] || [ -d "$src_dir/qtbase" ]; then
                mark_step initialized
                echo "Submodules initialized successfully after fresh clone."
            else
                echo "ERROR: init-repository reported success but 'qtbase' submodule missing."
                exit 1
            fi
        else
            echo "ERROR: Failed to initialize Qt submodules."
            exit 1
        fi
    ) || {
        echo "ERROR: Failed during submodule initialization after clone."
        exit 1
    }
    source_changed=true
fi
echo

#--------------------------------------------------------------
# Step 5: Configure the build (reconfigure when sources changed or first time)
# Use checkpoint to skip unnecessary reconfigure
#--------------------------------------------------------------
cd "${Qt_root_dir}"

# Clean build/install only if sources changed (reclone or switched)
if $source_changed; then
    if [ -d "$build_dir" ]; then
        echo "==============================================="
        echo "Sources changed -> cleaning existing build folder..."
        echo "==============================================="
        rm -rf "$build_dir"
    fi
    if [ -d "$install_dir" ]; then
        echo "==============================================="
        echo "Sources changed -> cleaning existing install folder..."
        echo "==============================================="
        rm -rf "$install_dir"
    fi
fi

# Decide whether to configure: if configured checkpoint exists and sources didn't change, skip
should_configure=false
if step_done configured && ! $source_changed && [ -f "$build_dir/CMakeCache.txt" ]; then
    echo "Found configured checkpoint and no source changes -> skipping configure."
    should_configure=false
else
    should_configure=true
fi

if $should_configure; then
    echo "==============================================="
    echo "Configuring build settings ($Build_type)..."
    echo "==============================================="
    mkdir -p "$build_dir"
    cd "$build_dir"
    # Build full configure command
    ../src/configure \
        "${configure_flags[@]}" "${install_dir}/" \
        -nomake examples -nomake tests -skip qtwebengine \
        -DQT_FEATURE_xcb=ON -DFEATURE_xcb_xlib=ON -DQT_FEATURE_xlib=ON
    if [[ $? -ne 0 ]]; then
        echo "ERROR: Configure failed."
        exit 1
    fi
    mark_step configured
    echo
else
    echo "Configuration unchanged; skipping reconfigure."
fi

#--------------------------------------------------------------
# Step 6: Compile Qt (only when needed)
#--------------------------------------------------------------
if step_done built && ! $should_configure; then
    echo "Found built checkpoint and no reconfigure required -> skipping build."
else
    echo "==============================================="
    echo "Compiling $Qt_version..."
    echo "==============================================="
    mkdir -p "$build_dir"
    cd "$build_dir"
    cmake --build . --parallel
    if [[ $? -ne 0 ]]; then
        echo "ERROR: Build failed."
        exit 1
    fi
    mark_step built
    echo
fi

#--------------------------------------------------------------
# Step 7: Install Qt (only when build ran this run or not installed)
#--------------------------------------------------------------
if step_done installed && ! $should_configure; then
    echo "Found installed checkpoint -> skipping install."
else
    echo "==============================================="
    echo "Installing $Qt_version..."
    echo "==============================================="
    cd "$build_dir"
    cmake --install .
    if [[ $? -ne 0 ]]; then
        echo "ERROR: Install failed."
        exit 1
    fi
    mark_step installed
    echo
fi

#--------------------------------------------------------------
# Step 8: Install Qt Creator (optional)
#--------------------------------------------------------------
echo "==============================================="
echo "Installing Qt Creator..."
echo "==============================================="
if sudo -n true 2>/dev/null; then
    sudo apt install qtcreator -y
post_cleanup
else
    # avoid parentheses in this echoed string to reduce chance of parser confusion
    echo "Skipping Qt Creator install - sudo requires password."
fi
echo

#--------------------------------------------------------------
# Final Step: Completion Message
#--------------------------------------------------------------
echo "==============================================="
echo "Qt $Qt_version installation and setup completed successfully!"
echo "==============================================="

# --- Ensure cleanup runs after install ---
if $delete_build_after && [ -d "$build_dir" ]; then
    echo "Deleting build directory to free disk space..."
    rm -rf "$build_dir"
    echo "Build directory deleted."
fi
if $delete_src_after && [ -d "$src_dir" ]; then
    echo "Deleting source directory..."
    rm -rf "$src_dir"
    echo "Source directory deleted."
fi

#!/bin/bash

#--------------------------------------------------------------
# Step 0: Check if the script is being run as a superuser
#--------------------------------------------------------------
if [ "$(id -u)" -eq 0 ]; then
    echo "ERROR: This script should not be run as a superuser (root). Please run it as a normal user."
    exit 1
fi

#--------------------------------------------------------------
# Check for the -y flag to skip all prompts
#--------------------------------------------------------------
skip_prompts=false
while getopts "y" opt; do
    case ${opt} in
        y ) skip_prompts=true;;
        \? ) echo "Usage: $0 [-y]"; exit 1;;
    esac
done

#--------------------------------------------------------------
# Step 1: Update and Upgrade the System
#--------------------------------------------------------------
echo "==============================================="
echo "Updating and Upgrading System Packages..."
echo "==============================================="
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
    libsdl2-dev g++ pkg-config libgl1-mesa-dev libxcb*-dev libfontconfig1-dev libxkbcommon-x11-dev libgtk-3-dev
echo

#--------------------------------------------------------------
# Step 3: Setup Qt Version and Directory
#--------------------------------------------------------------
Qt_version="v6.8.0"
Qt_root_dir="$HOME/Qt/$Qt_version"

echo "==============================================="
echo "Setting up Qt $Qt_version installation directory in $Qt_root_dir..."
echo "==============================================="

# Check if the root directory exists; if not, create it and clone the Qt repo
if [ ! -d "$Qt_root_dir" ]; then
    # Directory doesn't exist, create it
    mkdir -p "$Qt_root_dir"
    echo "Created directory: $Qt_root_dir"
    
    # Clone the Qt repository into src directory
    echo "Cloning $Qt_version into $Qt_root_dir/src"
    git clone --branch "${Qt_version}" git://code.qt.io/qt/qt5.git "${Qt_root_dir}/src"
    cd "${Qt_root_dir}/src"
    ./init-repository
else
    # Directory exists, ask if the user wants to delete it, or automatically delete if -y flag is set
    if $skip_prompts; then
        echo "Skipping prompt: Automatically deleting $Qt_root_dir..."
        rm -rf "$Qt_root_dir"
        mkdir -p "$Qt_root_dir"
        echo "Directory deleted and recreated: $Qt_root_dir"
        
        echo "Cloning $Qt_version into $Qt_root_dir/src"
        git clone --branch "${Qt_version}" git://code.qt.io/qt/qt5.git "${Qt_root_dir}/src"
        cd "${Qt_root_dir}/src"
        ./init-repository
    else
        echo "$Qt_root_dir already exists, do you want to delete it and clone? (yes/no)"
        while true; do
            read -p "Please enter yes or no: " yn
            case $yn in
                [Yy]* ) 
                    echo "Deleting $Qt_root_dir..."
                    rm -rf "$Qt_root_dir"
                    mkdir -p "$Qt_root_dir"
                    echo "Directory deleted and recreated: $Qt_root_dir"
                    
                    echo "Cloning $Qt_version into $Qt_root_dir/src"
                    git clone --branch "${Qt_version}" git://code.qt.io/qt/qt5.git "${Qt_root_dir}/src"
                    cd "${Qt_root_dir}/src"
                    ./init-repository
                    break
                    ;;
                [Nn]* ) 
                    echo "Aborting. The directory will not be deleted. Assumed repository has been cloned and properly initialized."
                    break
                    ;;
                * ) 
                    continue
                    ;;
            esac
        done
    fi
fi
echo

#--------------------------------------------------------------
# Step 4: Clean Existing Build Folders and Start Build Process
#--------------------------------------------------------------
cd "${Qt_root_dir}"

# Check and delete the build-static folder if it exists
if [ -d "${Qt_root_dir}/build-static" ]; then
    echo "==============================================="
    echo "Deleting existing build-static folder..."
    echo "==============================================="
    rm -rf "${Qt_root_dir}/build-static"
fi

# Check and delete the install-static folder if it exists
if [ -d "${Qt_root_dir}/install-static" ]; then
    echo "==============================================="
    echo "Deleting existing install-static folder..."
    echo "==============================================="
    rm -rf "${Qt_root_dir}/install-static"
fi

# Step 5: Configure the Build
echo "==============================================="
echo "Configuring build settings..."
echo "==============================================="
mkdir "${Qt_root_dir}/build-static"
cd "${Qt_root_dir}/build-static"

# Run the configure script with the necessary flags
../src/configure -static -no-feature-accessibility -release -opensource -prefix "${Qt_root_dir}/install-static/" \
    -nomake examples -nomake tests -DQT_FEATURE_xcb=ON -DFEATURE_xcb_xlib=ON -DQT_FEATURE_xlib=ON
echo

#--------------------------------------------------------------
# Step 6: Compile Qt
#--------------------------------------------------------------
echo "==============================================="
echo "Compiling $Qt_version..."
echo "==============================================="
cmake --build . --parallel
echo

#--------------------------------------------------------------
# Step 7: Install Qt
#--------------------------------------------------------------
echo "==============================================="
echo "Installing $Qt_version..."
echo "==============================================="
cmake --install .
echo

#--------------------------------------------------------------
# Step 8: Install Qt Creator (optional)
#--------------------------------------------------------------
echo "==============================================="
echo "Installing Qt Creator..."
echo "==============================================="
sudo apt install qtcreator -y
echo

#--------------------------------------------------------------
# Final Step: Completion Message
#--------------------------------------------------------------
echo "==============================================="
echo "Qt $Qt_version installation and setup completed successfully!"
echo "==============================================="

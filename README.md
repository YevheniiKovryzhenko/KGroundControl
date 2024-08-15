# KGroundControl (KGC) - Ground Control and Monitoring Software

# ![Logo](resources/Images/Logo/KLogo.png)

This is a cross-platform ground control station software I have developed for my research purposes. 
My initial goal for this was to combine multiple ground control stations I have previously written
and implement a graphical interface for obvious reasons. This is my first app written in Qt and 
developing a UI. I have found QGroundControl to be insufficient for my purposes, so KGroundControl
was originally written as a support application that should work together with QGroundControl.
KGroundControl is standalone and supports Mavlink communication via serial and UDP, 
which can relay/bridge data streams to/from single to/from multiple ports. Data forwarding and the ability
to talk with multiple devices through a single port was one major aspect I needed for my research, 
so the entire app is written from scratch with that in mind

Additionally, it supports the Optitrack Motion Capture System and can stream data to multiple UAVs.


# Tested Architectures and Platforms
* x86-64:
  - Windows - installer and binary available
  - Ubuntu - installer available
  - Arch   - binary available
* ARM64:
  - Raspbian - must compile from source

# Installation
## Installer
Use one of the installers, which are available under the releases tab. All instructions are 
included as part of the installation walkthrough. The installer will create a shortcut on your desktop
and an uninstaller in the same directory to properly remove the program.

## Binary
You can use the standalone binary (executable) file as is, without any installation. 
It is compiled with statically built Qt and should be the most optimized 
version of the app (performance-wise), but it may take longer to start. If using Linux, 
you may still have to download some external libraries, but this highly depends on your distribution
. The executable may need to be recompiled for your specific platform (see Compiling from source). 

## Compiling from source
Requirements: 
* Qt 6.2+
* For installer creation, you should install [cqtdeployer](https://github.com/QuasarApp/CQtDeployer)

### With [Qt online installer](https://doc.qt.io/qt-6/get-and-install-qt.html) (Comertial Licence Required)
This is, perhaps, the easiest option. You just need to install Qt using the official online installer, get the source code for KGroundControl, 
and compile. This will not be sufficient to generate a standalone executable so you will either have to use a deployment tool (like cqtdeployer),
create your own installer, or compile a static version of Qt from the source.

### With the static version of Qt (open-source)
> [!NOTE]
> Keep in mind, that this is a very time-consuming process and you may have to restart it a couple of times if something was done incorrectly.
> You need a lot of storage space for this, get yourself at least 128Gb or better 256Gb of free space before you start.

Here is a quick example of compiling everything on Linux:
1. Make sure you have all the dependencies:
  * Update system:
    ```bash
    sudo apt-get update && sudo apt-get upgrade -y    
    ```
  * Get required packages:
    ```bash
    sudo apt-get install -y make cmake build-essential libclang-dev clang ninja-build gcc git bison python3 gperf pkg-config libfontconfig1-dev libfreetype6-dev libx11-dev libx11-xcb-dev libxext-dev libxfixes-dev libxi-dev libxrender-dev libxcb1-dev libxcb-glx0-dev libxcb-keysyms1-dev libxcb-image0-dev libxcb-shm0-dev libxcb-icccm4-dev libxcb-sync-dev libxcb-xfixes0-dev libxcb-shape0-dev libxcb-randr0-dev libxcb-render-util0-dev libxcb-util-dev libxcb-xinerama0-dev libxcb-xkb-dev libxkbcommon-dev libxkbcommon-x11-dev libatspi2.0-dev libgl1-mesa-dev libglu1-mesa-dev freeglut3-dev    
    ```
2. [Compile Qt 6.7 from source](https://doc.qt.io/qt-6/get-and-install-qt.html):
  * Get Qt source:
    ```bash
    cd ~
    git clone --branch v6.7.2 git://code.qt.io/qt/qt5.git Qt/6.7.2/src    
    ```
  * Initialize the repository:
    ```bash
    cd ~/Qt/6.7.2/src/
    ./init-repository
    ```
  * Configure build settings:
    ```bash
    cd ~/Qt/6.7.2/
    mkdir build-static
    cd build-static
    ../src/configure -static -no-feature-accessibility -release -opensource -prefix ~/Qt/6.7.2/install-static/ -nomake examples -nomake tests -DQT_FEATURE_xcb=ON -DFEATURE_xcb_xlib=ON -DQT_FEATURE_xlib=ON
    ```
  * Compile Qt (give it 10x more of what it took you to get here)
    ```bash
    cmake --build . --parallel
    ```
> [!TIP]
> You can usually restart this building process from the point it was interrupted by running the same command.

  * If you have somehow made it here, you can proceed to install Qt:
    ```bash
    cmake --install .
    ```
3. Install QtCreator IDE:
   ```bash
   sudo apt install qtcreator -y
   ```
4. Configure QtCreator to use the Qt 6.7.2 library we have just compiled and installed.
5. Compile KGroundControl.

> [!TIP]
> If you want to deploy KGC to RPi or similar devices, I would recommend [cross-compiling](https://wiki.qt.io/Cross-Compile_Qt_6_for_Raspberry_Pi) everything using a powerful host machine
> instead of attempting to compile on the target device.  


# Optitrack Motion Capture System
Although KGroundControl is cross-platform and works on different operating systems, watch out for the OS-specific limitations.
Windows thread scheduling is not as accurate/consistent as in Linux. If you plan to use motion capture, make sure you are ok with <15ms 
scheduling precision truncation. I had to resort to running KGC in WSL2 (Ubuntu) to get more consistent streaming rates. 

# Contact
If you have any questions, please feel free to reach out to me, Yevhenii (Jack) Kovryzhenko, at yzk0058@auburn.edu.

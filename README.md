# KGroundControl (KGC) - Ground Control and Monitoring Software

# ![Logo](resources/Images/Logo/KLogo.png)

## Hello there 👋

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

Check out my videos using KGC on [YouTube](https://www.youtube.com/playlist?list=PLgxIoIw6ONulUfYvzxfoZNM6QXrcRqKkV)!

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

Here is a quick example of compiling everything on the target Linux device:
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
> instead of attempting to dirrectly compile on the target device.

### With the cross-compiled static version of Qt (open-source)
> [!WARNING]
> This is a very advanced topic, and you must have prior experience in cross-compiling for embedded platforms.

> [!WARNING]
> This section is still a work in progress, I will update it once I figure out why this does not work as expected.

I am using an Arch Linux host machine for cross-compilation for RPi 5 target. Refer to the [official documentation](https://wiki.qt.io/Cross-Compile_Qt_6_for_Raspberry_Pi),
since it is far more detailed and you are probably not using Arch. For now, I will leave some of my notes on the process I have gone through.

1. Prepare the target:
    1. Install the official Raspbian image for the RPi 5 and do all the setup you need. I recommend using a wired ethernet connection and work remotely though ssh.
         If you are actually setting up a brand new OS, don't forget to do a sull system upgrade first:
         ```bash
          sudo apt update
          sudo apt full-upgrade
          sudo reboot
        ```
    2. Regardless, make sure the system is up to date:
         ```bash
         sudo apt-get update && sudo apt-get upgrade -y
         sudo reboot    
        ```
    3. Install all the required libraries.
         ```bash
         sudo apt-get install -y libboost-all-dev libudev-dev libinput-dev libts-dev libmtdev-dev libjpeg-dev libfontconfig1-dev libssl-dev libdbus-1-dev libglib2.0-dev libxkbcommon-dev libegl1-mesa-dev libgbm-dev libgles2-mesa-dev mesa-common-dev libasound2-dev libpulse-dev gstreamer1.0-omx libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev  gstreamer1.0-alsa libvpx-dev libsrtp2-dev libsnappy-dev libnss3-dev "^libxcb.*" flex bison libxslt-dev ruby gperf libbz2-dev libcups2-dev libatkmm-1.6-dev libxi6 libxcomposite1 libfreetype6-dev libicu-dev libsqlite3-dev libxslt1-dev libavcodec-dev libavformat-dev libswscale-dev libx11-dev freetds-dev libpq-dev libiodbc2-dev firebird-dev  libxext-dev libxcb1 libxcb1-dev libx11-xcb1 libx11-xcb-dev libxcb-keysyms1 libxcb-keysyms1-dev libxcb-image0 libxcb-image0-dev libxcb-shm0 libxcb-shm0-dev libxcb-icccm4 libxcb-icccm4-dev libxcb-sync1 libxcb-sync-dev libxcb-render-util0 libxcb-render-util0-dev libxcb-xfixes0-dev libxrender-dev libxcb-shape0-dev libxcb-randr0-dev libxcb-glx0-dev libxi-dev libdrm-dev libxcb-xinerama0 libxcb-xinerama0-dev libatspi2.0-dev libxcursor-dev libxcomposite-dev libxdamage-dev libxss-dev libxtst-dev libpci-dev libcap-dev libxrandr-dev libdirectfb-dev libaudio-dev libxkbcommon-x11-dev rsync
         ```
2. Prepare the host:
    1. Compile Qt 6.7 from source for the host.
          1. Make sure you have all the dependencies:
                * Get required packages:
                ```bash
                 sudo pacman -S make cmake clang gcc git ninja bison python3 gperf pkg-config llvm rsync symlinks
                ```
          2. [Compile Qt 6.7 from source](https://doc.qt.io/qt-6/linux-building.html):
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
                ../src/configure -static -no-feature-accessibility -debug_and_release -opensource -prefix ~/Qt/6.7.2/install-static/
                ```
                * Compile Qt
                ```bash
                cmake --build . --parallel
                ```          
                * Install Qt:
                ```bash
                cmake --install .
                ```
          3. Install QtCreator IDE:
             ```bash
             sudo pacman -S qtcreator
             ```
          4. Run and configure QtCreator to use the Qt 6.7.2 library we have just compiled and installed.
          5. Compile KGroundControl.

    2. Create a sysroot directly on your host and synchronize it with the target.
         * Choose the storrage location. I used my second hard drive for this:
             ```bash
               cd /run/media/jack/HDD/
               mkdir RPi5
               cd RPi5
             ```
         * Create sysroot:
             ```bash
               mkdir sysroot
               cd sysroot
               mkdir lib usr opt
             ```
         * Start copying all the required files from RPi (username is **pi** and **raspberrypi.local** is the ip address for me):
             ```bash
              rsync -avzS --rsync-path="rsync" --delete pi@raspberrypi.local:/lib/* ./lib
              rsync -avzS --rsync-path="rsync" --delete pi@raspberrypi.local:/usr/include/* ./usr/include
              rsync -avzS --rsync-path="rsync" --delete pi@raspberrypi.local:/usr/lib/* ./usr/lib
              rsync -avzS --rsync-path="rsync" --delete pi@raspberrypi.local:/opt/vc rpi-sysroot/opt/vc
             ```
         * I did not have it, but you may need to also copy the following folder from RPi:
             ```bash
              rsync -avzS --rsync-path="rsync" --delete <pi_username>@<pi_ip_address>:/opt/vc rpi-sysroot/opt/vc
             ```
         * Don't forget to update the symbolic links:
             ```bash
               cd /run/media/jack/HDD/RPi
               symlinks -rc sysroot
             ```           
            
    3. Install the toolchain:
          ```bash
            sudo pacman -S aarch64-linux-gnu-gcc
          ```
    4. Setup build and install directories for the cross-compiled version of Qt.
         * I chose to keep all source and build files in the same top-level directory I created earlier. Let's create build directory:
         ```bash
           cd /run/media/jack/HDD/RPi
           mkdir Qt-raspi  Qt-raspi-build
         ``` 
    5. Configure toolchain CMake file:
         * Open a new file:
           ```bash
              nano toolchain.cmake
           ```
         * Configure toolchain for your case:
           ```
            cmake_minimum_required(VERSION 3.18)
            include_guard(GLOBAL)
            
            set(CMAKE_SYSTEM_NAME Linux)
            set(CMAKE_SYSTEM_PROCESSOR arm)
            
            set(TARGET_SYSROOT /run/media/jack/HDD/RPi5/sysroot)
            set(CMAKE_SYSROOT ${TARGET_SYSROOT})
            
            set(ENV{PKG_CONFIG_PATH} $PKG_CONFIG_PATH:/usr/lib/aarch64-linux-gnu/pkgconfig)
            set(ENV{PKG_CONFIG_LIBDIR} /usr/lib/pkgconfig:/usr/share/pkgconfig/:${TARGET_SYSROOT}/usr/lib/aarch64-linux-gnu/pkgconfig:${TARGET_SYSROOT}/usr/lib/pkgconfig)
            set(ENV{PKG_CONFIG_SYSROOT_DIR} ${CMAKE_SYSROOT})
            
            # if you use other version of gcc and g++, you must change the following variables
            set(CMAKE_C_COMPILER /usr/bin/aarch64-linux-gnu-gcc)
            set(CMAKE_CXX_COMPILER /usr/bin/aarch64-linux-gnu-g++)
            
            set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -I${TARGET_SYSROOT}/usr/include")
            set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS}")
            
            set(QT_COMPILER_FLAGS "-march=armv8-a")
            set(QT_COMPILER_FLAGS_RELEASE "-O2 -pipe")
            set(QT_LINKER_FLAGS "-Wl,-O1 -Wl,--hash-style=gnu -Wl,--as-needed")
            
            set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
            set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
            set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
            set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
            set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
            set(CMAKE_BUILD_RPATH ${TARGET_SYSROOT})
            
            
            include(CMakeInitializeConfigs)
            
            function(cmake_initialize_per_config_variable _PREFIX _DOCSTRING)
              if (_PREFIX MATCHES "CMAKE_(C|CXX|ASM)_FLAGS")
                set(CMAKE_${CMAKE_MATCH_1}_FLAGS_INIT "${QT_COMPILER_FLAGS}")
            
                foreach (config DEBUG RELEASE MINSIZEREL RELWITHDEBINFO)
                  if (DEFINED QT_COMPILER_FLAGS_${config})
                    set(CMAKE_${CMAKE_MATCH_1}_FLAGS_${config}_INIT "${QT_COMPILER_FLAGS_${config}}")
                  endif()
                endforeach()
              endif()
            
            
              if (_PREFIX MATCHES "CMAKE_(SHARED|MODULE|EXE)_LINKER_FLAGS")
                foreach (config SHARED MODULE EXE)
                  set(CMAKE_${config}_LINKER_FLAGS_INIT "${QT_LINKER_FLAGS}")
                endforeach()
              endif()
            
              _cmake_initialize_per_config_variable(${ARGV})
            endfunction()
            
            set(XCB_PATH_VARIABLE ${TARGET_SYSROOT})
            
            set(GL_INC_DIR ${TARGET_SYSROOT}/usr/include)
            set(GL_LIB_DIR ${TARGET_SYSROOT}:${TARGET_SYSROOT}/usr/lib/aarch64-linux-gnu/:${TARGET_SYSROOT}/usr:${TARGET_SYSROOT}/usr/lib)
            
            set(EGL_INCLUDE_DIR ${GL_INC_DIR})
            set(EGL_LIBRARY ${XCB_PATH_VARIABLE}/usr/lib/aarch64-linux-gnu/libEGL.so)
            
            set(OPENGL_INCLUDE_DIR ${GL_INC_DIR})
            set(OPENGL_opengl_LIBRARY ${XCB_PATH_VARIABLE}/usr/lib/aarch64-linux-gnu/libOpenGL.so)
           
            set(GLESv2_INCLUDE_DIR ${GL_INC_DIR})
            set(GLESv2_LIBRARY ${XCB_PATH_VARIABLE}/usr/lib/aarch64-linux-gnu/libGLESv2.so)
            
            set(gbm_INCLUDE_DIR ${GL_INC_DIR})
            set(gbm_LIBRARY ${XCB_PATH_VARIABLE}/usr/lib/aarch64-linux-gnu/libgbm.so)
            
            set(Libdrm_INCLUDE_DIR ${GL_INC_DIR})
            set(Libdrm_LIBRARY ${XCB_PATH_VARIABLE}/usr/lib/aarch64-linux-gnu/libdrm.so)
            
            set(XCB_XCB_INCLUDE_DIR ${GL_INC_DIR})
            set(XCB_XCB_LIBRARY ${XCB_PATH_VARIABLE}/usr/lib/aarch64-linux-gnu/libxcb.so)
           ```
    6. Configure your host Qt installation for the cross-compiling the RPi-Qt:
         ```bash
           cd Qt-raspi-build
            /home/jack/Qt/v6.7.2/src/configure -static -release -opengl es2 -nomake examples -nomake tests -qt-host-path /home/jack/Qt/v6.7.2/build/ -extprefix /run/media/jack/HDD/RPi5/Qt-raspi -prefix $HOME/Qt/6.7.2-RPi5-Static -device linux-rasp-pi4-aarch64 -device-option CROSS_COMPILE=aarch64-linux-gnu- -DCMAKE_TOOLCHAIN_FILE=/run/media/jack/HDD/RPi5/toolchain.cmake -DQT_FEATURE_xcb=ON -DFEATURE_xcb_xlib=ON -DQT_FEATURE_xlib=ON
         ```
    7. Compile:
        ```bash
          cmake --build . --parallel
        ```
    8. Install:
        ```bash
          cmake --install .
        ```
3. Compile KGroundControl:
    1. Go to where you have cloned KGroundControl source and create a build directory:
        ```bash
        cd /run/media/jack/HDD/Github/KGroundControl
        mkdir build/Qt6.7.2-Release-Static-RPi
        cd build/Qt6.7.2-Release-Static-RPi
       ```
    2. Run CMake using the RPi-Qt:
        ```bash
        /run/media/jack/HDD/RPi5/Qt-raspi/bin/qt-cmake ../../CMakeLists.txt
       ```
    3. Compile:
       ```bash           
        cmake --build . --parallel
       ```
4. If you have compiled everything statically, you can not copy the executable to the RPi5 and test it.
   

# Optitrack Motion Capture System
Although KGroundControl is cross-platform and works on different operating systems, watch out for the OS-specific limitations.
Windows thread scheduling is not as accurate/consistent as in Linux. If you plan to use motion capture, make sure you are ok with <15ms 
scheduling precision truncation. I had to resort to running KGC in WSL2 (Ubuntu) to get more consistent streaming rates. 

# Contact
If you have any questions, please feel free to contact me, Yevhenii (Jack) Kovryzhenko, at yzk0058@auburn.edu.

# Credit
This work started during my Ph.D. at [ACELAB](https://etaheri0.wixsite.com/acelabauburnuni) at Auburn University, under the supervision of Dr. Ehsan Taheri.
Part of this work has been used during my participation in the STTR Phase II project with Transcend Air Co. to support the ground station and related activities.
For more details, check out my [PX4 Simulink IO Framework](https://github.com/YevheniiKovryzhenko/PX4_SIMULINK_IO_Framework.git) and [RRTV TiltWing](https://github.com/YevheniiKovryzhenko/RRTV_TiltWing.git) repositories that
were all part of this project. 

I am still in the process of publishing journal papers that have directly used this work, so I will keep this section actively updated. Feel free to credit [me](https://scholar.google.com/citations?user=P812qiUAAAAJ&hl=en) by citing any of my relevant works.

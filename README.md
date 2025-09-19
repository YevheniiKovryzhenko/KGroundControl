# KGroundControl (KGC) - Ground Control and Monitoring Software

<p align="center">
  <img src="resources/Images/Logo/KGC_Logo.png" alt="KGroundControl logo" width="220">
</p>

## Hello there 👋

KGroundControl is cross-platform ground control station software I developed for research purposes.
My initial goal was to combine multiple ground control stations I had previously written
and implement a graphical interface. I found QGroundControl insufficient for my purposes, so KGroundControl
was originally written as a support application that works together with QGroundControl.
KGroundControl is standalone and supports Mavlink communication via serial and UDP,
which can relay/bridge data streams between a single port and multiple ports. Data forwarding and the ability
to communicate with multiple devices through a single port were major requirements,
so the app was written from scratch with that in mind

Additionally, it supports the Optitrack Motion Capture System and can stream data to multiple UAVs.

Check out my videos using KGC on [YouTube](https://www.youtube.com/playlist?list=PLgxIoIw6ONulUfYvzxfoZNM6QXrcRqKkV)!

## Cross-Platform Features
KGroundControl is designed to work across multiple platforms with minimal configuration:

* Automatic Qt detection: The build system detects Qt installations in common locations.
* Cross-platform deployment: Uses CMake-based deployment on Windows and Linux.
* [To be completed] SDL2 integration: Cross-platform multimedia support for joysticks and other input devices.
* MAVLink protocol: Platform-independent drone communication protocol

## Tested Architectures and Platforms
* x86-64:
  - Windows 10/11
  - Ubuntu 20.04+
* ARM64:
  - Raspbian (build from source)

## Installation
### Binary
You can use the standalone binary (executable) without installation. The most recent version is always available through the Releases tab.
It is built with statically linked Qt and should be the most optimized version performance-wise, but it may take longer to start. On Linux,
you may still need some external libraries depending on your distribution.
The executable may need to be recompiled for your specific platform (see “Compiling from source”).

### Compiling from source
Requirements:
* Qt 6.2+
* CMake 3.18+
* C++20-compatible compiler

### Windows
1. Install Qt 6.2+ using the [Qt online installer](https://doc.qt.io/qt-6/get-and-install-qt.html)
2. Install Visual Studio 2019/2022 or MinGW
3. Install CMake
4. Clone the repository and navigate to the project directory
5. Configure the build:
   ```bash
   mkdir build
   cd build
   cmake .. -DCMAKE_PREFIX_PATH="C:/Qt/6.9.2/msvc2019_64"  # Adjust path as needed
   ```
6. Build the project:
   ```bash
   cmake --build . --config Release
   ```

### Linux
#### With [Qt online installer](https://doc.qt.io/qt-6/get-and-install-qt.html) (Open Source or Commercial)
This is usually the easiest option. Install Qt using the official online installer, get the KGroundControl source code,
and compile. This will not produce a fully standalone executable; you will need to use a deployment tool (like cqtdeployer),
create your own installer, or compile a static version of Qt from source.

#### With the static version of Qt (open-source)
> [!NOTE]
> Keep in mind that this is time-consuming and may require restarts if something is misconfigured.
> Ensure you have plenty of disk space (at least 128 GB, preferably 256 GB).

Here is a quick example of compiling everything on the target Linux device:
1. Make sure you have all the dependencies:
  * Update system:
    ```bash
    sudo apt-get update && sudo apt-get upgrade -y    
    ```
  * Get required packages:
    ```bash
    sudo apt-get install -y make cmake build-essential libclang-dev clang ninja-build gcc git bison python3 gperf pkg-config libfontconfig1-dev libfreetype6-dev libx11-dev libx11-xcb-dev libxext-dev libxfixes-dev libxi-dev libxrender-dev libxcb1-dev libxcb-glx0-dev libxcb-keysyms1-dev libxcb-image0-dev libxcb-shm0-dev libxcb-icccm4-dev libxcb-sync-dev libxcb-xfixes0-dev libxcb-shape0-dev libxcb-randr0-dev libxcb-render-util0-dev libxcb-util-dev libxcb-xinerama0-dev libxcb-xkb-dev libxkbcommon-dev libxkbcommon-x11-dev libatspi2.0-dev libgl1-mesa-dev libglu1-mesa-dev freeglut3-dev libsdl2-dev g++ pkg-config libgl1-mesa-dev libxcb*-dev libfontconfig1-dev libxkbcommon-x11-dev libgtk-3-dev
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
  * Compile Qt (this can take a long time)
    ```bash
    cmake --build . --parallel
    ```
> [!TIP]
> You can usually restart the build from where it stopped by running the same command again.

  * If you have somehow made it here, you can proceed to install Qt:
    ```bash
    cmake --install .
    ```
3. Install Qt Creator IDE:
   ```bash
   sudo apt install qtcreator -y
   ```
4. Configure Qt Creator to use the Qt 6.7.2 library we have just compiled and installed.
5. Compile KGroundControl.

> [!TIP]
> If you want to deploy KGC to RPi or similar devices, cross-compile everything using a powerful host machine
> instead of attempting to directly compile on the target device.

### With the cross-compiled static version of Qt (open-source)
> [!WARNING]
> This is an advanced topic; prior cross-compilation experience is recommended.

> [!WARNING]
> This section is still a work in progress; behavior may vary by setup.

I am using an Arch Linux host machine for cross-compilation for RPi 5 target. Refer to the [official documentation](https://wiki.qt.io/Cross-Compile_Qt_6_for_Raspberry_Pi),
since it is far more detailed and you are probably not using Arch. For now, I will leave some of my notes on the process I have gone through.

1. Prepare the target:
    1. Install the official Raspbian image for the RPi 5 and do the initial setup. A wired Ethernet connection with SSH is recommended.
         If you are setting up a brand-new OS, don't forget to do a full system upgrade first:
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
          3. Install Qt Creator IDE:
             ```bash
             sudo pacman -S qtcreator
             ```
          4. Run and configure Qt Creator to use the Qt 6.7.2 library we have just compiled and installed.
          5. Compile KGroundControl.

    2. Create a sysroot directly on your host and synchronize it with the target.
         * Choose the storage location. For example:
             ```bash
               cd /run/media/jack/HDD/
               mkdir RPi5
               cd RPi5
             ```
         * Create the sysroot:
             ```bash
               mkdir sysroot
               cd sysroot
               mkdir -p lib usr opt
             ```
         * Copy required files from the RPi (username is pi and raspberrypi.local is the hostname here):
             ```bash
              rsync -avzS --rsync-path="rsync" --delete pi@raspberrypi.local:/lib/ ./lib
              rsync -avzS --rsync-path="rsync" --delete pi@raspberrypi.local:/usr/include/ ./usr/include
              rsync -avzS --rsync-path="rsync" --delete pi@raspberrypi.local:/usr/lib/ ./usr/lib
              rsync -avzS --rsync-path="rsync" --delete pi@raspberrypi.local:/opt/vc ./opt/vc
             ```
         * If needed, you may also copy the following folder from the RPi:
             ```bash
              rsync -avzS --rsync-path="rsync" --delete <pi_username>@<pi_ip_address>:/opt/vc ./opt/vc
             ```
         * Update symbolic links:
             ```bash
               cd /run/media/jack/HDD/RPi5
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
         * Configure toolchain for your case:
           ```
            cmake_minimum_required(VERSION 3.18)
            include_guard(GLOBAL)

            set(CMAKE_SYSTEM_NAME Linux)
            set(CMAKE_SYSTEM_PROCESSOR aarch64)

            set(TARGET_SYSROOT /run/media/jack/HDD/RPi5/sysroot)
            set(CMAKE_SYSROOT ${TARGET_SYSROOT})

            set(ENV{PKG_CONFIG_PATH} $PKG_CONFIG_PATH:/usr/lib/aarch64-linux-gnu/pkgconfig)
            set(ENV{PKG_CONFIG_LIBDIR} /usr/lib/pkgconfig:/usr/share/pkgconfig/:${TARGET_SYSROOT}/usr/lib/aarch64-linux-gnu/pkgconfig:${TARGET_SYSROOT}/usr/lib/pkgconfig)
            set(ENV{PKG_CONFIG_SYSROOT_DIR} ${CMAKE_SYSROOT})

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
    6. Configure your host Qt installation for cross-compiling the RPi Qt:
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
4. If you have compiled everything statically, you can now copy the executable to the RPi5 and test it.

## Optitrack Motion Capture System
KGroundControl is designed to work with Optitrack Motive (v2.3.0) data streaming application.

## Contact
If you have any questions, please feel free to contact me, Yevhenii (Jack) Kovryzhenko, at yzk0058@auburn.edu.

## Credit
This work started during my Ph.D. at [ACELAB](https://etaheri0.wixsite.com/acelabauburnuni) at Auburn University, under the supervision of Dr. Ehsan Taheri.
Part of this work has been used during my participation in the STTR Phase II project with Transcend Air Co. to support the ground station and related activities.
For more details, check out my [PX4 Simulink IO Framework](https://github.com/YevheniiKovryzhenko/PX4_SIMULINK_IO_Framework.git) and [RRTV TiltWing](https://github.com/YevheniiKovryzhenko/RRTV_TiltWing.git) repositories that
were all part of this project. 

I am still in the process of publishing journal papers that have directly used this work, so I will keep this section actively updated. Feel free to credit [me](https://scholar.google.com/citations?user=P812qiUAAAAJ&hl=en) by citing any of my relevant works.

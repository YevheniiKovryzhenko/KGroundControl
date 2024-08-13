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
  - Windows - installer available
  - Ubuntu - installer available
  - Arch   - must compile from source
* ARM64:
  - Raspbian - must compile from source

# Installation
## With Installer
Use one of the installers, which are available under the releases tab. All instructions are 
included as part of the installation walkthrough. 



## Compiling from source
Requirements: 
* Qt 6.2+ (all releases use Qt 6.7)
* For installer creation, you should install [cqtdeployer](https://github.com/QuasarApp/CQtDeployer)


# Optitrack Motion Capture System
Although KGroundControl is cross-platform and works on different operating systems, watch out for the OS-specific limitations.
Windows thread scheduling is not as accurate/consistent as in Linux. If you plan to use motion capture, make sure you are ok with <15ms 
scheduling precision truncation. I had to resort to running KGC in WSL2 (Ubuntu) to get more consistent streaming rates. 

# Contact
If you have any questions, please feel free to reach out to me, Yevhenii (Jack) Kovryzhenko, at yzk0058@auburn.edu.

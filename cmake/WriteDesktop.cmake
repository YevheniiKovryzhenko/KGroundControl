# Write desktop file
file(WRITE ${DESKTOP_FILE} "[Desktop Entry]\n")
file(APPEND ${DESKTOP_FILE} "Name=KGroundControl\n")

# Check if this is for AppImage (AppDir) or DEB package
if(${DESKTOP_FILE} MATCHES "AppDir")
    file(APPEND ${DESKTOP_FILE} "Exec=KGroundControl\n")
else()
    file(APPEND ${DESKTOP_FILE} "Exec=/usr/bin/KGroundControl\n")
endif()

file(APPEND ${DESKTOP_FILE} "Icon=KGroundControl\n")
file(APPEND ${DESKTOP_FILE} "Type=Application\n")
file(APPEND ${DESKTOP_FILE} "Categories=Utility;\n")
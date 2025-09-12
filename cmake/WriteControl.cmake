# Write DEB control file
file(WRITE ${CONTROL_FILE} "Package: kgroundcontrol\n")
file(APPEND ${CONTROL_FILE} "Version: ${PROJECT_VERSION}\n")
file(APPEND ${CONTROL_FILE} "Architecture: amd64\n")
file(APPEND ${CONTROL_FILE} "Maintainer: Your Name <your.email@example.com>\n")
file(APPEND ${CONTROL_FILE} "Description: Ground Control Application\n")
file(APPEND ${CONTROL_FILE} "Depends: libc6 (>= 2.15)\n")
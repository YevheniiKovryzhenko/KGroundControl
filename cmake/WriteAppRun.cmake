# Write AppRun script for AppImage
file(WRITE ${APP_RUN_FILE} "#!/bin/bash\n")
file(APPEND ${APP_RUN_FILE} "cd \"\$(dirname \"\$0\")\n")
file(APPEND ${APP_RUN_FILE} "exec ./usr/bin/KGroundControl \"\$@\"\n")
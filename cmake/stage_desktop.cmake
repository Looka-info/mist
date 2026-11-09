# apps/desktop/stage_desktop.cmake
# Stages mist-desktop.exe from whichever target directory Cargo placed the artifact.

if(DEFINED SOURCE_1 AND EXISTS "${SOURCE_1}")
    file(COPY_FILE "${SOURCE_1}" "${DESTINATION}" ONLY_IF_DIFFERENT)
elseif(DEFINED SOURCE_2 AND EXISTS "${SOURCE_2}")
    file(COPY_FILE "${SOURCE_2}" "${DESTINATION}" ONLY_IF_DIFFERENT)
elseif(DEFINED SOURCE_3 AND EXISTS "${SOURCE_3}")
    file(COPY_FILE "${SOURCE_3}" "${DESTINATION}" ONLY_IF_DIFFERENT)
else()
    message(FATAL_ERROR "Could not find mist-desktop.exe in candidates:\n  SOURCE_1: ${SOURCE_1}\n  SOURCE_2: ${SOURCE_2}\n  SOURCE_3: ${SOURCE_3}")
endif()

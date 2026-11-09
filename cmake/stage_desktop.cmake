# cmake/stage_desktop.cmake
# Stages mist-desktop.exe and mist-signaling.exe from whichever target directory Cargo placed the artifacts.

if(DEFINED SOURCE_1 AND EXISTS "${SOURCE_1}")
    file(COPY_FILE "${SOURCE_1}" "${DESTINATION}" ONLY_IF_DIFFERENT)
elseif(DEFINED SOURCE_2 AND EXISTS "${SOURCE_2}")
    file(COPY_FILE "${SOURCE_2}" "${DESTINATION}" ONLY_IF_DIFFERENT)
elseif(DEFINED SOURCE_3 AND EXISTS "${SOURCE_3}")
    file(COPY_FILE "${SOURCE_3}" "${DESTINATION}" ONLY_IF_DIFFERENT)
else()
    message(FATAL_ERROR "Could not find mist-desktop.exe in candidates:\n  SOURCE_1: ${SOURCE_1}\n  SOURCE_2: ${SOURCE_2}\n  SOURCE_3: ${SOURCE_3}")
endif()

# Ensure bin/mist-signaling.exe is staged for the installer payload
if(DEFINED SOURCE_DIR AND DEFINED RUST_TARGET_SUBDIR)
    set(SIGNALING_CANDIDATES
        "${SOURCE_DIR}/target/${RUST_TARGET_SUBDIR}/mist-signaling.exe"
        "D:/rust/target/${RUST_TARGET_SUBDIR}/mist-signaling.exe"
        "${SOURCE_DIR}/services/signaling/target/${RUST_TARGET_SUBDIR}/mist-signaling.exe"
        "${SOURCE_DIR}/bin/mist-signaling.exe"
    )
    foreach(CAND ${SIGNALING_CANDIDATES})
        if(EXISTS "${CAND}")
            file(MAKE_DIRECTORY "${SOURCE_DIR}/bin")
            file(COPY_FILE "${CAND}" "${SOURCE_DIR}/bin/mist-signaling.exe" ONLY_IF_DIFFERENT)
            break()
        endif()
    endforeach()
endif()

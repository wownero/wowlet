# Generates src/assets_wownerod.qrc embedding the wownerod binary from WOWNEROD_DIR, so wowlet ships
# as a single self-contained file with its node inside. Mirror of TorQrcGenerator.cmake. When
# WOWNEROD_DIR is OFF the generated qrc is empty (no embedded daemon) and HAS_WOWNEROD_BIN is unset.
set(QRC_LIST)

if (WOWNEROD_DIR)
    FILE(GLOB WOWNEROD_FILES LIST_DIRECTORIES false ${WOWNEROD_DIR}/*)

    foreach(FILE ${WOWNEROD_FILES})
        cmake_path(GET FILE FILENAME FILE_REL)
        list(APPEND QRC_LIST "        <file>assets/wownerod/${FILE_REL}</file>")

        if (FILE_REL STREQUAL "wownerod" OR FILE_REL STREQUAL "wownerod.exe")
            set(WOWNEROD_BIN_FOUND 1)
        endif()
    endforeach()

    if (NOT WOWNEROD_BIN_FOUND)
        message(FATAL_ERROR "WOWNEROD_DIR was specified but the wownerod binary could not be found")
    endif()
endif()

list(JOIN QRC_LIST "\n" QRC_DATA)
configure_file("cmake/assets_wownerod.qrc" "${CMAKE_CURRENT_SOURCE_DIR}/src/assets_wownerod.qrc")

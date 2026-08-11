if (NOT DEFINED FSORG_SOURCE_DIR)
    message(FATAL_ERROR "FSORG_SOURCE_DIR must be defined.")
endif ()

set(CORE_TREES "src/domain" "src/application" "src/viewmodel")

set(WIN32_HEADERS "windows.h" "shlobj.h" "shlobj_core.h" "tlhelp32.h" "dwmapi.h" "winioctl.h" "objbase.h")

set(OFFENCES "")
set(SCANNED "")

foreach (TREE IN LISTS CORE_TREES)
    file(GLOB_RECURSE FOUND "${FSORG_SOURCE_DIR}/${TREE}/*.h" "${FSORG_SOURCE_DIR}/${TREE}/*.cpp")

    foreach (FOUND_FILE IN LISTS FOUND)
        file(RELATIVE_PATH SHOWN "${FSORG_SOURCE_DIR}" "${FOUND_FILE}")
        list(APPEND SCANNED "${SHOWN}")
    endforeach ()
endforeach ()

list(REMOVE_DUPLICATES SCANNED)

if (NOT SCANNED)
    message(FATAL_ERROR "Scanned nothing: the three core trees are gone, or FSORG_SOURCE_DIR points elsewhere.")
endif ()

foreach (SCANNED_FILE IN LISTS SCANNED)
    file(STRINGS "${FSORG_SOURCE_DIR}/${SCANNED_FILE}" LINES ENCODING UTF-8)

    foreach (LINE IN LISTS LINES)
        foreach (WIN32_HEADER IN LISTS WIN32_HEADERS)
            if (LINE MATCHES "^[ \t]*#[ \t]*include[ \t]*<${WIN32_HEADER}>")
                list(APPEND OFFENCES "  ${SCANNED_FILE}: includes <${WIN32_HEADER}>")
            endif ()
        endforeach ()
    endforeach ()
endforeach ()

if (OFFENCES)
    list(REMOVE_DUPLICATES OFFENCES)
    string(REPLACE ";" "\n" REPORT "${OFFENCES}")
    message(FATAL_ERROR
            "The core reached for win32, and the core is domain, application and viewmodel.\n"
            "Put the win32 call behind a port and let an adapter in src/infrastructure make it.\n"
            "${REPORT}")
endif ()

list(LENGTH SCANNED HOW_MANY)
message(STATUS "No win32 header inside the ${HOW_MANY} files of the domain, the application and the viewmodel.")

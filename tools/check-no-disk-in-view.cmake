if (NOT DEFINED FSORG_SOURCE_DIR)
    message(FATAL_ERROR "FSORG_SOURCE_DIR must be defined.")
endif ()

set(SEARCHED_DIRECTORIES
        "${FSORG_SOURCE_DIR}/src/view"
        "${FSORG_SOURCE_DIR}/src/viewmodel"
)

set(ASKS_THE_STANDARD_LIBRARY
        "std::filesystem::(recursive_directory_iterator|directory_iterator|directory_entry|symlink_status|status|exists|is_regular_file|is_directory|is_symlink|is_empty|file_size|last_write_time|space|weakly_canonical|canonical|absolute|equivalent|read_symlink|hard_link_count|current_path|temp_directory_path)[^A-Za-z0-9_]")

set(ORDERS_THE_STANDARD_LIBRARY
        "std::filesystem::(create_directory_symlink|create_directories|create_directory|create_symlink|create_hard_link|copy_file|copy|rename|remove_all|remove|resize_file|permissions)[^A-Za-z0-9_]")

set(ASKS_QT "(QFileInfo|QFileSystemModel|QFileSystemWatcher|QStorageInfo|QFile[^A-Za-z0-9_])")

set(NAMES_A_QT_FOLDER "QDir")

set(ONLY_SPELLS_WITH_A_QT_FOLDER
        "QDir::(fromNativeSeparators|toNativeSeparators|listSeparator|separator|isAbsolutePath|isRelativePath|cleanPath)")

set(OFFENCES "")

foreach (DIRECTORY IN LISTS SEARCHED_DIRECTORIES)
    file(GLOB_RECURSE WIDGET_SOURCES "${DIRECTORY}/*.cpp" "${DIRECTORY}/*.h")

    foreach (WIDGET_SOURCE IN LISTS WIDGET_SOURCES)
        file(STRINGS "${WIDGET_SOURCE}" LINES ENCODING UTF-8)
        file(RELATIVE_PATH SHOWN "${FSORG_SOURCE_DIR}" "${WIDGET_SOURCE}")

        set(LINE_NUMBER 0)
        foreach (LINE IN LISTS LINES)
            math(EXPR LINE_NUMBER "${LINE_NUMBER} + 1")

            set(PADDED "${LINE} ")

            set(GUILTY OFF)

            if (PADDED MATCHES "${ASKS_THE_STANDARD_LIBRARY}" OR PADDED MATCHES "${ORDERS_THE_STANDARD_LIBRARY}"
                    OR PADDED MATCHES "${ASKS_QT}")
                set(GUILTY ON)
            endif ()

            if (PADDED MATCHES "${NAMES_A_QT_FOLDER}" AND NOT PADDED MATCHES "${ONLY_SPELLS_WITH_A_QT_FOLDER}")
                set(GUILTY ON)
            endif ()

            if (GUILTY)
                list(APPEND OFFENCES "  ${SHOWN}:${LINE_NUMBER}: ${LINE}")
            endif ()
        endforeach ()
    endforeach ()
endforeach ()

if (OFFENCES)
    string(REPLACE ";" "\n" REPORT "${OFFENCES}")
    message(FATAL_ERROR
            "A screen or a view model is interrogating the file system itself.\n"
            "The presentation layer reaches disk through a port and nothing else: FilesystemProbe answers\n"
            "existence, kind, free space and last write, CatalogScanner lists what is installed, and\n"
            "FileOperations is the one that writes.\n"
            "A direct call makes the screen untestable without a real disk, since the doubles model the ports\n"
            "and cannot model the disk, and it puts a call of unbounded cost inside a repaint.\n"
            "Handing a folder to the shell with QDesktopServices::openUrl asks disk nothing and stays legal,\n"
            "and so does std::filesystem::path as a type: this reports the calls, not the vocabulary.\n"
            "${REPORT}")
endif ()

message(STATUS "No screen or view model interrogates the file system.")

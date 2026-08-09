if (NOT DEFINED FSORG_SOURCE_DIR)
    message(FATAL_ERROR "FSORG_SOURCE_DIR must be defined.")
endif ()

set(SEARCHED_DIRECTORIES
        "${FSORG_SOURCE_DIR}/src/domain"
        "${FSORG_SOURCE_DIR}/src/application"
)

set(WHERE_THE_PAIR_LIVES "/src/domain/support/PathUtils.h")

set(READS_A_PATH_AS_TEXT "\\.(generic_)?(u8)?string[ \t]*\\(")
set(BUILDS_A_PATH_FROM_TEXT "std::filesystem::path[ \t]*\\([^)]")
set(BRACES_A_PATH_FROM_TEXT "std::filesystem::path[ \t]*\\{[^}]")
set(JOINS_A_BARE_NAME "[^/]/[ \t]+[A-Za-z_][A-Za-z0-9_.]*[ \t]*[;,)]")
set(JOINS_A_CALL "[^/]/[ \t]+[A-Za-z_][A-Za-z0-9_.]*[ \t]*\\(")
set(JOINS_A_KNOWN_CONSTANT "/[ \t]+k[A-Z]")
set(JOINS_SOMETHING_ALREADY_A_PATH
        "/[ \t]+([A-Za-z0-9_.]*\\.)?(PathFromUtf8|PathUnder|TailBelow|filename|parent_path|stem|lexically_relative)[ \t]*\\(")

set(OFFENCES "")

foreach (DIRECTORY IN LISTS SEARCHED_DIRECTORIES)
    file(GLOB_RECURSE PORTABLE_SOURCES "${DIRECTORY}/*.cpp" "${DIRECTORY}/*.h")

    foreach (PORTABLE_SOURCE IN LISTS PORTABLE_SOURCES)
        if (PORTABLE_SOURCE MATCHES "${WHERE_THE_PAIR_LIVES}")
            continue()
        endif ()

        file(STRINGS "${PORTABLE_SOURCE}" LINES)

        set(LINE_NUMBER 0)
        foreach (LINE IN LISTS LINES)
            math(EXPR LINE_NUMBER "${LINE_NUMBER} + 1")

            set(GUILTY OFF)

            if (LINE MATCHES "${READS_A_PATH_AS_TEXT}" OR LINE MATCHES "${BUILDS_A_PATH_FROM_TEXT}"
                    OR LINE MATCHES "${BRACES_A_PATH_FROM_TEXT}")
                set(GUILTY ON)
            endif ()

            if (LINE MATCHES "${JOINS_A_BARE_NAME}" AND NOT LINE MATCHES "${JOINS_A_KNOWN_CONSTANT}")
                set(GUILTY ON)
            endif ()

            if (LINE MATCHES "${JOINS_A_CALL}" AND NOT LINE MATCHES "${JOINS_SOMETHING_ALREADY_A_PATH}")
                set(GUILTY ON)
            endif ()

            if (GUILTY)
                file(RELATIVE_PATH SHOWN "${FSORG_SOURCE_DIR}" "${PORTABLE_SOURCE}")
                list(APPEND OFFENCES "  ${SHOWN}:${LINE_NUMBER}: ${LINE}")
            endif ()
        endforeach ()
    endforeach ()
endforeach ()

if (OFFENCES)
    string(REPLACE ";" "\n" REPORT "${OFFENCES}")
    message(FATAL_ERROR
            "A path may not cross into text through the code page of the host.\n"
            "On MSVC, path::string() converts through the process code page and throws for a character\n"
            "that has no mapping in it; building a path from a narrow string, and joining one with /,\n"
            "convert the same way and mangle in silence instead of throwing.\n"
            "Read a path with AsUtf8 and build one with PathFromUtf8, both in domain/support/PathUtils.h.\n"
            "A join may only take a call that already answers a path: PathFromUtf8, filename, parent_path, stem.\n"
            "A new one of those belongs in this guard's list, which is what keeps the list honest.\n"
            "${REPORT}")
endif ()

message(STATUS "No path crosses into text through the code page of the host.")

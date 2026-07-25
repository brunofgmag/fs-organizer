if (NOT DEFINED FSORG_SOURCE_DIR)
    message(FATAL_ERROR "FSORG_SOURCE_DIR must be defined.")
endif ()

set(SEARCHED_DIRECTORIES
        "${FSORG_SOURCE_DIR}/src/view"
        "${FSORG_SOURCE_DIR}/src/viewmodel"
)

set(HEX_COLOR "#[0-9a-fA-F][0-9a-fA-F][0-9a-fA-F][0-9a-fA-F][0-9a-fA-F][0-9a-fA-F]")
set(NAMED_COLOR "Qt::(white|black|red|darkRed|green|darkGreen|blue|darkBlue|cyan|darkCyan|magenta|darkMagenta|yellow|darkYellow|gray|darkGray|lightGray|transparent)")
set(COLOR_TYPE "QColor[ \t]*\\(")

set(OFFENCES "")

foreach (DIRECTORY IN LISTS SEARCHED_DIRECTORIES)
    file(GLOB_RECURSE WIDGET_SOURCES "${DIRECTORY}/*.cpp" "${DIRECTORY}/*.h")

    foreach (WIDGET_SOURCE IN LISTS WIDGET_SOURCES)
        file(STRINGS "${WIDGET_SOURCE}" LINES)

        set(LINE_NUMBER 0)
        foreach (LINE IN LISTS LINES)
            math(EXPR LINE_NUMBER "${LINE_NUMBER} + 1")

            if (LINE MATCHES "${HEX_COLOR}" OR LINE MATCHES "${NAMED_COLOR}"
                    OR LINE MATCHES "${COLOR_TYPE}")
                file(RELATIVE_PATH SHOWN "${FSORG_SOURCE_DIR}" "${WIDGET_SOURCE}")
                list(APPEND OFFENCES "  ${SHOWN}:${LINE_NUMBER}: ${LINE}")
            endif ()
        endforeach ()
    endforeach ()
endforeach ()

if (OFFENCES)
    string(REPLACE ";" "\n" REPORT "${OFFENCES}")
    message(FATAL_ERROR
            "Literal colours are not allowed in widget or viewmodel code.\n"
            "Use a QPalette role instead, so the app follows the system theme (ADR-0001).\n"
            "${REPORT}")
endif ()

message(STATUS "No literal colours found in widget or viewmodel code.")

if (NOT DEFINED FSORG_SOURCE_DIR)
    message(FATAL_ERROR "FSORG_SOURCE_DIR must be defined.")
endif ()

set(SEARCHED_DIRECTORIES
        "${FSORG_SOURCE_DIR}/src/view"
        "${FSORG_SOURCE_DIR}/src/viewmodel"
)

set(OFFENCES "")

foreach (DIRECTORY IN LISTS SEARCHED_DIRECTORIES)
    file(GLOB_RECURSE WIDGET_SOURCES "${DIRECTORY}/*.cpp" "${DIRECTORY}/*.h")

    foreach (WIDGET_SOURCE IN LISTS WIDGET_SOURCES)
        file(STRINGS "${WIDGET_SOURCE}" LINES ENCODING UTF-8)

        set(LINE_NUMBER 0)
        foreach (LINE IN LISTS LINES)
            math(EXPR LINE_NUMBER "${LINE_NUMBER} + 1")

            if (LINE MATCHES "—" OR LINE MATCHES "–")
                file(RELATIVE_PATH SHOWN "${FSORG_SOURCE_DIR}" "${WIDGET_SOURCE}")
                list(APPEND OFFENCES "  ${SHOWN}:${LINE_NUMBER}: ${LINE}")
            endif ()
        endforeach ()
    endforeach ()
endforeach ()

if (OFFENCES)
    string(REPLACE ";" "\n" REPORT "${OFFENCES}")
    message(FATAL_ERROR
            "Em and en dashes are not allowed in interface text.\n"
            "This app separates with the middle dot, joins clauses with a comma or a full stop, "
            "and introduces an explanation with a colon.\n"
            "${REPORT}")
endif ()

message(STATUS "No em or en dashes found in interface text.")

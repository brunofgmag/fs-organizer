if (NOT DEFINED FSORG_SOURCE_DIR)
    message(FATAL_ERROR "FSORG_SOURCE_DIR must be defined.")
endif ()

set(SEARCHED_DIRECTORIES
        "${FSORG_SOURCE_DIR}/src/view"
        "${FSORG_SOURCE_DIR}/src/viewmodel"
)

set(THE_DISABLED_INK "(tones_?\\.disabled|%disabled%)")
set(A_DISABLING_CONDITION "(QPalette::Disabled|:disabled|[^A-Za-z_]live[^A-Za-z_])")
set(A_PAINTED_INK "color:")
set(THE_SUBSTITUTION_TABLE "QStringLiteral\\(\"%")

set(WORN "")
set(SWAPPED "")

foreach (DIRECTORY IN LISTS SEARCHED_DIRECTORIES)
    file(GLOB_RECURSE WIDGET_SOURCES "${DIRECTORY}/*.cpp" "${DIRECTORY}/*.h")

    foreach (WIDGET_SOURCE IN LISTS WIDGET_SOURCES)
        if (WIDGET_SOURCE MATCHES "/ModernistTones\\.(cpp|h)$")
            continue()
        endif ()

        file(STRINGS "${WIDGET_SOURCE}" LINES)
        file(RELATIVE_PATH SHOWN "${FSORG_SOURCE_DIR}" "${WIDGET_SOURCE}")

        set(LINE_NUMBER 0)
        foreach (LINE IN LISTS LINES)
            math(EXPR LINE_NUMBER "${LINE_NUMBER} + 1")

            if (LINE MATCHES "${THE_SUBSTITUTION_TABLE}")
                continue()
            endif ()

            if (LINE MATCHES "${THE_DISABLED_INK}" AND NOT LINE MATCHES "${A_DISABLING_CONDITION}")
                list(APPEND WORN "  ${SHOWN}:${LINE_NUMBER}: ${LINE}")
            endif ()

            if (LINE MATCHES "${A_DISABLING_CONDITION}" AND LINE MATCHES "${A_PAINTED_INK}"
                    AND NOT LINE MATCHES "${THE_DISABLED_INK}")
                list(APPEND SWAPPED "  ${SHOWN}:${LINE_NUMBER}: ${LINE}")
            endif ()
        endforeach ()
    endforeach ()
endforeach ()

if (WORN)
    string(REPLACE ";" "\n" REPORT "${WORN}")
    message(FATAL_ERROR
            "The disabled ink is painting text that is not disabled.\n"
            "It is exempt from the contrast minimum because disabling is what it says (ADR-0019),\n"
            "so live text painted with it is unreadable and nothing else reports it.\n"
            "Use the faint tone for live discreet text.\n"
            "${REPORT}")
endif ()

if (SWAPPED)
    string(REPLACE ";" "\n" REPORT "${SWAPPED}")
    message(FATAL_ERROR
            "Disabled text is being painted with a tone other than the disabled ink.\n"
            "Brightening disabled text erases the distinction it exists to draw (ADR-0019).\n"
            "${REPORT}")
endif ()

message(STATUS "The disabled ink paints disabled text and nothing else.")

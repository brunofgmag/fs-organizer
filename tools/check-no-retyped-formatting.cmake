if (NOT DEFINED FSORG_SOURCE_DIR)
    message(FATAL_ERROR "FSORG_SOURCE_DIR must be defined.")
endif ()

set(SIZE_UNITS B KiB MiB GiB TiB KB MB GB TB)

file(GLOB_RECURSE TEST_SOURCES "${FSORG_SOURCE_DIR}/tests/*.cpp" "${FSORG_SOURCE_DIR}/tests/*.h")

set(OFFENCES "")

foreach (TEST_SOURCE IN LISTS TEST_SOURCES)
    file(STRINGS "${TEST_SOURCE}" LINES ENCODING UTF-8)

    set(LINE_NUMBER 0)
    foreach (LINE IN LISTS LINES)
        math(EXPR LINE_NUMBER "${LINE_NUMBER} + 1")

        set(RETYPED FALSE)

        foreach (UNIT IN LISTS SIZE_UNITS)
            if (LINE MATCHES "\"[0-9]+[.,][0-9]+ ?${UNIT}\"")
                set(RETYPED TRUE)
            endif ()
        endforeach ()

        if (RETYPED)
            file(RELATIVE_PATH SHOWN "${FSORG_SOURCE_DIR}" "${TEST_SOURCE}")
            list(APPEND OFFENCES "  ${SHOWN}:${LINE_NUMBER}: ${LINE}")
        endif ()
    endforeach ()
endforeach ()

if (OFFENCES)
    string(REPLACE ";" "\n" REPORT "${OFFENCES}")
    message(FATAL_ERROR
            "A test retyped by hand a size that a locale formats.\n"
            "Size text comes from AsSize in src/support/SizeText.h, which is QLocale::formattedDataSize, "
            "and the locale is the machine's: this host writes 1,90 GiB and the CI runner writes 1.90 GiB. "
            "A literal passes here and fails there, and no local configuration tells the two apart.\n"
            "Call AsSize on the same number production is given and compare against its answer, "
            "so both sides fold through one locale.\n"
            "${REPORT}")
endif ()

message(STATUS "No test retypes a formatted size by hand.")

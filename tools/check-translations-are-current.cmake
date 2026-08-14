if (NOT DEFINED FSORG_SOURCE_DIR)
    message(FATAL_ERROR "FSORG_SOURCE_DIR must be defined.")
endif ()

if (NOT DEFINED FSORG_SCRATCH)
    message(FATAL_ERROR "FSORG_SCRATCH must be defined.")
endif ()

if (NOT DEFINED FSORG_LUPDATE)
    message(FATAL_ERROR "FSORG_LUPDATE must be defined.")
endif ()

set(CATALOGUES
        "${FSORG_SOURCE_DIR}/i18n/app_en.ts"
        "${FSORG_SOURCE_DIR}/i18n/app_pt_BR.ts"
)

set(UNTRANSLATED "")

foreach (CATALOGUE IN LISTS CATALOGUES)
    get_filename_component(NAMED "${CATALOGUE}" NAME)
    set(FRESH "${FSORG_SCRATCH}/asked-of-lupdate-${NAMED}")

    configure_file("${CATALOGUE}" "${FRESH}" COPYONLY)

    execute_process(
            COMMAND "${FSORG_LUPDATE}" -no-obsolete -locations none "${FSORG_SOURCE_DIR}/src" -ts "${FRESH}"
            RESULT_VARIABLE ITWENT
            OUTPUT_VARIABLE ITSAID
            ERROR_VARIABLE ITCOMPLAINED)

    if (NOT ITWENT EQUAL 0)
        message(FATAL_ERROR "lupdate could not read the sources: ${ITCOMPLAINED}")
    endif ()

    file(STRINGS "${FRESH}" LINES ENCODING UTF-8)

    set(LAST_SAID "")

    foreach (LINE IN LISTS LINES)
        if (LINE MATCHES "<source>(.*)</source>")
            set(LAST_SAID "${CMAKE_MATCH_1}")
        elseif (LINE MATCHES "<source>(.*)")
            set(LAST_SAID "${CMAKE_MATCH_1}")
        endif ()

        if (LINE MATCHES "type=.unfinished.")
            list(APPEND UNTRANSLATED "  ${NAMED}: ${LAST_SAID}")
        endif ()
    endforeach ()
endforeach ()

if (UNTRANSLATED)
    string(REPLACE ";" "\n" REPORT "${UNTRANSLATED}")
    message(FATAL_ERROR
            "These strings reach the user in English whatever language was chosen, because the catalogue "
            "carries no translation for them.\n"
            "The build runs lrelease over the catalogues as they are committed and never lupdate, so a string "
            "added to the code is invisible here until somebody says it.\n"
            "Fix it by running lupdate over i18n/app_en.ts and i18n/app_pt_BR.ts and translating what it adds.\n"
            "${REPORT}")
endif ()

message(STATUS "Every string the code says is translated in both catalogues.")

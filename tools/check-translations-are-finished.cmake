if (NOT DEFINED FSORG_SOURCE_DIR)
    message(FATAL_ERROR "FSORG_SOURCE_DIR must be defined.")
endif ()

file(GLOB CATALOGUES "${FSORG_SOURCE_DIR}/i18n/*.ts")

if (NOT CATALOGUES)
    message(FATAL_ERROR
            "No translation catalogue was found under ${FSORG_SOURCE_DIR}/i18n.\n"
            "A guard that reads nothing passes for the wrong reason: either the catalogues moved "
            "and this path follows them, or they are gone and this target goes with them.")
endif ()

set(OFFENCES "")
set(MESSAGES_READ 0)

foreach (CATALOGUE IN LISTS CATALOGUES)
    file(STRINGS "${CATALOGUE}" LINES ENCODING UTF-8)
    file(RELATIVE_PATH SHOWN "${FSORG_SOURCE_DIR}" "${CATALOGUE}")

    set(CONTEXT "")
    set(SOURCE_TEXT "")
    set(LINE_NUMBER 0)

    foreach (LINE IN LISTS LINES)
        math(EXPR LINE_NUMBER "${LINE_NUMBER} + 1")

        if (LINE MATCHES "<name>(.*)</name>")
            set(CONTEXT "${CMAKE_MATCH_1}")
        endif ()

        if (LINE MATCHES "<source>(.*)</source>")
            set(SOURCE_TEXT "${CMAKE_MATCH_1}")
        endif ()

        if (LINE MATCHES "<message")
            math(EXPR MESSAGES_READ "${MESSAGES_READ} + 1")
        endif ()

        if (LINE MATCHES "type=\"unfinished\"" OR LINE MATCHES "<translation></translation>")
            list(APPEND OFFENCES "  ${SHOWN}:${LINE_NUMBER}: ${CONTEXT} says \"${SOURCE_TEXT}\"")
        endif ()
    endforeach ()
endforeach ()

list(LENGTH CATALOGUES HOW_MANY_CATALOGUES)

if (OFFENCES)
    list(LENGTH OFFENCES HOW_MANY_OFFENCES)
    string(REPLACE ";" "\n" REPORT "${OFFENCES}")
    message(FATAL_ERROR
            "${HOW_MANY_OFFENCES} translations are still pending.\n"
            "A pending entry ships the English source to whoever chose another language, and no other "
            "target sees it: a GUI test compares screen text against tr(), which answers the English "
            "source when the translation is missing, so the assertion and the defect say the same thing.\n"
            "Run the fs-organizer_lupdate target, translate what it lists in every catalogue, and remove "
            "the unfinished marker. This guard never runs lupdate itself, because that would rewrite "
            "versioned files during ctest.\n"
            "${REPORT}")
endif ()

message(STATUS
        "Every translation is finished: ${MESSAGES_READ} messages read "
        "across ${HOW_MANY_CATALOGUES} catalogues.")

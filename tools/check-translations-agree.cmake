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

macro(fsorg_tidy WHAT)
    string(REPLACE "</numerusform>" " / " ${WHAT} "${${WHAT}}")
    string(REPLACE "<numerusform>" "" ${WHAT} "${${WHAT}}")
    string(REGEX REPLACE "[ \t]+" " " ${WHAT} "${${WHAT}}")
    string(STRIP "${${WHAT}}" ${WHAT})
    string(REGEX REPLACE " /$" "" ${WHAT} "${${WHAT}}")
    string(STRIP "${${WHAT}}" ${WHAT})
endmacro()

macro(fsorg_keep_reading WHAT CLOSING)
    if (LINE MATCHES "^(.*)${CLOSING}")
        string(APPEND ${WHAT} " ${CMAKE_MATCH_1}")
        set(COLLECTING "")
    else ()
        string(APPEND ${WHAT} " ${LINE}")
    endif ()
endmacro()

macro(fsorg_settle_the_message)
    fsorg_tidy(SOURCE_TEXT)
    fsorg_tidy(COMMENT_TEXT)
    fsorg_tidy(TRANSLATION)

    if (NOT SETTLED AND NOT SOURCE_TEXT STREQUAL "")
        string(MD5 SLOT "${LANGUAGE}|${SOURCE_TEXT}|${COMMENT_TEXT}")

        if (NOT DEFINED SEEN_${SLOT})
            set(SEEN_${SLOT} "${TRANSLATION}")
            set(WHERE_${SLOT} "${CONTEXT}:${OPENED_AT}")
        elseif (NOT "${SEEN_${SLOT}}" STREQUAL "${TRANSLATION}")
            math(EXPR HOW_MANY_OFFENCES "${HOW_MANY_OFFENCES} + 1")
            set(TOLD "\"${SOURCE_TEXT}\"")

            if (NOT COMMENT_TEXT STREQUAL "")
                set(TOLD "${TOLD}, told apart as ${COMMENT_TEXT},")
            endif ()

            string(APPEND REPORT
                    "  ${SHOWN}: ${TOLD}\n"
                    "      ${WHERE_${SLOT}} says \"${SEEN_${SLOT}}\"\n"
                    "      ${CONTEXT}:${OPENED_AT} says \"${TRANSLATION}\"\n")
        endif ()
    endif ()
endmacro()

set(REPORT "")
set(HOW_MANY_OFFENCES 0)
set(MESSAGES_READ 0)

foreach (CATALOGUE IN LISTS CATALOGUES)
    file(STRINGS "${CATALOGUE}" LINES ENCODING UTF-8)
    file(RELATIVE_PATH SHOWN "${FSORG_SOURCE_DIR}" "${CATALOGUE}")
    get_filename_component(LANGUAGE "${CATALOGUE}" NAME_WE)

    set(CONTEXT "")
    set(SOURCE_TEXT "")
    set(COMMENT_TEXT "")
    set(TRANSLATION "")
    set(COLLECTING "")
    set(SETTLED FALSE)
    set(LINE_NUMBER 0)
    set(OPENED_AT 0)

    foreach (LINE IN LISTS LINES)
        math(EXPR LINE_NUMBER "${LINE_NUMBER} + 1")

        if (COLLECTING STREQUAL "source")
            fsorg_keep_reading(SOURCE_TEXT "</source>")
        elseif (COLLECTING STREQUAL "comment")
            fsorg_keep_reading(COMMENT_TEXT "</comment>")
        elseif (COLLECTING STREQUAL "translation")
            fsorg_keep_reading(TRANSLATION "</translation>")
        elseif (LINE MATCHES "<name>(.*)</name>")
            set(CONTEXT "${CMAKE_MATCH_1}")
        elseif (LINE MATCHES "<message")
            math(EXPR MESSAGES_READ "${MESSAGES_READ} + 1")
            set(SOURCE_TEXT "")
            set(COMMENT_TEXT "")
            set(TRANSLATION "")
            set(SETTLED FALSE)
            set(OPENED_AT ${LINE_NUMBER})
        elseif (LINE MATCHES "<source>(.*)</source>")
            set(SOURCE_TEXT "${CMAKE_MATCH_1}")
        elseif (LINE MATCHES "<source>(.*)$")
            set(SOURCE_TEXT "${CMAKE_MATCH_1}")
            set(COLLECTING "source")
        elseif (LINE MATCHES "<comment>(.*)</comment>")
            set(COMMENT_TEXT "${CMAKE_MATCH_1}")
        elseif (LINE MATCHES "<comment>(.*)$")
            set(COMMENT_TEXT "${CMAKE_MATCH_1}")
            set(COLLECTING "comment")
        elseif (LINE MATCHES "type=\"vanished\"" OR LINE MATCHES "type=\"unfinished\"")
            set(SETTLED TRUE)
        elseif (LINE MATCHES "<translation[^>]*>(.*)</translation>")
            set(TRANSLATION "${CMAKE_MATCH_1}")
        elseif (LINE MATCHES "<translation[^>]*>(.*)$")
            set(TRANSLATION "${CMAKE_MATCH_1}")
            set(COLLECTING "translation")
        elseif (LINE MATCHES "</message>")
            fsorg_settle_the_message()
        endif ()
    endforeach ()
endforeach ()

list(LENGTH CATALOGUES HOW_MANY_CATALOGUES)

if (MESSAGES_READ EQUAL 0)
    message(FATAL_ERROR
            "${HOW_MANY_CATALOGUES} catalogues were opened and not one message was read.\n"
            "The file format changed under this guard, and a guard that reads nothing agrees with everything.")
endif ()

if (HOW_MANY_OFFENCES GREATER 0)
    set(HOW_MANY_SAID "${HOW_MANY_OFFENCES} phrases are")

    if (HOW_MANY_OFFENCES EQUAL 1)
        set(HOW_MANY_SAID "1 phrase is")
    endif ()

    message(FATAL_ERROR
            "${HOW_MANY_SAID} translated two ways inside the same catalogue.\n"
            "The reader sees two names for the same button on two screens and concludes they are two things, "
            "which is what the glossary exists to prevent. The other two translation guards look at the absence "
            "of a translation and are blind to this one, because here every translation exists.\n"
            "Settle it by the glossary, not by taste. When the two really are different messages that English "
            "happens to spell alike, say so in the code with a disambiguation, tr(\"Broken\", \"several addons\"), "
            "and hand-copy the <comment> into every catalogue: running lupdate over the committed files drops "
            "the vanished entries and the locations, and neither of the other guards reports the loss.\n"
            "${REPORT}")
endif ()

message(STATUS "${MESSAGES_READ} messages across ${HOW_MANY_CATALOGUES} catalogues, and each phrase is said one way.")

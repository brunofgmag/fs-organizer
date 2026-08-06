if (NOT DEFINED FSORG_SOURCE_DIR)
    message(FATAL_ERROR "FSORG_SOURCE_DIR must be defined.")
endif ()

set(SEARCHED_DIRECTORIES
        "${FSORG_SOURCE_DIR}/src"
        "${FSORG_SOURCE_DIR}/tools"
)

set(A_STRING_LITERAL "\"([^\"\\\\]|\\\\.)*\"")
set(A_CHARACTER_LITERAL "'(\\\\.|[^'\\\\])'")
set(A_LINE_COMMENT "//.*$")
set(A_ONE_LINE_BLOCK_COMMENT "/\\*[^*]*\\*/")
set(WHAT_IS_NOT_STRUCTURE "[^][(){},?;]")

set(OFFENCES "")

foreach (DIRECTORY IN LISTS SEARCHED_DIRECTORIES)
    file(GLOB_RECURSE SOURCES "${DIRECTORY}/*.cpp" "${DIRECTORY}/*.h")

    foreach (SOURCE IN LISTS SOURCES)
        file(STRINGS "${SOURCE}" LINES ENCODING UTF-8)
        file(RELATIVE_PATH SHOWN "${FSORG_SOURCE_DIR}" "${SOURCE}")

        set(DEPTH 0)
        set(OPEN_TERNARIES "")
        set(INSIDE_BLOCK_COMMENT OFF)
        set(LINE_NUMBER 0)

        foreach (LINE IN LISTS LINES)
            math(EXPR LINE_NUMBER "${LINE_NUMBER} + 1")

            if (INSIDE_BLOCK_COMMENT)
                string(FIND "${LINE}" "*/" CLOSES)
                if (CLOSES LESS 0)
                    continue()
                endif ()
                math(EXPR CLOSES "${CLOSES} + 2")
                string(SUBSTRING "${LINE}" ${CLOSES} -1 LINE)
                set(INSIDE_BLOCK_COMMENT OFF)
            endif ()

            string(REGEX REPLACE "${A_STRING_LITERAL}" "\"\"" LINE "${LINE}")
            string(REGEX REPLACE "${A_CHARACTER_LITERAL}" "''" LINE "${LINE}")
            string(REGEX REPLACE "${A_ONE_LINE_BLOCK_COMMENT}" " " LINE "${LINE}")
            string(REGEX REPLACE "${A_LINE_COMMENT}" "" LINE "${LINE}")

            string(FIND "${LINE}" "/*" OPENS)
            if (OPENS GREATER_EQUAL 0)
                string(SUBSTRING "${LINE}" 0 ${OPENS} LINE)
                set(INSIDE_BLOCK_COMMENT ON)
            endif ()

            string(REGEX REPLACE "${WHAT_IS_NOT_STRUCTURE}" "" SKELETON "${LINE}")
            string(LENGTH "${SKELETON}" WIDTH)
            if (WIDTH EQUAL 0)
                continue()
            endif ()

            math(EXPR LAST "${WIDTH} - 1")
            foreach (POSITION RANGE ${LAST})
                string(SUBSTRING "${SKELETON}" ${POSITION} 1 MARK)

                if (MARK STREQUAL "(" OR MARK STREQUAL "[" OR MARK STREQUAL "{")
                    math(EXPR DEPTH "${DEPTH} + 1")
                elseif (MARK STREQUAL ")" OR MARK STREQUAL "]" OR MARK STREQUAL "}")
                    math(EXPR DEPTH "${DEPTH} - 1")
                    list(POP_BACK OPEN_TERNARIES INNERMOST)
                    while (DEFINED INNERMOST AND INNERMOST GREATER ${DEPTH})
                        unset(INNERMOST)
                        list(POP_BACK OPEN_TERNARIES INNERMOST)
                    endwhile ()
                    if (DEFINED INNERMOST)
                        list(APPEND OPEN_TERNARIES ${INNERMOST})
                        unset(INNERMOST)
                    endif ()
                elseif (MARK STREQUAL ",")
                    list(POP_BACK OPEN_TERNARIES INNERMOST)
                    while (DEFINED INNERMOST AND NOT INNERMOST LESS ${DEPTH})
                        unset(INNERMOST)
                        list(POP_BACK OPEN_TERNARIES INNERMOST)
                    endwhile ()
                    if (DEFINED INNERMOST)
                        list(APPEND OPEN_TERNARIES ${INNERMOST})
                        unset(INNERMOST)
                    endif ()
                elseif (MARK STREQUAL ";")
                    set(OPEN_TERNARIES "")
                elseif (MARK STREQUAL "?")
                    if (OPEN_TERNARIES)
                        list(APPEND OFFENCES "  ${SHOWN}:${LINE_NUMBER}")
                    endif ()
                    list(APPEND OPEN_TERNARIES ${DEPTH})
                endif ()
            endforeach ()
        endforeach ()
    endforeach ()
endforeach ()

if (OFFENCES)
    list(REMOVE_DUPLICATES OFFENCES)
    string(REPLACE ";" "\n" REPORT "${OFFENCES}")
    message(FATAL_ERROR
            "A conditional expression is answering a second question inside the answer to the first.\n"
            "Two ternaries side by side are fine: this reports only the one written inside the other, "
            "where the reader has to hold the outer question open to read the inner one.\n"
            "Give the outer question a guard with an early return, or name the inner answer in a local.\n"
            "${REPORT}")
endif ()

message(STATUS "No conditional expression is nested inside another.")

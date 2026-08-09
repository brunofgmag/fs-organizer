if (NOT DEFINED FSORG_SOURCE_DIR)
    message(FATAL_ERROR "FSORG_SOURCE_DIR must be defined.")
endif ()

set(SEARCHED_DIRECTORIES
        "${FSORG_SOURCE_DIR}/src"
        "${FSORG_SOURCE_DIR}/tests"
        "${FSORG_SOURCE_DIR}/tools"
)

set(A_NAME "[A-Za-z_][A-Za-z0-9_]*")
set(A_PIXMAP_GIVEN_A_SIZE "QPixmap +(${A_NAME}) *\\(")

set(DIRTY "")

foreach (DIRECTORY IN LISTS SEARCHED_DIRECTORIES)
    file(GLOB_RECURSE SOURCES "${DIRECTORY}/*.cpp" "${DIRECTORY}/*.h")

    foreach (SOURCE IN LISTS SOURCES)
        file(STRINGS "${SOURCE}" LINES)
        file(RELATIVE_PATH SHOWN "${FSORG_SOURCE_DIR}" "${SOURCE}")

        set(PENDING "")
        set(LINE_NUMBER 0)

        foreach (LINE IN LISTS LINES)
            math(EXPR LINE_NUMBER "${LINE_NUMBER} + 1")

            set(SETTLED "")

            foreach (NAME IN LISTS PENDING)
                set(A_FILL "${NAME} *\\. *fill *\\(")
                set(A_BRUSH_TAKING_IT "(render *\\(|QPainter +${A_NAME} *\\() *& *${NAME}[^A-Za-z0-9_]")

                if (LINE MATCHES "${A_FILL}")
                    list(APPEND SETTLED "${NAME}")
                elseif (LINE MATCHES "${A_BRUSH_TAKING_IT}")
                    list(APPEND DIRTY "  ${SHOWN}:${LINE_NUMBER}: ${NAME}")
                    list(APPEND SETTLED "${NAME}")
                endif ()
            endforeach ()

            foreach (NAME IN LISTS SETTLED)
                list(REMOVE_ITEM PENDING "${NAME}")
            endforeach ()

            if (LINE MATCHES "${A_PIXMAP_GIVEN_A_SIZE}")
                list(APPEND PENDING "${CMAKE_MATCH_1}")
            endif ()
        endforeach ()
    endforeach ()
endforeach ()

if (DIRTY)
    string(REPLACE ";" "\n" REPORT "${DIRTY}")
    message(FATAL_ERROR
            "A QPixmap is painted before anything wrote to it.\n"
            "A pixmap given a size owns uninitialised memory, and a widget that does not paint\n"
            "its own background leaves that memory showing, so the picture carries whatever the\n"
            "process rendered before it. The symptom is an intermittent red, not a red.\n"
            "Call fill() on it between the size and the brush.\n"
            "${REPORT}")
endif ()

message(STATUS "Every QPixmap given a size is filled before it is painted.")

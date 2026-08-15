if (NOT DEFINED FSORG_SOURCE_DIR)
    message(FATAL_ERROR "FSORG_SOURCE_DIR must be defined.")
endif ()

# The manual is two independent sources on purpose, and the PDF beside them is
# rebuilt by the Rebuild Manual job inside the release pull request. This reads
# text and nothing else: it never runs lualatex, and it never looks at the PDF,
# whose age says nothing after a clone, because git stamps every file with the
# checkout time.
#
# The cover reads VERSION.txt at composition time, so the version on it cannot
# be wrong. What can be wrong is someone typing the version there again, which
# is the shape this guard now refuses.

set(MANUAL_DIRECTORY "${FSORG_SOURCE_DIR}/manual")

file(GLOB MANUAL_SOURCES RELATIVE "${MANUAL_DIRECTORY}" "${MANUAL_DIRECTORY}/fs-organizer-*.tex")
list(SORT MANUAL_SOURCES)
list(LENGTH MANUAL_SOURCES HOW_MANY_SOURCES)

# One language cannot disagree with itself, so the heading comparison would pass
# by having nothing to compare. A manual deleted has to fail here, not go quiet.
if (HOW_MANY_SOURCES LESS 2)
    message(FATAL_ERROR
            "manual/ carries ${HOW_MANY_SOURCES} of fs-organizer-*.tex, and this guard needs at least two to compare.")
endif ()

file(READ "${FSORG_SOURCE_DIR}/VERSION.txt" DECLARED_VERSION)
string(STRIP "${DECLARED_VERSION}" DECLARED_VERSION)

set(OFFENCES "")
set(FIRST_SHAPE "")
set(FIRST_SOURCE "")

foreach (MANUAL_SOURCE IN LISTS MANUAL_SOURCES)
    set(SOURCE_PATH "${MANUAL_DIRECTORY}/${MANUAL_SOURCE}")

    file(STRINGS "${SOURCE_PATH}" LINES ENCODING UTF-8)

    set(SHAPE "")
    set(TYPED_VERSION "")
    set(READS_THE_VERSION FALSE)
    set(FIGURE_DIRECTORY "")
    set(WANTED_FIGURES "")

    foreach (LINE IN LISTS LINES)
        if (LINE MATCHES "^\\\\subsection\\{")
            list(APPEND SHAPE "subsection")
        elseif (LINE MATCHES "^\\\\section\\{")
            list(APPEND SHAPE "section")
        endif ()

        if (LINE MATCHES "^ *\\{Vers[^ ]* \\\\input\\{\\.\\./VERSION\\.txt\\}\\}")
            set(READS_THE_VERSION TRUE)
        elseif (LINE MATCHES "^ *\\{Vers[^ ]* ([0-9][^}]*)\\}")
            set(TYPED_VERSION "${CMAKE_MATCH_1}")
        endif ()

        if (LINE MATCHES "\\\\graphicspath\\{\\{([^}]+)\\}\\}")
            set(FIGURE_DIRECTORY "${CMAKE_MATCH_1}")
        endif ()

        if (LINE MATCHES "\\\\shot\\{([^}]+)\\}")
            list(APPEND WANTED_FIGURES "${CMAKE_MATCH_1}")
        endif ()
    endforeach ()

    if (NOT TYPED_VERSION STREQUAL "")
        list(APPEND OFFENCES
                "  ${MANUAL_SOURCE} types version ${TYPED_VERSION} on its cover, which goes stale on its own: the cover reads it with \\input{../VERSION.txt}")
    elseif (NOT READS_THE_VERSION)
        list(APPEND OFFENCES
                "  ${MANUAL_SOURCE} carries no version on its cover, and VERSION.txt says ${DECLARED_VERSION}")
    endif ()

    if (FIGURE_DIRECTORY STREQUAL "")
        list(APPEND OFFENCES "  ${MANUAL_SOURCE} names no graphicspath, so no figure of it can be found")
    else ()
        foreach (FIGURE IN LISTS WANTED_FIGURES)
            if (NOT EXISTS "${MANUAL_DIRECTORY}/${FIGURE_DIRECTORY}${FIGURE}")
                list(APPEND OFFENCES
                        "  ${MANUAL_SOURCE} asks for manual/${FIGURE_DIRECTORY}${FIGURE}, which is not on disk")
            endif ()
        endforeach ()
    endif ()

    if (FIRST_SOURCE STREQUAL "")
        set(FIRST_SHAPE "${SHAPE}")
        set(FIRST_SOURCE "${MANUAL_SOURCE}")
    elseif (NOT SHAPE STREQUAL FIRST_SHAPE)
        list(LENGTH SHAPE HOW_MANY)
        list(LENGTH FIRST_SHAPE HOW_MANY_FIRST)
        list(APPEND OFFENCES
                "  ${FIRST_SOURCE} and ${MANUAL_SOURCE} do not carry the same headings in the same order: "
                "${HOW_MANY_FIRST} against ${HOW_MANY}")
    endif ()
endforeach ()

if (OFFENCES)
    string(REPLACE ";" "\n" REPORT "${OFFENCES}")
    message(FATAL_ERROR
            "The user manuals have drifted.\n"
            "They are independent sources covering the same subjects, and the PDF beside them is rebuilt by the "
            "Rebuild Manual job inside the release pull request, or by hand with manual/build.ps1.\n"
            "${REPORT}")
endif ()

list(LENGTH FIRST_SHAPE HEADINGS)

message(STATUS
        "The ${HOW_MANY_SOURCES} user manuals carry the same ${HEADINGS} headings, read version ${DECLARED_VERSION} "
        "off VERSION.txt, and every figure they ask for is on disk.")

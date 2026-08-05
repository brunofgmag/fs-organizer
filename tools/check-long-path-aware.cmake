if (NOT DEFINED FSORG_EXECUTABLE)
    message(FATAL_ERROR "FSORG_EXECUTABLE must be defined.")
endif ()

if (NOT EXISTS "${FSORG_EXECUTABLE}")
    message(FATAL_ERROR "The executable to inspect was not found at ${FSORG_EXECUTABLE}.")
endif ()

file(READ "${FSORG_EXECUTABLE}" BINARY HEX)
string(HEX ">true</longPathAware>" DECLARATION)
string(FIND "${BINARY}" "${DECLARATION}" FOUND)

if (FOUND EQUAL -1)
    message(FATAL_ERROR
            "The executable does not declare longPathAware in its manifest.\n"
            "Without it every std::filesystem call in the app stops at 260 characters, "
            "because those calls carry no extended prefix.\n"
            "The declaration comes from assets/app.manifest through APP_SOURCES; "
            "check that the linker still embeds it.\n"
            "Inspected: ${FSORG_EXECUTABLE}")
endif ()

message(STATUS "The executable declares longPathAware: ${FSORG_EXECUTABLE}")

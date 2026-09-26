set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_SUPPRESS_REGENERATION ON)

set(APP_NAME "fs-organizer")

set(FSORG_EDITION "github" CACHE STRING "The edition the executable is built as: github or flightsim-to.")
set_property(CACHE FSORG_EDITION PROPERTY STRINGS github flightsim-to)

if (NOT FSORG_EDITION STREQUAL "github" AND NOT FSORG_EDITION STREQUAL "flightsim-to")
    message(FATAL_ERROR "FSORG_EDITION must be github or flightsim-to, not '${FSORG_EDITION}'.")
endif ()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if (NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "The project must be configured for x64.")
endif ()

if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif ()

set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<$<CONFIG:Debug,RelWithDebInfo>:Embedded>")

if (MSVC)
    add_compile_options(/MP /W4 /WX /w14062 /external:W0)
else ()
    add_compile_options(-Wall -Wextra -Werror)
endif ()

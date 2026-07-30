set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_SUPPRESS_REGENERATION ON)

set(APP_NAME "fs-organizer")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(FSORG_TESTS_ONLY "Configure only the unit tests (host build, e.g. Linux CI)." OFF)

if (NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "The project must be configured for x64.")
endif ()

if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif ()

if (MSVC)
    add_compile_options(/MP /FS /W4 /WX /w14062 /external:W0)
else ()
    add_compile_options(-Wall -Wextra -Werror)
endif ()

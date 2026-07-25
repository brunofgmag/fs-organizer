if (NOT WIN32)
    return()
endif ()

add_executable(fsorg-probe
        tools/fsorg-probe/main.cpp
        src/domain/linking/EnabledStateResolver.cpp
        src/domain/tree/AddonTree.cpp
        src/infrastructure/catalog/FilesystemScanner.cpp
        src/infrastructure/catalog/JsonManifestParser.cpp
        src/infrastructure/fileops/WindowsFileOperations.cpp
        src/infrastructure/link/WindowsLinkService.cpp
        src/infrastructure/platform/WindowsKnownFolders.cpp
        src/infrastructure/sim/WindowsProcessProbe.cpp
        src/infrastructure/sim/WindowsSimulatorLocator.cpp
        src/infrastructure/sim/WindowsUserCfgLocations.cpp
)

target_include_directories(fsorg-probe PRIVATE "${CMAKE_SOURCE_DIR}/src")

target_compile_definitions(fsorg-probe PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)

if (MSVC)
    target_compile_options(fsorg-probe PRIVATE /W4 /permissive- /Zc:preprocessor /external:W0)
endif ()

target_link_libraries(fsorg-probe PRIVATE Qt6::Core)

add_custom_command(TARGET fsorg-probe POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "$<TARGET_FILE:Qt6::Core>"
        "$<TARGET_FILE_DIR:fsorg-probe>"
        VERBATIM)

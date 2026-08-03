if (NOT WIN32)
    return()
endif ()

add_executable(fsorg-probe
        tools/fsorg-probe/main.cpp
)

target_include_directories(fsorg-probe PRIVATE "${CMAKE_SOURCE_DIR}/src")

target_compile_definitions(fsorg-probe PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)

if (MSVC)
    target_compile_options(fsorg-probe PRIVATE /permissive- /Zc:preprocessor)
endif ()

target_link_libraries(fsorg-probe PRIVATE fsorg-infrastructure Qt6::Core)

add_custom_command(TARGET fsorg-probe POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "$<TARGET_FILE:Qt6::Core>"
        "$<TARGET_FILE_DIR:fsorg-probe>"
        VERBATIM)

add_executable(fsorg-shot
        tools/fsorg-shot/main.cpp
        tools/shared/DisposableState.h
        ${NETWORK_INFRASTRUCTURE_SOURCES}
        ${WINDOWS_SHELL_SOURCES}
        assets/resources.qrc
)

target_include_directories(fsorg-shot PRIVATE "${CMAKE_SOURCE_DIR}/src" "${CMAKE_SOURCE_DIR}/tools")

target_compile_definitions(fsorg-shot PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX FSORG_VERSION="${FSORG_VERSION}")

target_link_libraries(fsorg-shot PRIVATE fsorg-view fsorg-infrastructure Qt6::Widgets Qt6::Network dwmapi)

add_custom_command(TARGET fsorg-shot POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "$<TARGET_FILE:Qt6::Core>"
        "$<TARGET_FILE:Qt6::Gui>"
        "$<TARGET_FILE:Qt6::Network>"
        "$<TARGET_FILE:Qt6::Widgets>"
        "$<TARGET_FILE_DIR:fsorg-shot>"
        COMMAND "${CMAKE_COMMAND}" -E make_directory
        "$<TARGET_FILE_DIR:fsorg-shot>/platforms"
        "$<TARGET_FILE_DIR:fsorg-shot>/styles"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "$<TARGET_FILE:Qt6::QWindowsIntegrationPlugin>"
        "$<TARGET_FILE:Qt6::QOffscreenIntegrationPlugin>"
        "$<TARGET_FILE_DIR:fsorg-shot>/platforms/"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "$<TARGET_FILE:Qt6::QModernWindowsStylePlugin>"
        "$<TARGET_FILE_DIR:fsorg-shot>/styles/"
        COMMAND "${CMAKE_COMMAND}" -E make_directory
        "$<TARGET_FILE_DIR:fsorg-shot>/i18n"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${CMAKE_BINARY_DIR}/app_en.qm"
        "${CMAKE_BINARY_DIR}/app_pt_BR.qm"
        "$<TARGET_FILE_DIR:fsorg-shot>/i18n/"
        VERBATIM)

add_dependencies(fsorg-shot release_translations)

add_executable(fsorg-timing
        tools/fsorg-timing/main.cpp
        tools/fsorg-timing/JournalScroll.cpp
        tools/fsorg-timing/AppScroll.cpp
        tools/shared/DisposableState.h
        ${NETWORK_INFRASTRUCTURE_SOURCES}
        ${WINDOWS_SHELL_SOURCES}
)

target_include_directories(fsorg-timing PRIVATE "${CMAKE_SOURCE_DIR}/src" "${CMAKE_SOURCE_DIR}/tools")

target_compile_definitions(fsorg-timing PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)

target_link_libraries(fsorg-timing PRIVATE fsorg-view fsorg-infrastructure Qt6::Widgets Qt6::Network dwmapi)

add_custom_command(TARGET fsorg-timing POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "$<TARGET_FILE:Qt6::Core>"
        "$<TARGET_FILE:Qt6::Gui>"
        "$<TARGET_FILE:Qt6::Network>"
        "$<TARGET_FILE:Qt6::Widgets>"
        "$<TARGET_FILE_DIR:fsorg-timing>"
        COMMAND "${CMAKE_COMMAND}" -E make_directory
        "$<TARGET_FILE_DIR:fsorg-timing>/platforms"
        "$<TARGET_FILE_DIR:fsorg-timing>/styles"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "$<TARGET_FILE:Qt6::QWindowsIntegrationPlugin>"
        "$<TARGET_FILE_DIR:fsorg-timing>/platforms/"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "$<TARGET_FILE:Qt6::QModernWindowsStylePlugin>"
        "$<TARGET_FILE_DIR:fsorg-timing>/styles/"
        VERBATIM)

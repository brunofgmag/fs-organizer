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

add_executable(fsorg-packages
        tools/fsorg-packages/main.cpp
)

target_include_directories(fsorg-packages PRIVATE "${CMAKE_SOURCE_DIR}/src")

target_compile_definitions(fsorg-packages PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)

if (MSVC)
    target_compile_options(fsorg-packages PRIVATE /permissive- /Zc:preprocessor)
endif ()

target_link_libraries(fsorg-packages PRIVATE fsorg-infrastructure Qt6::Core)

add_custom_command(TARGET fsorg-packages POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "$<TARGET_FILE:Qt6::Core>"
        "$<TARGET_FILE_DIR:fsorg-packages>"
        VERBATIM)

add_executable(fsorg-startup
        tools/fsorg-startup/main.cpp
)

target_include_directories(fsorg-startup PRIVATE "${CMAKE_SOURCE_DIR}/src")

target_compile_definitions(fsorg-startup PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)

if (MSVC)
    target_compile_options(fsorg-startup PRIVATE /permissive- /Zc:preprocessor)
endif ()

target_link_libraries(fsorg-startup PRIVATE fsorg-application fsorg-infrastructure Qt6::Core)

add_custom_command(TARGET fsorg-startup POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "$<TARGET_FILE:Qt6::Core>"
        "$<TARGET_FILE_DIR:fsorg-startup>"
        VERBATIM)

add_executable(fsorg-delete
        tools/fsorg-delete/main.cpp
        src/viewmodel/FailureText.cpp
)

target_include_directories(fsorg-delete PRIVATE "${CMAKE_SOURCE_DIR}/src")

target_compile_definitions(fsorg-delete PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)

if (MSVC)
    target_compile_options(fsorg-delete PRIVATE /permissive- /Zc:preprocessor)
endif ()

target_link_libraries(fsorg-delete PRIVATE fsorg-application fsorg-infrastructure Qt6::Core)

add_custom_command(TARGET fsorg-delete POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "$<TARGET_FILE:Qt6::Core>"
        "$<TARGET_FILE_DIR:fsorg-delete>"
        VERBATIM)

add_executable(fsorg-adopt
        tools/fsorg-adopt/main.cpp
        src/viewmodel/FailureText.cpp
)

target_include_directories(fsorg-adopt PRIVATE "${CMAKE_SOURCE_DIR}/src")

target_compile_definitions(fsorg-adopt PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)

if (MSVC)
    target_compile_options(fsorg-adopt PRIVATE /permissive- /Zc:preprocessor)
endif ()

target_link_libraries(fsorg-adopt PRIVATE fsorg-application fsorg-infrastructure Qt6::Core)

add_custom_command(TARGET fsorg-adopt POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "$<TARGET_FILE:Qt6::Core>"
        "$<TARGET_FILE_DIR:fsorg-adopt>"
        VERBATIM)

add_executable(fsorg-bgl
        tools/fsorg-bgl/main.cpp
)

target_include_directories(fsorg-bgl PRIVATE "${CMAKE_SOURCE_DIR}/src" "${CMAKE_SOURCE_DIR}/tools")

target_compile_definitions(fsorg-bgl PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)

if (MSVC)
    target_compile_options(fsorg-bgl PRIVATE /permissive- /Zc:preprocessor)
endif ()

target_link_libraries(fsorg-bgl PRIVATE fsorg-domain fsorg-infrastructure Qt6::Core)

add_custom_command(TARGET fsorg-bgl POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "$<TARGET_FILE:Qt6::Core>"
        "$<TARGET_FILE_DIR:fsorg-bgl>"
        VERBATIM)

add_executable(fsorg-bisect
        tools/fsorg-bisect/main.cpp
)

target_include_directories(fsorg-bisect PRIVATE "${CMAKE_SOURCE_DIR}/src")

target_compile_definitions(fsorg-bisect PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)

if (MSVC)
    target_compile_options(fsorg-bisect PRIVATE /permissive- /Zc:preprocessor)
endif ()

target_link_libraries(fsorg-bisect PRIVATE fsorg-application fsorg-infrastructure Qt6::Core)

add_custom_command(TARGET fsorg-bisect POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "$<TARGET_FILE:Qt6::Core>"
        "$<TARGET_FILE_DIR:fsorg-bisect>"
        VERBATIM)

add_executable(fsorg-size
        tools/fsorg-size/main.cpp
)

target_include_directories(fsorg-size PRIVATE "${CMAKE_SOURCE_DIR}/src")

target_compile_definitions(fsorg-size PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)

if (MSVC)
    target_compile_options(fsorg-size PRIVATE /permissive- /Zc:preprocessor)
endif ()

target_link_libraries(fsorg-size PRIVATE fsorg-application fsorg-infrastructure Qt6::Core)

add_custom_command(TARGET fsorg-size POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "$<TARGET_FILE:Qt6::Core>"
        "$<TARGET_FILE_DIR:fsorg-size>"
        VERBATIM)

add_executable(fsorg-docs
        tools/fsorg-docs/main.cpp
        ${PDF_INFRASTRUCTURE_SOURCES}
)

target_include_directories(fsorg-docs PRIVATE "${CMAKE_SOURCE_DIR}/src")

target_compile_definitions(fsorg-docs PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)

if (MSVC)
    target_compile_options(fsorg-docs PRIVATE /permissive- /Zc:preprocessor)
endif ()

target_link_libraries(fsorg-docs PRIVATE fsorg-application fsorg-infrastructure Qt6::Core Qt6::Pdf)

add_custom_command(TARGET fsorg-docs POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "$<TARGET_FILE:Qt6::Core>"
        "$<TARGET_FILE:Qt6::Gui>"
        "$<TARGET_FILE:Qt6::Network>"
        "$<TARGET_FILE:Qt6::Pdf>"
        "$<TARGET_FILE_DIR:fsorg-docs>"
        VERBATIM)

add_executable(fsorg-shot
        tools/fsorg-shot/main.cpp
        ${PDF_INFRASTRUCTURE_SOURCES}
        tools/shared/DisposableState.h
        ${NETWORK_INFRASTRUCTURE_SOURCES}
        ${WINDOWS_SHELL_SOURCES}
        assets/resources.qrc
)

target_include_directories(fsorg-shot PRIVATE "${CMAKE_SOURCE_DIR}/src" "${CMAKE_SOURCE_DIR}/tools")

target_compile_definitions(fsorg-shot PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX FSORG_VERSION="${FSORG_VERSION}")

target_link_libraries(fsorg-shot PRIVATE fsorg-view fsorg-infrastructure Qt6::Widgets Qt6::Network Qt6::Pdf dwmapi)

add_custom_command(TARGET fsorg-shot POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "$<TARGET_FILE:Qt6::Core>"
        "$<TARGET_FILE:Qt6::Gui>"
        "$<TARGET_FILE:Qt6::Network>"
        "$<TARGET_FILE:Qt6::Widgets>"
        "$<TARGET_FILE:Qt6::Pdf>"
        "$<TARGET_FILE:Qt6::PdfWidgets>"
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
        COMMAND "${CMAKE_COMMAND}" -E make_directory
        "$<TARGET_FILE_DIR:fsorg-shot>/translations"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "$<TARGET_FILE_DIR:Qt6::Core>/../translations/qtbase_pt_BR.qm"
        "$<TARGET_FILE_DIR:fsorg-shot>/translations/qt_pt_BR.qm"
        VERBATIM)

add_dependencies(fsorg-shot release_translations)

add_executable(fsorg-timing
        tools/fsorg-timing/main.cpp
        ${PDF_INFRASTRUCTURE_SOURCES}
        tools/fsorg-timing/JournalScroll.cpp
        tools/fsorg-timing/AppScroll.cpp
        tools/fsorg-timing/LibraryScroll.cpp
        tools/shared/DisposableState.h
        ${NETWORK_INFRASTRUCTURE_SOURCES}
        ${WINDOWS_SHELL_SOURCES}
)

target_include_directories(fsorg-timing PRIVATE "${CMAKE_SOURCE_DIR}/src" "${CMAKE_SOURCE_DIR}/tools")

target_compile_definitions(fsorg-timing PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)

target_link_libraries(fsorg-timing PRIVATE fsorg-view fsorg-infrastructure Qt6::Widgets Qt6::Network Qt6::Pdf dwmapi)

add_custom_command(TARGET fsorg-timing POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "$<TARGET_FILE:Qt6::Core>"
        "$<TARGET_FILE:Qt6::Gui>"
        "$<TARGET_FILE:Qt6::Network>"
        "$<TARGET_FILE:Qt6::Widgets>"
        "$<TARGET_FILE:Qt6::Pdf>"
        "$<TARGET_FILE:Qt6::PdfWidgets>"
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

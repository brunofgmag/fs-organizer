function(fsorg_add_console_tool TOOL_NAME)
    cmake_parse_arguments(TOOL "" "" "SOURCES;INCLUDES;LIBRARIES;BESIDE" ${ARGN})

    add_executable(${TOOL_NAME} ${TOOL_SOURCES} ${PLATFORM_COMPOSITION_SOURCES})

    target_include_directories(${TOOL_NAME} PRIVATE "${CMAKE_SOURCE_DIR}/src" ${TOOL_INCLUDES})

    target_link_libraries(${TOOL_NAME} PRIVATE ${TOOL_LIBRARIES})

    if (WIN32)
        target_compile_definitions(${TOOL_NAME} PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)

        set(BESIDE_FILES "")
        foreach (QT_LIBRARY IN LISTS TOOL_BESIDE)
            list(APPEND BESIDE_FILES "$<TARGET_FILE:${QT_LIBRARY}>")
        endforeach ()

        add_custom_command(TARGET ${TOOL_NAME} POST_BUILD
                COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                ${BESIDE_FILES}
                "$<TARGET_FILE_DIR:${TOOL_NAME}>"
                VERBATIM)
    endif ()

    if (MSVC)
        target_compile_options(${TOOL_NAME} PRIVATE /permissive- /Zc:preprocessor)
    endif ()
endfunction()

fsorg_add_console_tool(fsorg-probe
        SOURCES tools/fsorg-probe/main.cpp
        LIBRARIES fsorg-infrastructure Qt6::Core
        BESIDE Qt6::Core)

fsorg_add_console_tool(fsorg-packages
        SOURCES tools/fsorg-packages/main.cpp
        LIBRARIES fsorg-infrastructure Qt6::Core
        BESIDE Qt6::Core)

fsorg_add_console_tool(fsorg-startup
        SOURCES tools/fsorg-startup/main.cpp
        LIBRARIES fsorg-application fsorg-infrastructure Qt6::Core
        BESIDE Qt6::Core)

fsorg_add_console_tool(fsorg-delete
        SOURCES tools/fsorg-delete/main.cpp src/viewmodel/FailureText.cpp
        LIBRARIES fsorg-application fsorg-infrastructure Qt6::Core
        BESIDE Qt6::Core)

fsorg_add_console_tool(fsorg-adopt
        SOURCES tools/fsorg-adopt/main.cpp src/viewmodel/FailureText.cpp
        LIBRARIES fsorg-application fsorg-infrastructure Qt6::Core
        BESIDE Qt6::Core)

fsorg_add_console_tool(fsorg-install
        SOURCES tools/fsorg-install/main.cpp src/viewmodel/FailureText.cpp
        LIBRARIES fsorg-application fsorg-infrastructure Qt6::Core
        BESIDE Qt6::Core)

fsorg_add_console_tool(fsorg-bgl
        SOURCES tools/fsorg-bgl/main.cpp
        INCLUDES "${CMAKE_SOURCE_DIR}/tools"
        LIBRARIES fsorg-domain fsorg-infrastructure Qt6::Core
        BESIDE Qt6::Core)

fsorg_add_console_tool(fsorg-bisect
        SOURCES tools/fsorg-bisect/main.cpp
        LIBRARIES fsorg-application fsorg-infrastructure Qt6::Core
        BESIDE Qt6::Core)

fsorg_add_console_tool(fsorg-docs
        SOURCES tools/fsorg-docs/main.cpp ${PDF_INFRASTRUCTURE_SOURCES}
        LIBRARIES fsorg-application fsorg-infrastructure Qt6::Core Qt6::Pdf
        BESIDE Qt6::Core Qt6::Gui Qt6::Network Qt6::Pdf)

if (WIN32)
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

    add_executable(fsorg-shot
            tools/fsorg-shot/main.cpp
            ${PLATFORM_COMPOSITION_SOURCES}
            ${PDF_INFRASTRUCTURE_SOURCES}
            tools/shared/DisposableState.h
            ${NETWORK_INFRASTRUCTURE_SOURCES}
            src/infrastructure/update/GithubUpdateService.cpp
            src/infrastructure/update/NoticeOnlyUpdateService.cpp
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
            ${PLATFORM_COMPOSITION_SOURCES}
            ${PDF_INFRASTRUCTURE_SOURCES}
            tools/fsorg-timing/JournalScroll.cpp
            tools/fsorg-timing/AppScroll.cpp
            tools/fsorg-timing/LibraryScroll.cpp
            tools/fsorg-timing/OpenCosts.cpp
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
endif ()

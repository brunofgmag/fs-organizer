function(configure_fsorg_test TARGET_NAME TEST_NAME)
    target_link_libraries(${TARGET_NAME} PRIVATE Qt6::Core Qt6::Test)
    target_include_directories(${TARGET_NAME} PRIVATE "${CMAKE_SOURCE_DIR}" "${CMAKE_SOURCE_DIR}/src")
    add_test(NAME ${TEST_NAME} COMMAND ${TARGET_NAME})

    set_tests_properties(${TEST_NAME} PROPERTIES
            ENVIRONMENT "QT_ASSUME_STDERR_HAS_CONSOLE=1")

    if (WIN32)
        add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
                COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "$<TARGET_FILE:Qt6::Core>"
                "$<TARGET_FILE:Qt6::Test>"
                "$<TARGET_FILE_DIR:${TARGET_NAME}>"
                VERBATIM)
    endif ()
endfunction()

function(fsorg_add_qt_test TARGET_NAME TEST_NAME)
    add_executable(${TARGET_NAME} ${ARGN})
    configure_fsorg_test(${TARGET_NAME} ${TEST_NAME})
endfunction()

fsorg_add_qt_test(fsorg-smoke-tests smoke
        tests/tst_smoke.cpp)

fsorg_add_qt_test(fsorg-linking-engine-tests linking-engine
        tests/domain/linking/tst_linking_engine.cpp
        tests/doubles/FakeFileOperations.h
        tests/doubles/FakeLinkService.h
        tests/doubles/InMemoryFileSystem.h
        src/domain/linking/LinkingEngine.cpp)

fsorg_add_qt_test(fsorg-enabled-state-resolver-tests enabled-state-resolver
        tests/domain/linking/tst_enabled_state_resolver.cpp
        tests/doubles/FakeFileOperations.h
        tests/doubles/FakeLinkService.h
        tests/doubles/InMemoryFileSystem.h
        tests/support/PathPrinting.h
        src/domain/support/PathUtils.h
        src/domain/linking/EnabledStateResolver.cpp)

fsorg_add_qt_test(fsorg-json-manifest-parser-tests json-manifest-parser
        tests/infrastructure/catalog/tst_json_manifest_parser.cpp
        src/infrastructure/catalog/JsonManifestParser.cpp)

fsorg_add_qt_test(fsorg-filesystem-scanner-tests filesystem-scanner
        tests/infrastructure/catalog/tst_filesystem_scanner.cpp
        tests/support/PathPrinting.h
        src/infrastructure/catalog/JsonManifestParser.cpp
        src/infrastructure/catalog/FilesystemScanner.cpp)

fsorg_add_qt_test(fsorg-windows-simulator-locator-tests windows-simulator-locator
        tests/infrastructure/sim/tst_windows_simulator_locator.cpp
        tests/support/PathPrinting.h
        src/infrastructure/sim/WindowsSimulatorLocator.cpp)

if (WIN32)
    fsorg_add_qt_test(fsorg-windows-link-service-tests windows-link-service
            tests/infrastructure/link/tst_windows_link_service.cpp
            tests/support/PathPrinting.h
            src/domain/support/PathUtils.h
            src/infrastructure/link/WindowsLinkService.cpp)

    fsorg_add_qt_test(fsorg-windows-file-operations-tests windows-file-operations
            tests/infrastructure/fileops/tst_windows_file_operations.cpp
            tests/support/PathPrinting.h
            src/infrastructure/link/WindowsLinkService.cpp
            src/infrastructure/fileops/WindowsFileOperations.cpp)

    fsorg_add_qt_test(fsorg-windows-process-probe-tests windows-process-probe
            tests/infrastructure/sim/tst_windows_process_probe.cpp
            src/infrastructure/sim/WindowsProcessProbe.cpp)
endif ()

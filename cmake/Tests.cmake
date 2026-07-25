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

function(configure_fsorg_gui_test TARGET_NAME TEST_NAME)
    target_link_libraries(${TARGET_NAME} PRIVATE Qt6::Widgets dwmapi)

    set_tests_properties(${TEST_NAME} PROPERTIES
            ENVIRONMENT "QT_ASSUME_STDERR_HAS_CONSOLE=1;QT_QPA_PLATFORM=offscreen")

    add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "$<TARGET_FILE:Qt6::Gui>"
            "$<TARGET_FILE:Qt6::Widgets>"
            "$<TARGET_FILE_DIR:${TARGET_NAME}>"
            COMMAND "${CMAKE_COMMAND}" -E make_directory
            "$<TARGET_FILE_DIR:${TARGET_NAME}>/platforms"
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "$<TARGET_FILE:Qt6::QOffscreenIntegrationPlugin>"
            "$<TARGET_FILE_DIR:${TARGET_NAME}>/platforms"
            VERBATIM)
endfunction()

function(fsorg_add_qt_test TARGET_NAME TEST_NAME)
    add_executable(${TARGET_NAME} ${ARGN})
    configure_fsorg_test(${TARGET_NAME} ${TEST_NAME})
endfunction()

fsorg_add_qt_test(fsorg-smoke-tests smoke
        tests/tst_smoke.cpp)

add_test(NAME no-literal-colors
        COMMAND "${CMAKE_COMMAND}"
        "-DFSORG_SOURCE_DIR=${CMAKE_SOURCE_DIR}"
        -P "${CMAKE_SOURCE_DIR}/tools/check-no-literal-colors.cmake")

fsorg_add_qt_test(fsorg-path-utils-tests path-utils
        tests/domain/support/tst_path_utils.cpp
        tests/support/PathPrinting.h
        src/domain/support/PathUtils.h)

fsorg_add_qt_test(fsorg-linking-engine-tests linking-engine
        tests/domain/linking/tst_linking_engine.cpp
        tests/doubles/FakeFileOperations.h
        tests/doubles/FakeLinkService.h
        tests/doubles/InMemoryFileSystem.h
        src/domain/linking/LinkingEngine.cpp)

fsorg_add_qt_test(fsorg-entry-classifier-tests entry-classifier
        tests/domain/linking/tst_entry_classifier.cpp
        tests/doubles/FakeFileOperations.h
        tests/doubles/FakeLinkService.h
        tests/doubles/InMemoryFileSystem.h
        tests/support/PathPrinting.h
        src/domain/support/PathUtils.h
        src/domain/linking/EntryClassifier.cpp)

fsorg_add_qt_test(fsorg-addon-tree-tests addon-tree
        tests/domain/tree/tst_addon_tree.cpp
        tests/doubles/FakeFileOperations.h
        tests/doubles/FakeLinkService.h
        tests/doubles/InMemoryFileSystem.h
        tests/support/EnumPrinting.h
        tests/support/PathPrinting.h
        src/domain/model/EnabledAddons.h
        src/domain/support/PathUtils.h
        src/domain/linking/EntryClassifier.cpp
        src/domain/tree/AddonTree.cpp)

fsorg_add_qt_test(fsorg-effective-destination-tests effective-destination
        tests/domain/tree/tst_effective_destination.cpp
        tests/support/PathPrinting.h
        src/domain/support/PathUtils.h
        src/domain/tree/EffectiveDestination.cpp
        src/domain/tree/LibraryLookup.cpp)

fsorg_add_qt_test(fsorg-toggle-direction-tests toggle-direction
        tests/domain/tree/tst_toggle_direction.cpp
        tests/support/PathPrinting.h
        src/domain/support/PathUtils.h
        src/domain/tree/AddonTree.cpp
        src/domain/tree/EffectiveDestination.cpp
        src/domain/tree/LibraryLookup.cpp
        src/domain/tree/ToggleDirection.cpp)

fsorg_add_qt_test(fsorg-profile-service-tests profile-service
        tests/application/tst_profile_service.cpp
        tests/doubles/FakeCatalogScanner.h
        tests/doubles/FakeClock.h
        tests/doubles/FakeFileOperations.h
        tests/doubles/FakeLinkService.h
        tests/doubles/FakeOperationJournal.h
        tests/doubles/InMemoryFileSystem.h
        tests/support/EnumPrinting.h
        tests/support/PathPrinting.h
        src/domain/linking/EntryClassifier.cpp
        src/domain/linking/LinkingEngine.cpp
        src/domain/tree/AddonTree.cpp
        src/domain/tree/EffectiveDestination.cpp
        src/domain/tree/LibraryLookup.cpp
        src/domain/tree/ToggleDirection.cpp
        src/application/ProfileService.cpp)

fsorg_add_qt_test(fsorg-json-manifest-parser-tests json-manifest-parser
        tests/infrastructure/catalog/tst_json_manifest_parser.cpp
        src/infrastructure/catalog/JsonManifestParser.cpp)

fsorg_add_qt_test(fsorg-filesystem-scanner-tests filesystem-scanner
        tests/infrastructure/catalog/tst_filesystem_scanner.cpp
        tests/support/PathPrinting.h
        src/infrastructure/catalog/JsonManifestParser.cpp
        src/infrastructure/catalog/FilesystemScanner.cpp)

fsorg_add_qt_test(fsorg-jsonl-operation-journal-tests jsonl-operation-journal
        tests/infrastructure/journal/tst_jsonl_operation_journal.cpp
        src/infrastructure/journal/JsonlOperationJournal.cpp)

fsorg_add_qt_test(fsorg-json-settings-repository-tests json-settings-repository
        tests/infrastructure/settings/tst_json_settings_repository.cpp
        tests/support/PathPrinting.h
        src/infrastructure/settings/JsonSettingsRepository.cpp)

fsorg_add_qt_test(fsorg-setup-view-model-tests setup-view-model
        tests/viewmodel/tst_setup_view_model.cpp
        tests/doubles/FakeCatalogScanner.h
        tests/doubles/FakeFileOperations.h
        tests/doubles/FakeLibraryIdGenerator.h
        tests/doubles/FakeSettingsRepository.h
        tests/doubles/FakeSimulatorLocator.h
        tests/doubles/InMemoryFileSystem.h
        tests/support/PathPrinting.h
        src/domain/tree/AddonTree.cpp
        src/domain/tree/LibraryLookup.cpp
        src/viewmodel/SetupViewModel.cpp)

fsorg_add_qt_test(fsorg-addon-tree-model-tests addon-tree-model
        tests/viewmodel/tst_addon_tree_model.cpp
        tests/support/PathPrinting.h
        src/domain/linking/EntryClassifier.cpp
        src/domain/tree/AddonTree.cpp
        src/domain/tree/EffectiveDestination.cpp
        src/domain/tree/LibraryLookup.cpp
        src/viewmodel/AddonTreeModel.cpp)

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

    if (NOT FSORG_TESTS_ONLY)
        fsorg_add_qt_test(fsorg-main-window-tests main-window
                tests/view/tst_main_window.cpp
                src/view/MainWindow.cpp
                src/infrastructure/platform/WindowsTitleBar.cpp)
        configure_fsorg_gui_test(fsorg-main-window-tests main-window)
    endif ()

    fsorg_add_qt_test(fsorg-windows-process-probe-tests windows-process-probe
            tests/infrastructure/sim/tst_windows_process_probe.cpp
            src/infrastructure/sim/WindowsProcessProbe.cpp)
endif ()

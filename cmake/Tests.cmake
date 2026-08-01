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
    target_link_libraries(${TARGET_NAME} PRIVATE Qt6::Widgets "$<$<BOOL:${WIN32}>:dwmapi>")

    set_tests_properties(${TEST_NAME} PROPERTIES
            ENVIRONMENT "QT_ASSUME_STDERR_HAS_CONSOLE=1;QT_QPA_PLATFORM=offscreen")

    if (WIN32)
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
    endif ()
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

add_test(NAME no-em-dash
        COMMAND "${CMAKE_COMMAND}"
        "-DFSORG_SOURCE_DIR=${CMAKE_SOURCE_DIR}"
        -P "${CMAKE_SOURCE_DIR}/tools/check-no-em-dash.cmake")

fsorg_add_qt_test(fsorg-enum-printing-tests enum-printing
        tests/support/tst_enum_printing.cpp
        tests/support/EnumPrinting.h)

fsorg_add_qt_test(fsorg-path-utils-tests path-utils
        tests/domain/support/tst_path_utils.cpp
        tests/support/PathPrinting.h
        src/domain/support/PathUtils.h)

fsorg_add_qt_test(fsorg-in-memory-file-system-tests in-memory-file-system
        tests/doubles/tst_in_memory_file_system.cpp
        tests/doubles/InMemoryFileSystem.h
        tests/support/PathPrinting.h)

fsorg_add_qt_test(fsorg-journal-entries-tests journal-entries
        tests/domain/journal/tst_journal_entries.cpp
        tests/support/EnumPrinting.h
        tests/support/PathPrinting.h)
target_link_libraries(fsorg-journal-entries-tests PRIVATE fsorg-domain)

fsorg_add_qt_test(fsorg-copy-conflicts-tests copy-conflicts
        tests/domain/importing/tst_copy_conflicts.cpp
        tests/support/PathPrinting.h
        src/domain/support/PathUtils.h)
target_link_libraries(fsorg-copy-conflicts-tests PRIVATE fsorg-domain)

fsorg_add_qt_test(fsorg-import-engine-tests import-engine
        tests/domain/importing/tst_import_engine.cpp
        tests/doubles/FakeClock.h
        tests/doubles/FakeFileOperations.h
        tests/doubles/FakeFilesystemProbe.h
        tests/doubles/FakeLinkService.h
        tests/doubles/FakeOperationJournal.h
        tests/doubles/InMemoryFileSystem.h
        tests/support/EnumPrinting.h
        tests/support/PathPrinting.h
        src/domain/importing/ImportPaths.h)
target_link_libraries(fsorg-import-engine-tests PRIVATE fsorg-domain)

fsorg_add_qt_test(fsorg-import-service-tests import-service
        tests/application/tst_import_service.cpp
        tests/doubles/FakeClock.h
        tests/doubles/FakeFileOperations.h
        tests/doubles/FakeFilesystemProbe.h
        tests/doubles/FakeLinkService.h
        tests/doubles/FakeOperationJournal.h
        tests/doubles/FakeProcessProbe.h
        tests/doubles/InMemoryFileSystem.h
        tests/support/EnumPrinting.h
        tests/support/PathPrinting.h
        src/domain/support/PathUtils.h
        src/domain/importing/ImportPaths.h)
target_link_libraries(fsorg-import-service-tests PRIVATE fsorg-application)

fsorg_add_qt_test(fsorg-legacy-config-importer-tests legacy-config-importer
        tests/application/tst_legacy_config_importer.cpp
        tests/doubles/FakeFilesystemProbe.h
        tests/doubles/FakeLegacyConfigSource.h
        tests/doubles/InMemoryFileSystem.h
        tests/support/EnumPrinting.h
        tests/support/PathPrinting.h
        src/domain/support/PathUtils.h)
target_link_libraries(fsorg-legacy-config-importer-tests PRIVATE fsorg-application)

fsorg_add_qt_test(fsorg-library-organizer-tests library-organizer
        tests/application/tst_library_organizer.cpp
        tests/doubles/FakeCatalogScanner.h
        tests/doubles/FakeClock.h
        tests/doubles/FakeFileOperations.h
        tests/doubles/FakeFilesystemProbe.h
        tests/doubles/FakeLinkService.h
        tests/doubles/FakeOperationJournal.h
        tests/doubles/FakeProcessProbe.h
        tests/doubles/InMemoryFileSystem.h
        tests/support/EnumPrinting.h
        tests/support/PathPrinting.h
        src/domain/support/PathUtils.h)
target_link_libraries(fsorg-library-organizer-tests PRIVATE fsorg-application)

fsorg_add_qt_test(fsorg-linking-engine-tests linking-engine
        tests/domain/linking/tst_linking_engine.cpp
        tests/doubles/FakeFilesystemProbe.h
        tests/doubles/FakeLinkService.h
        tests/doubles/InMemoryFileSystem.h)
target_link_libraries(fsorg-linking-engine-tests PRIVATE fsorg-domain)

fsorg_add_qt_test(fsorg-entry-classifier-tests entry-classifier
        tests/domain/linking/tst_entry_classifier.cpp
        tests/doubles/FakeFilesystemProbe.h
        tests/doubles/FakeLinkService.h
        tests/doubles/InMemoryFileSystem.h
        tests/support/PathPrinting.h
        src/domain/support/PathUtils.h)
target_link_libraries(fsorg-entry-classifier-tests PRIVATE fsorg-domain)

fsorg_add_qt_test(fsorg-repair-plan-tests repair-plan
        tests/domain/linking/tst_repair_plan.cpp
        tests/support/EnumPrinting.h
        tests/support/PathPrinting.h
        src/domain/support/PathUtils.h)
target_link_libraries(fsorg-repair-plan-tests PRIVATE fsorg-domain)

fsorg_add_qt_test(fsorg-addon-tree-tests addon-tree
        tests/domain/tree/tst_addon_tree.cpp
        tests/doubles/FakeFilesystemProbe.h
        tests/doubles/FakeLinkService.h
        tests/doubles/InMemoryFileSystem.h
        tests/support/EnumPrinting.h
        tests/support/PathPrinting.h
        src/domain/model/EnabledAddons.h
        src/domain/support/PathUtils.h)
target_link_libraries(fsorg-addon-tree-tests PRIVATE fsorg-domain)

fsorg_add_qt_test(fsorg-category-suggester-tests category-suggester
        tests/domain/tree/tst_category_suggester.cpp
        tests/support/EnumPrinting.h
        tests/support/PathPrinting.h
        src/domain/support/PathUtils.h)
target_link_libraries(fsorg-category-suggester-tests PRIVATE fsorg-domain)

fsorg_add_qt_test(fsorg-effective-destination-tests effective-destination
        tests/domain/tree/tst_effective_destination.cpp
        tests/support/PathPrinting.h
        src/domain/support/PathUtils.h)
target_link_libraries(fsorg-effective-destination-tests PRIVATE fsorg-domain)

fsorg_add_qt_test(fsorg-toggle-direction-tests toggle-direction
        tests/domain/tree/tst_toggle_direction.cpp
        tests/support/PathPrinting.h
        src/domain/support/PathUtils.h)
target_link_libraries(fsorg-toggle-direction-tests PRIVATE fsorg-domain)

fsorg_add_qt_test(fsorg-preset-plan-tests preset-plan
        tests/domain/preset/tst_preset_plan.cpp
        tests/support/PathPrinting.h
        src/domain/support/PathUtils.h)
target_link_libraries(fsorg-preset-plan-tests PRIVATE fsorg-domain)

fsorg_add_qt_test(fsorg-legacy-preset-tests legacy-preset
        tests/domain/legacy/tst_legacy_preset.cpp
        tests/support/EnumPrinting.h
        tests/support/PathPrinting.h
        src/domain/support/PathUtils.h)
target_link_libraries(fsorg-legacy-preset-tests PRIVATE fsorg-domain)

fsorg_add_qt_test(fsorg-legacy-proposal-tests legacy-proposal
        tests/domain/legacy/tst_legacy_proposal.cpp
        tests/support/EnumPrinting.h
        tests/support/PathPrinting.h
        src/domain/support/PathUtils.h)
target_link_libraries(fsorg-legacy-proposal-tests PRIVATE fsorg-domain)

fsorg_add_qt_test(fsorg-orphan-overrides-tests orphan-overrides
        tests/domain/profile/tst_orphan_overrides.cpp
        tests/support/PathPrinting.h
        src/domain/support/PathUtils.h)
target_link_libraries(fsorg-orphan-overrides-tests PRIVATE fsorg-domain)

fsorg_add_qt_test(fsorg-profile-edits-tests profile-edits
        tests/domain/profile/tst_profile_edits.cpp
        tests/support/PathPrinting.h
        src/domain/support/PathUtils.h)
target_link_libraries(fsorg-profile-edits-tests PRIVATE fsorg-domain)

fsorg_add_qt_test(fsorg-profile-service-tests profile-service
        tests/application/tst_profile_service.cpp
        tests/doubles/FakeCatalogScanner.h
        tests/doubles/FakeClock.h
        tests/doubles/FakeFilesystemProbe.h
        tests/doubles/FakeLinkService.h
        tests/doubles/FakeOperationJournal.h
        tests/doubles/InMemoryFileSystem.h
        tests/support/EnumPrinting.h
        tests/support/PathPrinting.h)
target_link_libraries(fsorg-profile-service-tests PRIVATE fsorg-application)

fsorg_add_qt_test(fsorg-preset-service-tests preset-service
        tests/application/tst_preset_service.cpp
        tests/doubles/FakeCatalogScanner.h
        tests/doubles/FakeClock.h
        tests/doubles/FakeFilesystemProbe.h
        tests/doubles/FakeLibraryIdGenerator.h
        tests/doubles/FakeLinkService.h
        tests/doubles/FakeOperationJournal.h
        tests/doubles/FakePresetRepository.h
        tests/doubles/InMemoryFileSystem.h
        tests/support/EnumPrinting.h
        tests/support/PathPrinting.h)
target_link_libraries(fsorg-preset-service-tests PRIVATE fsorg-application)

fsorg_add_qt_test(fsorg-session-tests session
        tests/application/tst_session.cpp
        tests/doubles/FakeCatalogScanner.h
        tests/doubles/FakeClock.h
        tests/doubles/FakeFileOperations.h
        tests/doubles/FakeFilesystemProbe.h
        tests/doubles/FakeLibraryIdGenerator.h
        tests/doubles/FakeLinkService.h
        tests/doubles/FakeOperationJournal.h
        tests/doubles/FakeProcessProbe.h
        tests/doubles/FakeSettingsRepository.h
        tests/doubles/InMemoryFileSystem.h
        tests/doubles/InlineBackgroundRunner.h
        tests/doubles/RecordingSessionObserver.h
        tests/support/EnumPrinting.h
        tests/support/PathPrinting.h)
target_link_libraries(fsorg-session-tests PRIVATE fsorg-application)

fsorg_add_qt_test(fsorg-json-manifest-parser-tests json-manifest-parser
        tests/infrastructure/catalog/tst_json_manifest_parser.cpp
        src/infrastructure/catalog/JsonManifestParser.cpp)

fsorg_add_qt_test(fsorg-filesystem-scanner-tests filesystem-scanner
        tests/infrastructure/catalog/tst_filesystem_scanner.cpp
        tests/support/PathPrinting.h
        tests/support/StdFilesystemProbe.h
        src/domain/importing/ImportPaths.h
        src/infrastructure/catalog/JsonManifestParser.cpp
        src/infrastructure/catalog/FilesystemScanner.cpp)

fsorg_add_qt_test(fsorg-jsonl-operation-journal-tests jsonl-operation-journal
        tests/infrastructure/journal/tst_jsonl_operation_journal.cpp
        src/infrastructure/journal/JsonlOperationJournal.cpp)

fsorg_add_qt_test(fsorg-json-settings-repository-tests json-settings-repository
        tests/infrastructure/settings/tst_json_settings_repository.cpp
        tests/support/EnumPrinting.h
        tests/support/PathPrinting.h
        src/infrastructure/settings/JsonSettingsRepository.cpp)

fsorg_add_qt_test(fsorg-file-preset-repository-tests file-preset-repository
        tests/infrastructure/preset/tst_file_preset_repository.cpp
        tests/support/EnumPrinting.h
        tests/support/PathPrinting.h
        src/infrastructure/preset/FilePresetRepository.cpp)

fsorg_add_qt_test(fsorg-ini-legacy-config-reader-tests ini-legacy-config-reader
        tests/infrastructure/legacy/tst_ini_legacy_config_reader.cpp
        src/domain/support/PathUtils.h
        src/infrastructure/legacy/IniLegacyConfigReader.cpp)

fsorg_add_qt_test(fsorg-legacy-preset-reader-tests legacy-preset-reader
        tests/infrastructure/legacy/tst_legacy_preset_reader.cpp
        tests/support/PathPrinting.h
        src/domain/support/PathUtils.h
        src/infrastructure/legacy/IniLegacyConfigReader.cpp
        src/infrastructure/legacy/LegacyPresetReader.cpp)

fsorg_add_qt_test(fsorg-windows-legacy-config-source-tests windows-legacy-config-source
        tests/infrastructure/legacy/tst_windows_legacy_config_source.cpp
        src/domain/support/PathUtils.h
        src/infrastructure/legacy/IniLegacyConfigReader.cpp
        src/infrastructure/legacy/LegacyPresetReader.cpp
        src/infrastructure/legacy/WindowsLegacyConfigSource.cpp)

fsorg_add_qt_test(fsorg-github-release-parser-tests github-release-parser
        tests/infrastructure/update/tst_github_release_parser.cpp
        src/infrastructure/update/GithubReleaseParser.cpp)
target_compile_definitions(fsorg-github-release-parser-tests PRIVATE
        FSORG_FIXTURES_DIR=\"${CMAKE_SOURCE_DIR}/tests/fixtures\")

if (NOT FSORG_TESTS_ONLY)
    fsorg_add_qt_test(fsorg-github-update-service-tests github-update-service
            tests/infrastructure/update/tst_github_update_service.cpp
            src/infrastructure/update/GithubReleaseParser.cpp
            src/infrastructure/update/GithubUpdateService.cpp)
    target_link_libraries(fsorg-github-update-service-tests PRIVATE Qt6::Network)

    if (WIN32)
        add_custom_command(TARGET fsorg-github-update-service-tests POST_BUILD
                COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "$<TARGET_FILE:Qt6::Network>"
                "$<TARGET_FILE_DIR:fsorg-github-update-service-tests>"
                VERBATIM)
    endif ()
endif ()

fsorg_add_qt_test(fsorg-update-view-model-tests update-view-model
        tests/viewmodel/tst_update_view_model.cpp
        tests/doubles/FakeUpdateService.h)
target_link_libraries(fsorg-update-view-model-tests PRIVATE fsorg-viewmodel)

fsorg_add_qt_test(fsorg-setup-view-model-tests setup-view-model
        tests/viewmodel/tst_setup_view_model.cpp
        tests/doubles/FakeCatalogScanner.h
        tests/doubles/FakeFilesystemProbe.h
        tests/doubles/FakeLibraryIdGenerator.h
        tests/doubles/FakeSettingsRepository.h
        tests/doubles/FakeSimulatorLocator.h
        tests/doubles/InMemoryFileSystem.h
        tests/support/PathPrinting.h)
target_link_libraries(fsorg-setup-view-model-tests PRIVATE fsorg-viewmodel)

fsorg_add_qt_test(fsorg-addon-tree-model-tests addon-tree-model
        tests/viewmodel/tst_addon_tree_model.cpp
        tests/support/PathPrinting.h)
target_link_libraries(fsorg-addon-tree-model-tests PRIVATE fsorg-viewmodel)

fsorg_add_qt_test(fsorg-addon-tree-filter-model-tests addon-tree-filter-model
        tests/viewmodel/tst_addon_tree_filter_model.cpp
        tests/support/PathPrinting.h)
target_link_libraries(fsorg-addon-tree-filter-model-tests PRIVATE fsorg-viewmodel)

fsorg_add_qt_test(fsorg-category-suggestion-model-tests category-suggestion-model
        tests/viewmodel/tst_category_suggestion_model.cpp
        tests/support/PathPrinting.h)
target_link_libraries(fsorg-category-suggestion-model-tests PRIVATE fsorg-viewmodel)

fsorg_add_qt_test(fsorg-community-model-tests community-model
        tests/viewmodel/tst_community_model.cpp
        tests/support/EnumPrinting.h
        tests/support/PathPrinting.h)
target_link_libraries(fsorg-community-model-tests PRIVATE fsorg-viewmodel)

fsorg_add_qt_test(fsorg-journal-model-tests journal-model
        tests/viewmodel/tst_journal_model.cpp
        tests/support/PathPrinting.h)
target_link_libraries(fsorg-journal-model-tests PRIVATE fsorg-viewmodel)

fsorg_add_qt_test(fsorg-failure-text-tests failure-text
        tests/viewmodel/tst_failure_text.cpp)
target_link_libraries(fsorg-failure-text-tests PRIVATE fsorg-viewmodel)

fsorg_add_qt_test(fsorg-quarantine-model-tests quarantine-model
        tests/viewmodel/tst_quarantine_model.cpp
        tests/support/PathPrinting.h)
target_link_libraries(fsorg-quarantine-model-tests PRIVATE fsorg-viewmodel)

fsorg_add_qt_test(fsorg-addon-tree-view-model-tests addon-tree-view-model
        tests/viewmodel/tst_addon_tree_view_model.cpp
        tests/doubles/FakeCatalogScanner.h
        tests/doubles/FakeClock.h
        tests/doubles/FakeFileOperations.h
        tests/doubles/FakeFilesystemProbe.h
        tests/doubles/FakeLibraryIdGenerator.h
        tests/doubles/FakeLinkService.h
        tests/doubles/FakeOperationJournal.h
        tests/doubles/FakeProcessProbe.h
        tests/doubles/FakeSettingsRepository.h
        tests/doubles/InMemoryFileSystem.h
        tests/doubles/InlineBackgroundRunner.h
        tests/support/EnumPrinting.h
        tests/support/PathPrinting.h)
target_link_libraries(fsorg-addon-tree-view-model-tests PRIVATE fsorg-viewmodel)

fsorg_add_qt_test(fsorg-options-view-model-tests options-view-model
        tests/viewmodel/tst_options_view_model.cpp
        tests/doubles/FakeCatalogScanner.h
        tests/doubles/FakeClock.h
        tests/doubles/FakeFileOperations.h
        tests/doubles/FakeFilesystemProbe.h
        tests/doubles/FakeLibraryIdGenerator.h
        tests/doubles/FakeLinkService.h
        tests/doubles/FakeOperationJournal.h
        tests/doubles/FakeProcessProbe.h
        tests/doubles/FakeSettingsRepository.h
        tests/doubles/InMemoryFileSystem.h
        tests/doubles/InlineBackgroundRunner.h
        tests/support/EnumPrinting.h
        tests/support/PathPrinting.h)
target_link_libraries(fsorg-options-view-model-tests PRIVATE fsorg-viewmodel)

fsorg_add_qt_test(fsorg-preset-view-model-tests preset-view-model
        tests/viewmodel/tst_preset_view_model.cpp
        tests/doubles/FakeCatalogScanner.h
        tests/doubles/FakeClock.h
        tests/doubles/FakeFileOperations.h
        tests/doubles/FakeFilesystemProbe.h
        tests/doubles/FakeLibraryIdGenerator.h
        tests/doubles/FakeLinkService.h
        tests/doubles/FakeOperationJournal.h
        tests/doubles/FakePresetRepository.h
        tests/doubles/FakeProcessProbe.h
        tests/doubles/FakeSettingsRepository.h
        tests/doubles/InMemoryFileSystem.h
        tests/doubles/InlineBackgroundRunner.h
        tests/support/EnumPrinting.h
        tests/support/PathPrinting.h)
target_link_libraries(fsorg-preset-view-model-tests PRIVATE fsorg-viewmodel)

fsorg_add_qt_test(fsorg-community-view-model-tests community-view-model
        tests/viewmodel/tst_community_view_model.cpp
        tests/doubles/FakeCatalogScanner.h
        tests/doubles/FakeClock.h
        tests/doubles/FakeFileOperations.h
        tests/doubles/FakeFilesystemProbe.h
        tests/doubles/FakeLinkService.h
        tests/doubles/FakeOperationJournal.h
        tests/doubles/FakeProcessProbe.h
        tests/doubles/FakeSettingsRepository.h
        tests/doubles/InMemoryFileSystem.h
        tests/doubles/InlineBackgroundRunner.h
        tests/support/EnumPrinting.h
        tests/support/PathPrinting.h)
target_link_libraries(fsorg-community-view-model-tests PRIVATE fsorg-viewmodel)

fsorg_add_qt_test(fsorg-quarantine-view-model-tests quarantine-view-model
        tests/viewmodel/tst_quarantine_view_model.cpp
        tests/doubles/FakeCatalogScanner.h
        tests/doubles/FakeClock.h
        tests/doubles/FakeFileOperations.h
        tests/doubles/FakeFilesystemProbe.h
        tests/doubles/FakeLibraryIdGenerator.h
        tests/doubles/FakeLinkService.h
        tests/doubles/FakeOperationJournal.h
        tests/doubles/FakeProcessProbe.h
        tests/doubles/FakeSettingsRepository.h
        tests/doubles/InMemoryFileSystem.h
        tests/doubles/InlineBackgroundRunner.h
        tests/support/EnumPrinting.h
        tests/support/PathPrinting.h)
target_link_libraries(fsorg-quarantine-view-model-tests PRIVATE fsorg-viewmodel)

fsorg_add_qt_test(fsorg-import-view-model-tests import-view-model
        tests/viewmodel/tst_import_view_model.cpp
        tests/doubles/FakeCatalogScanner.h
        tests/doubles/FakeClock.h
        tests/doubles/FakeFileOperations.h
        tests/doubles/FakeFilesystemProbe.h
        tests/doubles/FakeLibraryIdGenerator.h
        tests/doubles/FakeLinkService.h
        tests/doubles/FakeOperationJournal.h
        tests/doubles/FakeProcessProbe.h
        tests/doubles/FakeSettingsRepository.h
        tests/doubles/InMemoryFileSystem.h
        tests/doubles/InlineBackgroundRunner.h
        tests/support/EnumPrinting.h
        tests/support/PathPrinting.h)
target_link_libraries(fsorg-import-view-model-tests PRIVATE fsorg-viewmodel)

fsorg_add_qt_test(fsorg-windows-simulator-locator-tests windows-simulator-locator
        tests/infrastructure/sim/tst_windows_simulator_locator.cpp
        tests/support/PathPrinting.h
        src/infrastructure/sim/WindowsSimulatorLocator.cpp)

if (WIN32)
    fsorg_add_qt_test(fsorg-windows-link-service-tests windows-link-service
            tests/infrastructure/link/tst_windows_link_service.cpp
            tests/support/EnumPrinting.h
            tests/support/PathPrinting.h
            src/domain/support/PathUtils.h
            src/infrastructure/link/WindowsLinkService.cpp)

    fsorg_add_qt_test(fsorg-windows-file-operations-tests windows-file-operations
            tests/infrastructure/fileops/tst_windows_file_operations.cpp
            tests/support/PathPrinting.h
            src/infrastructure/link/WindowsLinkService.cpp
            src/infrastructure/fileops/WindowsFileOperations.cpp)

    fsorg_add_qt_test(fsorg-import-on-real-disk-tests import-on-real-disk
            tests/infrastructure/importing/tst_import_on_real_disk.cpp
            tests/support/EnumPrinting.h
            tests/support/PathPrinting.h
            src/domain/importing/ImportPaths.h
            src/infrastructure/catalog/FilesystemScanner.cpp
            src/infrastructure/catalog/JsonManifestParser.cpp
            src/infrastructure/fileops/WindowsFileOperations.cpp
            src/infrastructure/fileops/WindowsFilesystemProbe.cpp
            src/infrastructure/journal/JsonlOperationJournal.cpp
            src/infrastructure/link/WindowsLinkService.cpp
            src/infrastructure/sim/WindowsProcessProbe.cpp)
    target_link_libraries(fsorg-import-on-real-disk-tests PRIVATE fsorg-application)

    fsorg_add_qt_test(fsorg-options-on-real-disk-tests options-on-real-disk
            tests/infrastructure/options/tst_options_on_real_disk.cpp
            tests/doubles/InlineBackgroundRunner.h
            tests/doubles/RecordingSessionObserver.h
            tests/support/EnumPrinting.h
            tests/support/PathPrinting.h
            src/infrastructure/catalog/FilesystemScanner.cpp
            src/infrastructure/catalog/JsonManifestParser.cpp
            src/infrastructure/fileops/WindowsFileOperations.cpp
            src/infrastructure/fileops/WindowsFilesystemProbe.cpp
            src/infrastructure/id/UuidLibraryIdGenerator.cpp
            src/infrastructure/journal/JsonlOperationJournal.cpp
            src/infrastructure/link/WindowsLinkService.cpp
            src/infrastructure/settings/JsonSettingsRepository.cpp
            src/infrastructure/sim/WindowsProcessProbe.cpp)
    target_link_libraries(fsorg-options-on-real-disk-tests PRIVATE fsorg-application)

    fsorg_add_qt_test(fsorg-preset-on-real-disk-tests preset-on-real-disk
            tests/infrastructure/preset/tst_preset_on_real_disk.cpp
            tests/doubles/FakeLibraryIdGenerator.h
            tests/support/EnumPrinting.h
            tests/support/PathPrinting.h
            src/infrastructure/catalog/FilesystemScanner.cpp
            src/infrastructure/catalog/JsonManifestParser.cpp
            src/infrastructure/fileops/WindowsFilesystemProbe.cpp
            src/infrastructure/journal/JsonlOperationJournal.cpp
            src/infrastructure/link/WindowsLinkService.cpp
            src/infrastructure/preset/FilePresetRepository.cpp)
    target_link_libraries(fsorg-preset-on-real-disk-tests PRIVATE fsorg-application)

    fsorg_add_qt_test(fsorg-windows-filesystem-probe-tests windows-filesystem-probe
            tests/infrastructure/fileops/tst_windows_filesystem_probe.cpp
            tests/support/PathPrinting.h
            src/infrastructure/link/WindowsLinkService.cpp
            src/infrastructure/fileops/WindowsFilesystemProbe.cpp)

    if (NOT FSORG_TESTS_ONLY)
        fsorg_add_qt_test(fsorg-main-window-tests main-window
                tests/view/tst_main_window.cpp
                assets/resources.qrc
                ${WINDOWS_SHELL_SOURCES}
                src/view/platform/WindowsTitleBar.cpp)
        target_link_libraries(fsorg-main-window-tests PRIVATE fsorg-view)
        configure_fsorg_gui_test(fsorg-main-window-tests main-window)



        fsorg_add_qt_test(fsorg-presets-page-tests presets-page
                tests/view/tst_presets_page.cpp
                assets/resources.qrc
                tests/doubles/FakeCatalogScanner.h
                tests/doubles/FakeClock.h
                tests/doubles/FakeFileOperations.h
                tests/doubles/FakeFilesystemProbe.h
                tests/doubles/FakeLibraryIdGenerator.h
                tests/doubles/FakeLinkService.h
                tests/doubles/FakeOperationJournal.h
                tests/doubles/FakePresetRepository.h
                tests/doubles/FakeProcessProbe.h
                tests/doubles/FakeSettingsRepository.h
                tests/doubles/InMemoryFileSystem.h
                tests/doubles/InlineBackgroundRunner.h
                tests/support/EnumPrinting.h
                tests/support/PathPrinting.h)
        target_link_libraries(fsorg-presets-page-tests PRIVATE fsorg-view)
        configure_fsorg_gui_test(fsorg-presets-page-tests presets-page)


        fsorg_add_qt_test(fsorg-options-page-tests options-page
                tests/view/tst_options_page.cpp
                assets/resources.qrc
                tests/doubles/FakeCatalogScanner.h
                tests/doubles/FakeClock.h
                tests/doubles/FakeFileOperations.h
                tests/doubles/FakeFilesystemProbe.h
                tests/doubles/FakeLibraryIdGenerator.h
                tests/doubles/FakeLinkService.h
                tests/doubles/FakeOperationJournal.h
                tests/doubles/FakeProcessProbe.h
                tests/doubles/FakeSettingsRepository.h
                tests/doubles/InMemoryFileSystem.h
                tests/doubles/InlineBackgroundRunner.h
                tests/doubles/FakeUpdateService.h)
        target_link_libraries(fsorg-options-page-tests PRIVATE fsorg-view)
        configure_fsorg_gui_test(fsorg-options-page-tests options-page)

        fsorg_add_qt_test(fsorg-legacy-import-dialog-tests legacy-import-dialog
                tests/view/tst_legacy_import_dialog.cpp
                tests/doubles/FakeCatalogScanner.h
                tests/doubles/FakeClock.h
                tests/doubles/FakeFileOperations.h
                tests/doubles/FakeFilesystemProbe.h
                tests/doubles/FakeLegacyConfigSource.h
                tests/doubles/FakeLibraryIdGenerator.h
                tests/doubles/FakeLinkService.h
                tests/doubles/FakeOperationJournal.h
                tests/doubles/FakePresetRepository.h
                tests/doubles/FakeProcessProbe.h
                tests/doubles/FakeSettingsRepository.h
                tests/doubles/InMemoryFileSystem.h
                tests/doubles/InlineBackgroundRunner.h
                tests/doubles/RecordingSessionObserver.h)
        target_link_libraries(fsorg-legacy-import-dialog-tests PRIVATE fsorg-view)
        configure_fsorg_gui_test(fsorg-legacy-import-dialog-tests legacy-import-dialog)

        fsorg_add_qt_test(fsorg-community-page-tests community-page
                tests/view/tst_community_page.cpp
                tests/doubles/FakeCatalogScanner.h
                tests/doubles/FakeClock.h
                tests/doubles/FakeFileOperations.h
                tests/doubles/FakeFilesystemProbe.h
                tests/doubles/FakeLibraryIdGenerator.h
                tests/doubles/FakeLinkService.h
                tests/doubles/FakeOperationJournal.h
                tests/doubles/FakeProcessProbe.h
                tests/doubles/FakeSettingsRepository.h
                tests/doubles/InMemoryFileSystem.h
                tests/doubles/InlineBackgroundRunner.h
                tests/support/EnumPrinting.h
                tests/support/PathPrinting.h
                assets/resources.qrc)
        target_link_libraries(fsorg-community-page-tests PRIVATE fsorg-view)
        configure_fsorg_gui_test(fsorg-community-page-tests community-page)

    endif ()

    fsorg_add_qt_test(fsorg-windows-process-probe-tests windows-process-probe
            tests/infrastructure/sim/tst_windows_process_probe.cpp
            src/infrastructure/sim/WindowsProcessProbe.cpp)

    fsorg_add_qt_test(fsorg-single-instance-tests single-instance
            tests/infrastructure/platform/tst_single_instance.cpp
            src/infrastructure/platform/SingleInstance.cpp)
endif ()

fsorg_add_qt_test(fsorg-table-columns-tests table-columns
        tests/view/tst_table_columns.cpp)
target_link_libraries(fsorg-table-columns-tests PRIVATE fsorg-view)
configure_fsorg_gui_test(fsorg-table-columns-tests table-columns)

fsorg_add_qt_test(fsorg-wheel-guard-tests wheel-guard
        tests/view/tst_wheel_guard.cpp)
target_link_libraries(fsorg-wheel-guard-tests PRIVATE fsorg-view)
configure_fsorg_gui_test(fsorg-wheel-guard-tests wheel-guard)

fsorg_add_qt_test(fsorg-context-panel-tests context-panel
        tests/view/panels/tst_context_panel.cpp
        assets/resources.qrc)
target_link_libraries(fsorg-context-panel-tests PRIVATE fsorg-view)
configure_fsorg_gui_test(fsorg-context-panel-tests context-panel)

fsorg_add_qt_test(fsorg-row-delegate-tests row-delegate
        tests/view/delegates/tst_row_delegate.cpp
        assets/resources.qrc)
target_link_libraries(fsorg-row-delegate-tests PRIVATE fsorg-view)
configure_fsorg_gui_test(fsorg-row-delegate-tests row-delegate)

fsorg_add_qt_test(fsorg-modernist-theme-tests modernist-theme
        tests/view/theme/tst_modernist_theme.cpp
        assets/resources.qrc)
target_link_libraries(fsorg-modernist-theme-tests PRIVATE fsorg-view)

configure_fsorg_gui_test(fsorg-modernist-theme-tests modernist-theme)

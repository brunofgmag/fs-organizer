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
        tests/support/PathPrinting.h
        src/domain/journal/JournalEntries.cpp)

fsorg_add_qt_test(fsorg-copy-conflicts-tests copy-conflicts
        tests/domain/importing/tst_copy_conflicts.cpp
        tests/support/PathPrinting.h
        src/domain/support/PathUtils.h
        src/domain/tree/AddonTree.cpp
        src/domain/importing/CopyConflicts.cpp)

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
        src/domain/journal/OperationLog.cpp
        src/domain/linking/LinkingEngine.cpp
        src/domain/tree/LibraryLookup.cpp
        src/domain/importing/ImportPaths.h
        src/domain/importing/ImportEngine.cpp)

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
        src/domain/journal/OperationLog.cpp
        src/domain/linking/DisableLinks.cpp
        src/domain/linking/EntryClassifier.cpp
        src/domain/linking/LinkingEngine.cpp
        src/domain/tree/AddonTree.cpp
        src/domain/tree/LibraryLookup.cpp
        src/domain/tree/LibraryTrees.cpp
        src/domain/importing/ImportPaths.h
        src/domain/importing/ImportEngine.cpp
        src/application/ImportService.cpp)

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
        src/domain/support/PathUtils.h
        src/domain/journal/OperationLog.cpp
        src/domain/linking/DisableLinks.cpp
        src/domain/linking/EntryClassifier.cpp
        src/domain/linking/LinkingEngine.cpp
        src/domain/tree/AddonTree.cpp
        src/domain/tree/EffectiveDestination.cpp
        src/domain/tree/LibraryLookup.cpp
        src/domain/tree/LibraryTrees.cpp
        src/application/LibraryOrganizer.cpp)

fsorg_add_qt_test(fsorg-linking-engine-tests linking-engine
        tests/domain/linking/tst_linking_engine.cpp
        tests/doubles/FakeFilesystemProbe.h
        tests/doubles/FakeLinkService.h
        tests/doubles/InMemoryFileSystem.h
        src/domain/linking/LinkingEngine.cpp)

fsorg_add_qt_test(fsorg-entry-classifier-tests entry-classifier
        tests/domain/linking/tst_entry_classifier.cpp
        tests/doubles/FakeFilesystemProbe.h
        tests/doubles/FakeLinkService.h
        tests/doubles/InMemoryFileSystem.h
        tests/support/PathPrinting.h
        src/domain/support/PathUtils.h
        src/domain/linking/EntryClassifier.cpp)

fsorg_add_qt_test(fsorg-repair-plan-tests repair-plan
        tests/domain/linking/tst_repair_plan.cpp
        tests/support/EnumPrinting.h
        tests/support/PathPrinting.h
        src/domain/support/PathUtils.h
        src/domain/tree/AddonTree.cpp
        src/domain/tree/LibraryLookup.cpp
        src/domain/linking/RepairPlan.cpp)

fsorg_add_qt_test(fsorg-addon-tree-tests addon-tree
        tests/domain/tree/tst_addon_tree.cpp
        tests/doubles/FakeFilesystemProbe.h
        tests/doubles/FakeLinkService.h
        tests/doubles/InMemoryFileSystem.h
        tests/support/EnumPrinting.h
        tests/support/PathPrinting.h
        src/domain/model/EnabledAddons.h
        src/domain/support/PathUtils.h
        src/domain/linking/EntryClassifier.cpp
        src/domain/tree/AddonTree.cpp)

fsorg_add_qt_test(fsorg-category-suggester-tests category-suggester
        tests/domain/tree/tst_category_suggester.cpp
        tests/support/EnumPrinting.h
        tests/support/PathPrinting.h
        src/domain/support/PathUtils.h
        src/domain/tree/AddonTree.cpp
        src/domain/tree/CategorySuggester.cpp)

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

fsorg_add_qt_test(fsorg-preset-plan-tests preset-plan
        tests/domain/preset/tst_preset_plan.cpp
        tests/support/PathPrinting.h
        src/domain/support/PathUtils.h
        src/domain/tree/AddonTree.cpp
        src/domain/tree/LibraryLookup.cpp
        src/domain/preset/PresetPlan.cpp)

fsorg_add_qt_test(fsorg-profile-service-tests profile-service
        tests/application/tst_profile_service.cpp
        tests/doubles/FakeCatalogScanner.h
        tests/doubles/FakeClock.h
        tests/doubles/FakeFilesystemProbe.h
        tests/doubles/FakeLinkService.h
        tests/doubles/FakeOperationJournal.h
        tests/doubles/InMemoryFileSystem.h
        tests/support/EnumPrinting.h
        tests/support/PathPrinting.h
        src/domain/journal/OperationLog.cpp
        src/domain/importing/CopyConflicts.cpp
        src/domain/linking/EntryClassifier.cpp
        src/domain/linking/LinkingEngine.cpp
        src/domain/linking/RepairPlan.cpp
        src/domain/tree/AddonTree.cpp
        src/domain/tree/EffectiveDestination.cpp
        src/domain/tree/LibraryLookup.cpp
        src/domain/tree/LibraryTrees.cpp
        src/domain/tree/ToggleDirection.cpp
        src/application/ProfileService.cpp)

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
        tests/support/PathPrinting.h
        src/domain/journal/OperationLog.cpp
        src/domain/importing/CopyConflicts.cpp
        src/domain/linking/EntryClassifier.cpp
        src/domain/linking/LinkingEngine.cpp
        src/domain/linking/RepairPlan.cpp
        src/domain/preset/PresetPlan.cpp
        src/domain/tree/AddonTree.cpp
        src/domain/tree/EffectiveDestination.cpp
        src/domain/tree/LibraryLookup.cpp
        src/domain/tree/LibraryTrees.cpp
        src/domain/tree/ToggleDirection.cpp
        src/application/ProfileService.cpp
        src/application/PresetService.cpp)

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
        tests/support/PathPrinting.h
        src/domain/journal/OperationLog.cpp
        src/domain/importing/CopyConflicts.cpp
        src/domain/linking/DisableLinks.cpp
        src/domain/linking/EntryClassifier.cpp
        src/domain/linking/LinkingEngine.cpp
        src/domain/linking/RepairPlan.cpp
        src/domain/tree/AddonTree.cpp
        src/domain/tree/EffectiveDestination.cpp
        src/domain/tree/LibraryLookup.cpp
        src/domain/tree/LibraryTrees.cpp
        src/domain/tree/ToggleDirection.cpp
        src/application/LibraryOrganizer.cpp
        src/application/ProfileService.cpp
        src/application/Session.cpp)

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
        tests/support/PathPrinting.h
        src/infrastructure/settings/JsonSettingsRepository.cpp)

fsorg_add_qt_test(fsorg-file-preset-repository-tests file-preset-repository
        tests/infrastructure/preset/tst_file_preset_repository.cpp
        tests/support/EnumPrinting.h
        tests/support/PathPrinting.h
        src/infrastructure/preset/FilePresetRepository.cpp)

fsorg_add_qt_test(fsorg-setup-view-model-tests setup-view-model
        tests/viewmodel/tst_setup_view_model.cpp
        tests/doubles/FakeCatalogScanner.h
        tests/doubles/FakeFilesystemProbe.h
        tests/doubles/FakeLibraryIdGenerator.h
        tests/doubles/FakeSettingsRepository.h
        tests/doubles/FakeSimulatorLocator.h
        tests/doubles/InMemoryFileSystem.h
        tests/support/PathPrinting.h
        src/domain/tree/AddonTree.cpp
        src/domain/tree/LibraryLookup.cpp
        src/application/SetupService.cpp
        src/viewmodel/SetupViewModel.cpp)

fsorg_add_qt_test(fsorg-addon-tree-model-tests addon-tree-model
        tests/viewmodel/tst_addon_tree_model.cpp
        tests/support/PathPrinting.h
        src/domain/importing/CopyConflicts.cpp
        src/domain/linking/EntryClassifier.cpp
        src/domain/tree/AddonTree.cpp
        src/domain/tree/EffectiveDestination.cpp
        src/domain/tree/DestinationDivergence.cpp
        src/domain/tree/LibraryLookup.cpp
        src/viewmodel/AddonTreeModel.cpp)

fsorg_add_qt_test(fsorg-addon-tree-filter-model-tests addon-tree-filter-model
        tests/viewmodel/tst_addon_tree_filter_model.cpp
        tests/support/PathPrinting.h
        src/domain/importing/CopyConflicts.cpp
        src/domain/linking/EntryClassifier.cpp
        src/domain/tree/AddonTree.cpp
        src/domain/tree/EffectiveDestination.cpp
        src/domain/tree/DestinationDivergence.cpp
        src/domain/tree/LibraryLookup.cpp
        src/viewmodel/AddonTreeFilterModel.cpp
        src/viewmodel/AddonTreeModel.cpp)

fsorg_add_qt_test(fsorg-category-suggestion-model-tests category-suggestion-model
        tests/viewmodel/tst_category_suggestion_model.cpp
        tests/support/PathPrinting.h
        src/domain/tree/AddonTree.cpp
        src/domain/tree/CategorySuggester.cpp
        src/viewmodel/CategorySuggestionModel.cpp
        src/viewmodel/FailureText.cpp)

fsorg_add_qt_test(fsorg-community-model-tests community-model
        tests/viewmodel/tst_community_model.cpp
        tests/support/EnumPrinting.h
        tests/support/PathPrinting.h
        src/domain/tree/AddonTree.cpp
        src/domain/importing/CopyConflicts.cpp
        src/viewmodel/CommunityModel.cpp)

fsorg_add_qt_test(fsorg-journal-model-tests journal-model
        tests/viewmodel/tst_journal_model.cpp
        tests/support/PathPrinting.h
        src/domain/journal/JournalEntries.cpp
        src/viewmodel/FailureText.cpp
        src/viewmodel/JournalModel.cpp)

fsorg_add_qt_test(fsorg-failure-text-tests failure-text
        tests/viewmodel/tst_failure_text.cpp
        src/viewmodel/FailureText.cpp)

fsorg_add_qt_test(fsorg-quarantine-model-tests quarantine-model
        tests/viewmodel/tst_quarantine_model.cpp
        tests/support/PathPrinting.h
        src/viewmodel/QuarantineModel.cpp)

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
        tests/support/PathPrinting.h
        src/domain/journal/OperationLog.cpp
        src/domain/importing/CopyConflicts.cpp
        src/domain/linking/DisableLinks.cpp
        src/domain/linking/EntryClassifier.cpp
        src/domain/linking/LinkingEngine.cpp
        src/domain/linking/RepairPlan.cpp
        src/domain/tree/AddonTree.cpp
        src/domain/tree/CategorySuggester.cpp
        src/domain/tree/DestinationDivergence.cpp
        src/domain/tree/EffectiveDestination.cpp
        src/domain/tree/LibraryLookup.cpp
        src/domain/tree/LibraryTrees.cpp
        src/domain/tree/ToggleDirection.cpp
        src/application/LibraryOrganizer.cpp
        src/application/ProfileService.cpp
        src/application/Session.cpp
        src/viewmodel/AddonTreeModel.cpp
        src/viewmodel/AddonTreeViewModel.cpp
        src/viewmodel/FailureText.cpp
        src/viewmodel/SessionNotifier.cpp)

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
        tests/support/PathPrinting.h
        src/domain/journal/OperationLog.cpp
        src/domain/importing/CopyConflicts.cpp
        src/domain/linking/DisableLinks.cpp
        src/domain/linking/EntryClassifier.cpp
        src/domain/linking/LinkingEngine.cpp
        src/domain/linking/RepairPlan.cpp
        src/domain/preset/PresetPlan.cpp
        src/domain/tree/AddonTree.cpp
        src/domain/tree/EffectiveDestination.cpp
        src/domain/tree/LibraryLookup.cpp
        src/domain/tree/LibraryTrees.cpp
        src/domain/tree/ToggleDirection.cpp
        src/application/LibraryOrganizer.cpp
        src/application/PresetService.cpp
        src/application/ProfileService.cpp
        src/application/Session.cpp
        src/viewmodel/PresetViewModel.cpp
        src/viewmodel/SessionNotifier.cpp)

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
        tests/support/PathPrinting.h
        src/domain/journal/OperationLog.cpp
        src/domain/importing/CopyConflicts.cpp
        src/domain/linking/DisableLinks.cpp
        src/domain/linking/EntryClassifier.cpp
        src/domain/linking/LinkingEngine.cpp
        src/domain/linking/RepairPlan.cpp
        src/domain/tree/AddonTree.cpp
        src/domain/tree/EffectiveDestination.cpp
        src/domain/tree/LibraryLookup.cpp
        src/domain/tree/LibraryTrees.cpp
        src/domain/tree/ToggleDirection.cpp
        src/application/LibraryOrganizer.cpp
        src/application/ProfileService.cpp
        src/application/Session.cpp
        src/viewmodel/CommunityModel.cpp
        src/viewmodel/CommunityViewModel.cpp
        src/viewmodel/SessionNotifier.cpp)

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
        tests/support/PathPrinting.h
        src/domain/journal/OperationLog.cpp
        src/domain/importing/CopyConflicts.cpp
        src/domain/importing/ImportEngine.cpp
        src/domain/linking/DisableLinks.cpp
        src/domain/linking/EntryClassifier.cpp
        src/domain/linking/LinkingEngine.cpp
        src/domain/linking/RepairPlan.cpp
        src/domain/tree/AddonTree.cpp
        src/domain/tree/EffectiveDestination.cpp
        src/domain/tree/LibraryLookup.cpp
        src/domain/tree/LibraryTrees.cpp
        src/domain/tree/ToggleDirection.cpp
        src/application/ImportService.cpp
        src/application/LibraryOrganizer.cpp
        src/application/ProfileService.cpp
        src/application/Session.cpp
        src/viewmodel/QuarantineModel.cpp
        src/viewmodel/QuarantineViewModel.cpp
        src/viewmodel/SessionNotifier.cpp)

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
        tests/support/PathPrinting.h
        src/domain/journal/OperationLog.cpp
        src/domain/importing/CopyConflicts.cpp
        src/domain/importing/ImportEngine.cpp
        src/domain/linking/DisableLinks.cpp
        src/domain/linking/EntryClassifier.cpp
        src/domain/linking/LinkingEngine.cpp
        src/domain/linking/RepairPlan.cpp
        src/domain/tree/AddonTree.cpp
        src/domain/tree/EffectiveDestination.cpp
        src/domain/tree/LibraryLookup.cpp
        src/domain/tree/LibraryTrees.cpp
        src/domain/tree/ToggleDirection.cpp
        src/application/ImportService.cpp
        src/application/LibraryOrganizer.cpp
        src/application/ProfileService.cpp
        src/application/Session.cpp
        src/viewmodel/FailureText.cpp
        src/viewmodel/ImportViewModel.cpp
        src/viewmodel/SessionNotifier.cpp)

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

    fsorg_add_qt_test(fsorg-import-on-real-disk-tests import-on-real-disk
            tests/infrastructure/importing/tst_import_on_real_disk.cpp
            tests/support/EnumPrinting.h
            tests/support/PathPrinting.h
            src/domain/journal/OperationLog.cpp
            src/application/ImportService.cpp
            src/application/LibraryOrganizer.cpp
            src/domain/importing/ImportEngine.cpp
            src/domain/importing/ImportPaths.h
            src/domain/linking/DisableLinks.cpp
            src/domain/linking/EntryClassifier.cpp
            src/domain/linking/LinkingEngine.cpp
            src/domain/tree/AddonTree.cpp
            src/domain/tree/EffectiveDestination.cpp
            src/domain/tree/LibraryLookup.cpp
            src/domain/tree/LibraryTrees.cpp
            src/infrastructure/catalog/FilesystemScanner.cpp
            src/infrastructure/catalog/JsonManifestParser.cpp
            src/infrastructure/fileops/WindowsFileOperations.cpp
            src/infrastructure/fileops/WindowsFilesystemProbe.cpp
            src/infrastructure/journal/JsonlOperationJournal.cpp
            src/infrastructure/link/WindowsLinkService.cpp
            src/infrastructure/sim/WindowsProcessProbe.cpp)

    fsorg_add_qt_test(fsorg-preset-on-real-disk-tests preset-on-real-disk
            tests/infrastructure/preset/tst_preset_on_real_disk.cpp
            tests/doubles/FakeLibraryIdGenerator.h
            tests/support/EnumPrinting.h
            tests/support/PathPrinting.h
            src/domain/journal/OperationLog.cpp
            src/domain/importing/CopyConflicts.cpp
            src/domain/linking/EntryClassifier.cpp
            src/domain/linking/LinkingEngine.cpp
            src/domain/linking/RepairPlan.cpp
            src/domain/preset/PresetPlan.cpp
            src/domain/tree/AddonTree.cpp
            src/domain/tree/EffectiveDestination.cpp
            src/domain/tree/LibraryLookup.cpp
            src/domain/tree/LibraryTrees.cpp
            src/application/PresetService.cpp
            src/application/ProfileService.cpp
            src/infrastructure/catalog/FilesystemScanner.cpp
            src/infrastructure/catalog/JsonManifestParser.cpp
            src/infrastructure/fileops/WindowsFilesystemProbe.cpp
            src/infrastructure/journal/JsonlOperationJournal.cpp
            src/infrastructure/link/WindowsLinkService.cpp
            src/infrastructure/preset/FilePresetRepository.cpp)

    fsorg_add_qt_test(fsorg-windows-filesystem-probe-tests windows-filesystem-probe
            tests/infrastructure/fileops/tst_windows_filesystem_probe.cpp
            tests/support/PathPrinting.h
            src/infrastructure/link/WindowsLinkService.cpp
            src/infrastructure/fileops/WindowsFilesystemProbe.cpp)

    if (NOT FSORG_TESTS_ONLY)
        fsorg_add_qt_test(fsorg-main-window-tests main-window
                tests/view/tst_main_window.cpp
                assets/resources.qrc
                src/view/shell/MainWindow.cpp
                src/view/WheelGuard.cpp
                src/view/shell/TriageStrip.cpp
                src/view/theme/ModernistPaint.cpp
                src/view/theme/ModernistStyle.cpp
                src/view/theme/ModernistTheme.cpp
                src/view/theme/ModernistTones.cpp
                src/view/theme/PageTab.cpp
                src/view/platform/WindowsTitleBar.cpp)
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
                tests/support/PathPrinting.h
                src/domain/journal/OperationLog.cpp
                src/domain/importing/CopyConflicts.cpp
                src/domain/linking/DisableLinks.cpp
                src/domain/linking/EntryClassifier.cpp
                src/domain/linking/LinkingEngine.cpp
                src/domain/linking/RepairPlan.cpp
                src/domain/preset/PresetPlan.cpp
                src/domain/tree/AddonTree.cpp
                src/domain/tree/EffectiveDestination.cpp
                src/domain/tree/LibraryLookup.cpp
                src/domain/tree/LibraryTrees.cpp
                src/domain/tree/ToggleDirection.cpp
                src/application/LibraryOrganizer.cpp
                src/application/PresetService.cpp
                src/application/ProfileService.cpp
                src/application/Session.cpp
                src/view/PresetsPage.cpp
                src/view/delegates/RowDelegate.cpp
                src/view/TableColumns.cpp
                src/view/panels/ContextPanel.cpp
                src/view/panels/EmptyState.cpp
                src/view/panels/PanelRail.cpp
                src/view/theme/ModernistPaint.cpp
                src/view/theme/ModernistStyle.cpp
                src/view/theme/ModernistTheme.cpp
                src/view/theme/ModernistTones.cpp
                src/viewmodel/PresetViewModel.cpp
                src/viewmodel/SessionNotifier.cpp)
        configure_fsorg_gui_test(fsorg-presets-page-tests presets-page)


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
                assets/resources.qrc
                src/domain/journal/OperationLog.cpp
                src/domain/importing/CopyConflicts.cpp
                src/domain/importing/ImportEngine.cpp
                src/domain/linking/DisableLinks.cpp
                src/domain/linking/EntryClassifier.cpp
                src/domain/linking/LinkingEngine.cpp
                src/domain/linking/RepairPlan.cpp
                src/domain/tree/AddonTree.cpp
                src/domain/tree/EffectiveDestination.cpp
                src/domain/tree/LibraryLookup.cpp
                src/domain/tree/LibraryTrees.cpp
                src/domain/tree/ToggleDirection.cpp
                src/application/ImportService.cpp
                src/application/LibraryOrganizer.cpp
                src/application/ProfileService.cpp
                src/application/Session.cpp
                src/view/community/CommunityPage.cpp
                src/view/community/ConflictDialog.cpp
                src/view/community/ImportDialog.cpp
                src/view/community/RepairDialog.cpp
                src/view/delegates/RowDelegate.cpp
                src/view/TableColumns.cpp
                src/view/WheelGuard.cpp
                src/view/panels/ContextPanel.cpp
                src/view/panels/ModelRowDetail.cpp
                src/view/panels/PanelRail.cpp
                src/view/theme/ModernistPaint.cpp
                src/view/theme/ModernistTones.cpp
                src/viewmodel/CommunityModel.cpp
                src/viewmodel/CommunityViewModel.cpp
                src/viewmodel/FailureText.cpp
                src/viewmodel/ImportViewModel.cpp
                src/viewmodel/SessionNotifier.cpp)
        configure_fsorg_gui_test(fsorg-community-page-tests community-page)

    endif ()

    fsorg_add_qt_test(fsorg-windows-process-probe-tests windows-process-probe
            tests/infrastructure/sim/tst_windows_process_probe.cpp
            src/infrastructure/sim/WindowsProcessProbe.cpp)
endif ()

fsorg_add_qt_test(fsorg-table-columns-tests table-columns
        tests/view/tst_table_columns.cpp
        src/view/TableColumns.cpp)
configure_fsorg_gui_test(fsorg-table-columns-tests table-columns)
fsorg_add_qt_test(fsorg-wheel-guard-tests wheel-guard
        tests/view/tst_wheel_guard.cpp
        src/view/community/RepairDialog.cpp
        src/view/setup/StagingLeftoverDialog.cpp
        src/view/WheelGuard.cpp)
configure_fsorg_gui_test(fsorg-wheel-guard-tests wheel-guard)
fsorg_add_qt_test(fsorg-context-panel-tests context-panel
        tests/view/panels/tst_context_panel.cpp
        src/view/panels/ContextPanel.cpp
        src/view/panels/ModelRowDetail.cpp
        src/view/panels/PanelRail.cpp
        src/view/shell/TriageStrip.cpp
        src/view/theme/ModernistPaint.cpp
        src/view/theme/ModernistTones.cpp)
configure_fsorg_gui_test(fsorg-context-panel-tests context-panel)
fsorg_add_qt_test(fsorg-modernist-theme-tests modernist-theme
        tests/view/theme/tst_modernist_theme.cpp
        assets/resources.qrc
        src/view/delegates/RowDelegate.cpp
        src/view/theme/ModernistPaint.cpp
        src/view/theme/ModernistStyle.cpp
        src/view/theme/ModernistTheme.cpp
        src/view/theme/ModernistTones.cpp)
configure_fsorg_gui_test(fsorg-modernist-theme-tests modernist-theme)

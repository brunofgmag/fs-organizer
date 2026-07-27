set(DOMAIN_SOURCES
        src/domain/importing/CopyConflicts.cpp
        src/domain/importing/ImportEngine.cpp
        src/domain/journal/JournalEntries.cpp
        src/domain/journal/OperationLog.cpp
        src/domain/linking/DisableLinks.cpp
        src/domain/linking/EntryClassifier.cpp
        src/domain/linking/LinkingEngine.cpp
        src/domain/linking/RepairPlan.cpp
        src/domain/tree/AddonTree.cpp
        src/domain/tree/CategorySuggester.cpp
        src/domain/tree/EffectiveDestination.cpp
        src/domain/tree/LibraryLookup.cpp
        src/domain/tree/LibraryTrees.cpp
        src/domain/tree/ToggleDirection.cpp
)

set(APPLICATION_SOURCES
        src/application/ImportService.cpp
        src/application/LibraryOrganizer.cpp
        src/application/ProfileService.cpp
        src/application/Session.cpp
)

set(INFRASTRUCTURE_SOURCES
        src/infrastructure/catalog/FilesystemScanner.cpp
        src/infrastructure/catalog/JsonManifestParser.cpp
        src/infrastructure/fileops/WindowsFileOperations.cpp
        src/infrastructure/fileops/WindowsFilesystemProbe.cpp
        src/infrastructure/id/UuidLibraryIdGenerator.cpp
        src/infrastructure/journal/JsonlOperationJournal.cpp
        src/infrastructure/link/WindowsLinkService.cpp
        src/infrastructure/platform/WindowsKnownFolders.cpp
        src/infrastructure/platform/WindowsTitleBar.cpp
        src/infrastructure/settings/JsonSettingsRepository.cpp
        src/infrastructure/sim/WindowsProcessProbe.cpp
        src/infrastructure/sim/WindowsSimulatorLocator.cpp
        src/infrastructure/sim/WindowsUserCfgLocations.cpp
)

set(VIEWMODEL_SOURCES
        src/viewmodel/AddonTreeFilterModel.cpp
        src/viewmodel/AddonTreeModel.cpp
        src/viewmodel/AddonTreeViewModel.cpp
        src/viewmodel/CommunityModel.cpp
        src/viewmodel/CommunityViewModel.cpp
        src/viewmodel/FailureText.cpp
        src/viewmodel/ImportViewModel.cpp
        src/viewmodel/JournalModel.cpp
        src/viewmodel/JournalViewModel.cpp
        src/viewmodel/QtBackgroundRunner.cpp
        src/viewmodel/QuarantineModel.cpp
        src/viewmodel/QuarantineViewModel.cpp
        src/viewmodel/SessionNotifier.cpp
        src/viewmodel/SetupViewModel.cpp
)

set(VIEW_SOURCES
        src/view/AddonTreePage.cpp
        src/view/CommunityPage.cpp
        src/view/ConflictDialog.cpp
        src/view/ImportDialog.cpp
        src/view/JournalPage.cpp
        src/view/MainWindow.cpp
        src/view/PlainTextDelegate.cpp
        src/view/QuarantinePage.cpp
        src/view/RepairDialog.cpp
        src/view/SetupWizard.cpp
        src/view/StagingLeftoverDialog.cpp
        src/view/TableColumns.cpp
)

set(APP_SOURCES
        src/main.cpp
        ${DOMAIN_SOURCES}
        ${APPLICATION_SOURCES}
        ${INFRASTRUCTURE_SOURCES}
        ${VIEWMODEL_SOURCES}
        ${VIEW_SOURCES}
)

if (EXISTS "${CMAKE_SOURCE_DIR}/assets/branding/app.ico")
    list(APPEND APP_SOURCES assets/app.rc)
endif ()

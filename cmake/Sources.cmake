set(DOMAIN_SOURCES
        src/domain/linking/EntryClassifier.cpp
        src/domain/linking/LinkingEngine.cpp
        src/domain/linking/RepairPlan.cpp
        src/domain/tree/AddonTree.cpp
        src/domain/tree/EffectiveDestination.cpp
        src/domain/tree/LibraryLookup.cpp
        src/domain/tree/ToggleDirection.cpp
)

set(APPLICATION_SOURCES
        src/application/ProfileService.cpp
)

set(INFRASTRUCTURE_SOURCES
        src/infrastructure/catalog/FilesystemScanner.cpp
        src/infrastructure/catalog/JsonManifestParser.cpp
        src/infrastructure/fileops/WindowsFileOperations.cpp
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
        src/viewmodel/SetupViewModel.cpp
)

set(VIEW_SOURCES
        src/view/AddonTreePage.cpp
        src/view/CommunityPage.cpp
        src/view/FailureText.cpp
        src/view/MainWindow.cpp
        src/view/RepairDialog.cpp
        src/view/SetupWizard.cpp
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

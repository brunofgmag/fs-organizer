set(DOMAIN_SOURCES
        src/domain/linking/EnabledStateResolver.cpp
        src/domain/linking/LinkingEngine.cpp
        src/domain/tree/AddonTree.cpp
        src/domain/tree/EffectiveDestination.cpp
        src/domain/tree/LibraryLookup.cpp
        src/domain/tree/ToggleDirection.cpp
)

set(APPLICATION_SOURCES
        src/application/AddonService.cpp
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
        src/viewmodel/SetupViewModel.cpp
)

set(VIEW_SOURCES
        src/view/MainWindow.cpp
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

#ifndef FS_ORGANIZER_VIEWMODEL_INSTALL_OFFER_H
#define FS_ORGANIZER_VIEWMODEL_INSTALL_OFFER_H

#include <vector>

#include <QtCore/QString>

enum class SourceKind
{
    Archive,
    Folder,
};

enum class InstallStanding
{
    New,
    Replaces,
    ReplacesAndTheQuarantineHoldsOne,
    AFolderIsInTheWay,
};

struct InstallStandingInALibrary
{
    InstallStanding standing = InstallStanding::New;
    QString installedVersion{};
    QString installedIn{};
    QString quarantinedVersion{};
    QString occupant{};
};

struct InstallAddonLine
{
    QString folderName{};
    QString version{};
    qulonglong bytes = 0;
    std::vector<InstallStandingInALibrary> inEachLibrary{};
    QString root{};
    std::vector<std::vector<InstallStandingInALibrary>> inEachCategory{};
};

struct InstallArchiveLine
{
    QString fileName{};
    std::vector<InstallAddonLine> addons{};
    QString refusal{};
    QString path{};
    SourceKind source = SourceKind::Archive;
};

struct InstallCategoryChoice
{
    QString shown{};
    QString path{};
};

struct InstallLibraryChoice
{
    QString shown{};
    QString path{};
    std::vector<InstallCategoryChoice> categories{};
};

struct InstallOffer
{
    std::vector<InstallArchiveLine> archives{};
    std::vector<InstallLibraryChoice> libraries{};
    QString droppedOn{};
    QString runningSimulator{};
};

struct ChosenAddon
{
    int archive = 0;
    int addon = 0;
    bool discardsTheQuarantinedCopy = false;
    QString archivePath{};
    QString root{};
    QString folderName{};
    QString replaces{};
    SourceKind source = SourceKind::Archive;
};

enum class InstallStep
{
    Extracting,
    Copying,
    Checking,
    PuttingIntoPlace,
};

enum class InstallReportKind
{
    NothingToInstall,
    SomeWereNotInstalled,
    NothingWasInstalled,
    InstalledButNotLinked,
    InstalledButAFolderStayed,
};

struct InstallReportLine
{
    QString addon{};
    QString reason{};
};

struct InstallReportArchive
{
    QString fileName{};
    std::vector<InstallReportLine> lines{};
    QString leftBehind{};
};

struct InstallReport
{
    InstallReportKind kind = InstallReportKind::NothingToInstall;
    std::vector<InstallReportArchive> archives{};
    int installed = 0;
    int notInstalled = 0;
};

#endif // FS_ORGANIZER_VIEWMODEL_INSTALL_OFFER_H

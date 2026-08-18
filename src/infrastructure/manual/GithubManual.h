#ifndef FS_ORGANIZER_INFRASTRUCTURE_MANUAL_GITHUB_MANUAL_H
#define FS_ORGANIZER_INFRASTRUCTURE_MANUAL_GITHUB_MANUAL_H

#include <filesystem>
#include <string>
#include <vector>

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtNetwork/QNetworkAccessManager>

#include "application/ports/ManualSource.h"

class QNetworkReply;

class GithubManual final : public QObject, public ManualSource
{
    Q_OBJECT

public:
    GithubManual(std::string version, std::filesystem::path folder, QObject* parent = nullptr);

    [[nodiscard]] std::filesystem::path WhereTheManualWouldBe(const std::string& language) const override;

    [[nodiscard]] bool TheManualIsHere(const std::string& language) const override;

    void FetchTheManual(const std::string& language) override;

    void AddObserver(ManualSourceObserver* observer) override;

    void RemoveObserver(ManualSourceObserver* observer) override;

private:
    void OnFetchFinished();

    void ForgetTheOlderCopies(const std::filesystem::path& kept) const;

    void SayItArrived(bool ok, const std::filesystem::path& file, const QString& error) const;

    QNetworkAccessManager network_;
    std::string version_;
    std::filesystem::path folder_;
    std::vector<ManualSourceObserver*> observers_;

    QNetworkReply* fetch_ = nullptr;
    std::filesystem::path fetching_{};
};

#endif // FS_ORGANIZER_INFRASTRUCTURE_MANUAL_GITHUB_MANUAL_H

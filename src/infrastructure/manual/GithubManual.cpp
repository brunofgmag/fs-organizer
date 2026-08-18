#include "infrastructure/manual/GithubManual.h"

#include <utility>
#include <vector>

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QUrl>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkRequest>

#include "application/ManualCopy.h"
#include "support/PathText.h"

namespace
{
    constexpr int kTransferTimeoutMs = 30000;

    QString HttpError(const QNetworkReply* reply)
    {
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

        return status > 0 ? QStringLiteral("HTTP %1").arg(status) : reply->errorString();
    }

    bool WriteTheDownload(QNetworkReply* reply, const QString& file)
    {
        QFile pdf(file);
        if (!pdf.open(QIODevice::WriteOnly | QIODevice::Truncate))
        {
            return false;
        }

        const qint64 written = pdf.write(reply->readAll());
        pdf.close();

        return written > 0;
    }
}

GithubManual::GithubManual(std::string version, std::filesystem::path folder, QObject* parent)
    : QObject(parent), version_(std::move(version)), folder_(std::move(folder))
{
    network_.setTransferTimeout(kTransferTimeoutMs);
}

std::filesystem::path GithubManual::WhereTheManualWouldBe(const std::string& language) const
{
    return ManualFileIn(folder_, version_, language);
}

bool GithubManual::TheManualIsHere(const std::string& language) const
{
    const QFileInfo kept(AsText(WhereTheManualWouldBe(language)));

    return kept.isFile() && kept.size() > 0;
}

void GithubManual::FetchTheManual(const std::string& language)
{
    if (fetch_ != nullptr)
    {
        return;
    }

    fetching_ = WhereTheManualWouldBe(language);

    QNetworkRequest asking{QUrl(QString::fromStdString(ManualUrlFor(version_, language)))};
    asking.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

    fetch_ = network_.get(asking);

    connect(fetch_, &QNetworkReply::finished, this, &GithubManual::OnFetchFinished);
}

void GithubManual::OnFetchFinished()
{
    QNetworkReply* reply = std::exchange(fetch_, nullptr);
    reply->deleteLater();

    const std::filesystem::path file = std::exchange(fetching_, {});

    if (reply->error() != QNetworkReply::NoError)
    {
        SayItArrived(false, {}, HttpError(reply));

        return;
    }

    if (!QDir().mkpath(AsText(folder_)) || !WriteTheDownload(reply, AsText(file)))
    {
        SayItArrived(false, {}, tr("The manual came down but could not be written to %1.").arg(AsText(file)));

        return;
    }

    ForgetTheOlderCopies(file);

    SayItArrived(true, file, {});
}

std::vector<std::filesystem::path> ManualCopiesToForget(const std::filesystem::path& folder,
                                                        const std::filesystem::path& kept)
{
    const QString keeping = AsText(kept.filename());

    std::vector<std::filesystem::path> forgotten;

    for (const QFileInfo& older : QDir(AsText(folder)).entryInfoList(QDir::Files))
    {
        if (older.fileName() != keeping && ItIsAManualCopy(AsPath(older.fileName())))
        {
            forgotten.push_back(AsPath(older.absoluteFilePath()));
        }
    }

    return forgotten;
}

void GithubManual::ForgetTheOlderCopies(const std::filesystem::path& kept) const
{
    for (const std::filesystem::path& older : ManualCopiesToForget(folder_, kept))
    {
        static_cast<void>(QFile::remove(AsText(older)));
    }
}

void GithubManual::SayItArrived(const bool ok, const std::filesystem::path& file, const QString& error) const
{
    for (ManualSourceObserver* observer : observers_)
    {
        observer->OnManualFetched(ok, file, error.toStdString());
    }
}

void GithubManual::AddObserver(ManualSourceObserver* observer)
{
    observers_.push_back(observer);
}

void GithubManual::RemoveObserver(ManualSourceObserver* observer)
{
    std::erase(observers_, observer);
}

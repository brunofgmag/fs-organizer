#include "infrastructure/archive/TarExeArchiveReader.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>

#include <QtCore/QByteArray>
#include <QtCore/QElapsedTimer>
#include <QtCore/QProcess>
#include <QtCore/QRegularExpression>
#include <QtCore/QStringList>

#include <algorithm>
#include <array>
#include <cstdint>
#include <iterator>
#include <limits>
#include <optional>
#include <system_error>
#include <unordered_map>
#include <utility>

#include "infrastructure/archive/EmptyStagingFolder.h"
#include "support/PathText.h"

namespace
{
    constexpr int kStartTimeMs = 10000;
    constexpr int kReapTimeMs = 10000;
    constexpr int kTickMs = 50;
    constexpr std::chrono::milliseconds kHeartbeat{250};
    constexpr qsizetype kNoCap = std::numeric_limits<qsizetype>::max();
    constexpr QStringView kPatternCharacters = u"\\[]*?";
    constexpr QStringView kSeams = u"\\/':";
    constexpr std::array<QStringView, 3> kPasswordWords{u"passphrase", u"encrypt", u"password"};

    struct Run
    {
        bool started = false;
        bool timedOut = false;
        bool truncated = false;
        int exitCode = -1;
        QByteArray out{};
        QByteArray err{};
    };

    struct Printout
    {
        ArchiveFailure failure = ArchiveFailure::None;
        QString text{};
    };

    struct Details
    {
        QChar mode{};
        std::uintmax_t bytes = 0;
    };

    enum class Ending
    {
        Finished,
        Cancelled,
        Stalled,
    };

    [[nodiscard]] const QRegularExpression& DetailLine()
    {
        static const QRegularExpression line(QStringLiteral(R"(^(\S+)\s+\d+\s+\S+\s+\S+\s+(\d+)(?:\s|$))"));

        return line;
    }

    [[nodiscard]] std::string NormalisedPath(const QString& printed)
    {
        QByteArray path = printed.toUtf8();
        path.replace('\\', '/');
        while (path.startsWith("./"))
        {
            path.remove(0, 2);
        }
        while (path.endsWith('/'))
        {
            path.chop(1);
        }
        if (path == ".")
        {
            path.clear();
        }

        return path.toStdString();
    }

    [[nodiscard]] ArchiveEntryKind KindOf(const QChar mode)
    {
        switch (mode.toLatin1())
        {
        case 'd': return ArchiveEntryKind::Folder;
        case '-': return ArchiveEntryKind::File;
        default: return ArchiveEntryKind::Other;
        }
    }

    [[nodiscard]] std::optional<Details> DetailsOf(const QStringView text)
    {
        const QRegularExpressionMatch match = DetailLine().matchView(text);
        if (!match.hasMatch())
        {
            return std::nullopt;
        }

        bool sized = false;
        const qulonglong bytes = match.capturedView(2).toULongLong(&sized);
        if (!sized)
        {
            return std::nullopt;
        }

        return Details{.mode = match.capturedView(1).front(), .bytes = bytes};
    }

    [[nodiscard]] std::vector<QStringView> PrintedLines(const QStringView printed)
    {
        std::vector<QStringView> lines;
        for (const QStringView line : printed.split(u'\n'))
        {
            const QStringView text = line.endsWith(u'\r') ? line.chopped(1) : line;
            if (!text.isEmpty())
            {
                lines.push_back(text);
            }
        }

        return lines;
    }

    [[nodiscard]] int ExitCodeOf(const QProcess& process)
    {
        return process.exitStatus() == QProcess::NormalExit ? process.exitCode() : -1;
    }

    [[nodiscard]] QStringList Spelled(const QString& name)
    {
        QString backslashed = name;
        backslashed.replace(u'/', u'\\');

        QString doubled = backslashed;
        doubled.replace(u'\\', QStringLiteral("\\\\"));

        return {name, backslashed, doubled};
    }

    [[nodiscard]] bool IsAtASeam(const QString& text, const qsizetype at, const qsizetype length)
    {
        const qsizetype after = at + length;

        return at > 0 && kSeams.contains(text[at - 1]) && (after == text.size() || kSeams.contains(text[after]));
    }

    void EraseAtSeams(QString& text, const QString& name)
    {
        for (qsizetype at = text.indexOf(name, 0, Qt::CaseInsensitive); at >= 0;
             at = text.indexOf(name, at, Qt::CaseInsensitive))
        {
            if (IsAtASeam(text, at, name.size()))
            {
                text.remove(at, name.size());
                continue;
            }

            ++at;
        }
    }

    [[nodiscard]] QString WithoutKnownNames(QString message, const QStringList& names)
    {
        QStringList spellings;
        for (const QString& name : names)
        {
            if (!name.isEmpty())
            {
                spellings += Spelled(name);
            }
        }

        std::ranges::sort(spellings,
                          [](const QString& left, const QString& right)
                          {
                              return left.size() > right.size();
                          });

        for (const QString& spelling : spellings)
        {
            EraseAtSeams(message, spelling);
        }

        return message;
    }

    [[nodiscard]] bool MentionsAPassword(const QStringView text)
    {
        return std::ranges::any_of(kPasswordWords,
                                   [text](const QStringView word)
                                   {
                                       return text.contains(word, Qt::CaseInsensitive);
                                   });
    }

    [[nodiscard]] QString FirstLinesMentioningAPassword(const QString& message)
    {
        QStringList kept;
        for (const QStringView line : QStringView(message).split(u'\n'))
        {
            if (!MentionsAPassword(line))
            {
                continue;
            }

            kept << line.toString();
            if (kept.size() == kMostLinesChecked)
            {
                break;
            }
        }

        return kept.join(u'\n');
    }

    [[nodiscard]] QStringList NamesMentioningAPassword(const QStringList& names)
    {
        QStringList kept;
        std::ranges::copy_if(names, std::back_inserter(kept),
                             [](const QString& name)
                             {
                                 return MentionsAPassword(name);
                             });

        return kept;
    }

    [[nodiscard]] ArchiveFailure
    PasswordOr(const QString& message, const QStringList& names, const ArchiveFailure otherwise)
    {
        return AsksForAPassword(message, names) ? ArchiveFailure::NeedsAPassword : otherwise;
    }

    [[nodiscard]] QString TextOf(const std::string& entry)
    {
        return QString::fromUtf8(entry.data(), static_cast<qsizetype>(entry.size()));
    }

    [[nodiscard]] QStringList NamesInTheMessages(const std::filesystem::path& archive,
                                                 const std::filesystem::path& into,
                                                 const std::vector<ArchiveEntry>& listed)
    {
        QStringList names{AsText(into), AsText(archive)};
        for (const ArchiveEntry& entry : listed)
        {
            names << TextOf(entry.path);
        }

        return names;
    }

    [[nodiscard]] QString WithoutMemberNames(const QString& printed)
    {
        QStringList kept;
        for (const QStringView line : QStringView(printed).split(u'\n'))
        {
            if (!line.startsWith(u"x "))
            {
                kept << line.toString();
                continue;
            }

            const qsizetype message = line.indexOf(u": ");
            if (message >= 0)
            {
                kept << line.mid(message).toString();
            }
        }

        return kept.join(u'\n');
    }

    [[nodiscard]] ArchiveFailure FailureToStart(const std::filesystem::path& tool, const ArchiveFailure otherwise)
    {
        std::error_code error;

        return std::filesystem::exists(tool, error) ? otherwise : ArchiveFailure::TheToolIsMissing;
    }

    [[nodiscard]] ArchiveFailure
    FailureBeforeTheExit(const Run& run, const std::filesystem::path& tool, const ArchiveFailure otherwise)
    {
        if (!run.started)
        {
            return FailureToStart(tool, otherwise);
        }

        return run.timedOut ? ArchiveFailure::TookTooLong : ArchiveFailure::None;
    }

    [[nodiscard]] QString Escaped(const std::string& entry)
    {
        const QString text = QString::fromUtf8(entry.data(), static_cast<qsizetype>(entry.size()));

        QString escaped;
        for (const QChar letter : text)
        {
            if (kPatternCharacters.contains(letter))
            {
                escaped += u'\\';
            }
            escaped += letter;
        }

        return escaped;
    }

    [[nodiscard]] QString WithoutTheEntry(QString printed, const std::string& entry)
    {
        const QString quoted = QString::fromUtf8(entry.data(), static_cast<qsizetype>(entry.size()));
        for (const QString& name : {quoted, Escaped(entry)})
        {
            printed.replace(QStringLiteral(": ") + name + QStringLiteral(": "), QStringLiteral(": "));
        }

        return printed;
    }

    void Reap(QProcess& process)
    {
        process.kill();
        process.waitForFinished(kReapTimeMs);
    }

    [[nodiscard]] Run RunWithin(const std::filesystem::path& tool,
                                const QStringList& arguments,
                                const std::chrono::milliseconds limit,
                                const qsizetype keep)
    {
        QProcess process;
        process.start(AsText(tool), arguments);

        Run run;
        run.started = process.waitForStarted(kStartTimeMs);
        if (!run.started)
        {
            return run;
        }
        process.closeWriteChannel();

        QElapsedTimer clock;
        clock.start();

        bool finished = false;
        while (!finished && !run.timedOut && !run.truncated)
        {
            finished = process.waitForFinished(kTickMs);
            run.out += process.readAllStandardOutput();
            run.err += process.readAllStandardError();
            run.truncated = run.out.size() > keep;
            run.timedOut = !finished && clock.hasExpired(limit.count());
        }

        if (!finished)
        {
            Reap(process);
        }
        run.exitCode = ExitCodeOf(process);

        return run;
    }

    [[nodiscard]] Printout PrintedByListing(const std::filesystem::path& tool,
                                            const QString& flag,
                                            const std::filesystem::path& archive,
                                            const std::chrono::milliseconds limit)
    {
        const Run run = RunWithin(tool, {flag, AsText(archive)}, limit, kNoCap);

        const ArchiveFailure unfinished = FailureBeforeTheExit(run, tool, ArchiveFailure::CouldNotOpen);
        if (unfinished != ArchiveFailure::None)
        {
            return {.failure = unfinished, .text = {}};
        }
        if (run.exitCode != 0)
        {
            return {.failure =
                        PasswordOr(QString::fromLocal8Bit(run.err), {AsText(archive)}, ArchiveFailure::CouldNotOpen),
                    .text = {}};
        }

        return {.failure = ArchiveFailure::None, .text = QString::fromLocal8Bit(run.out)};
    }

    class ProcessActivity
    {
    public:
        ProcessActivity(const qint64 processId, const std::chrono::milliseconds limit)
            : handle_(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, static_cast<DWORD>(processId))),
              limit_(limit)
        {
            last_ = Counted();
            sinceMove_.start();
        }

        ProcessActivity(const ProcessActivity&) = delete;
        ProcessActivity& operator=(const ProcessActivity&) = delete;

        ~ProcessActivity()
        {
            if (handle_ != nullptr)
            {
                CloseHandle(handle_);
            }
        }

        [[nodiscard]] bool HasStalled()
        {
            const std::optional<Counters> now = Counted();
            if (!now.has_value() || now != last_)
            {
                last_ = now;
                sinceMove_.restart();
            }

            return sinceMove_.hasExpired(limit_.count());
        }

    private:
        using Counters = std::array<ULONGLONG, 4>;

        [[nodiscard]] std::optional<Counters> Counted() const
        {
            IO_COUNTERS counters{};
            if (handle_ == nullptr || !GetProcessIoCounters(handle_, &counters))
            {
                return std::nullopt;
            }

            return Counters{counters.ReadOperationCount, counters.WriteOperationCount, counters.ReadTransferCount,
                            counters.WriteTransferCount};
        }

        HANDLE handle_;
        std::chrono::milliseconds limit_;
        std::optional<Counters> last_{};
        QElapsedTimer sinceMove_{};
    };

    class ExtractionWatch
    {
    public:
        ExtractionWatch(QProcess& process,
                        const std::vector<ArchiveEntry>& listed,
                        const std::function<bool(const CopyProgress&)>& onProgress,
                        const std::chrono::milliseconds idleLimit)
            : process_(process), onProgress_(onProgress), activity_(process.processId(), idleLimit)
        {
            for (const ArchiveEntry& entry : listed)
            {
                if (entry.kind == ArchiveEntryKind::File)
                {
                    sizes_.emplace(entry.path, entry.bytes);
                    progress_.totalBytes += entry.bytes;
                }
            }
            sinceReport_.start();
        }

        [[nodiscard]] Ending Follow()
        {
            bool finished = false;
            while (!finished)
            {
                finished = process_.waitForFinished(kTickMs);
                if (!Take() || !StillWanted())
                {
                    return Ending::Cancelled;
                }
                if (!finished && activity_.HasStalled())
                {
                    return Ending::Stalled;
                }
            }

            return Extracted(std::exchange(pending_, {})) ? Ending::Finished : Ending::Cancelled;
        }

        [[nodiscard]] const QByteArray& Printed() const
        {
            return printed_;
        }

    private:
        [[nodiscard]] bool Take()
        {
            const QByteArray arrived = process_.readAllStandardError();
            printed_ += arrived;
            pending_ += arrived;

            for (qsizetype end = pending_.indexOf('\n'); end >= 0; end = pending_.indexOf('\n'))
            {
                const QByteArray line = pending_.left(end);
                pending_.remove(0, end + 1);
                if (!Extracted(line))
                {
                    return false;
                }
            }

            return true;
        }

        [[nodiscard]] bool Extracted(const QByteArray& line)
        {
            const QString text = QString::fromLocal8Bit(line.endsWith('\r') ? line.chopped(1) : line);
            if (!text.startsWith(QStringLiteral("x ")))
            {
                return true;
            }

            const auto size = sizes_.find(NormalisedPath(text.mid(2)));
            progress_.copiedBytes += size == sizes_.end() ? 0 : size->second;

            return Report();
        }

        [[nodiscard]] bool StillWanted()
        {
            return sinceReport_.hasExpired(kHeartbeat.count()) ? Report() : true;
        }

        [[nodiscard]] bool Report()
        {
            sinceReport_.restart();

            return !onProgress_ || onProgress_(progress_);
        }

        QProcess& process_;
        const std::function<bool(const CopyProgress&)>& onProgress_;
        ProcessActivity activity_;
        std::unordered_map<std::string, std::uintmax_t> sizes_{};
        CopyProgress progress_{};
        QByteArray printed_{};
        QByteArray pending_{};
        QElapsedTimer sinceReport_{};
    };
}

std::filesystem::path SystemTarTool()
{
    const QString root = qEnvironmentVariable("SystemRoot", QStringLiteral("C:\\Windows"));

    return AsPath(root) / L"System32" / L"tar.exe";
}

bool AsksForAPassword(const QString& message, const QStringList& names)
{
    if (!MentionsAPassword(message))
    {
        return false;
    }

    return MentionsAPassword(
        WithoutKnownNames(FirstLinesMentioningAPassword(message), NamesMentioningAPassword(names)));
}

ArchiveListing ListingFrom(const QString& names, const QString& details)
{
    const std::vector<QStringView> named = PrintedLines(names);
    const std::vector<QStringView> described = PrintedLines(details);
    if (named.size() != described.size())
    {
        return {.failure = ArchiveFailure::CouldNotOpen, .entries = {}};
    }

    ArchiveListing listing;
    for (std::size_t at = 0; at < named.size(); ++at)
    {
        const std::optional<Details> entry = DetailsOf(described[at]);
        if (!entry.has_value())
        {
            return {.failure = ArchiveFailure::CouldNotOpen, .entries = {}};
        }

        const std::string path = NormalisedPath(named[at].toString());
        if (!path.empty())
        {
            listing.entries.push_back(ArchiveEntry{.path = path, .bytes = entry->bytes, .kind = KindOf(entry->mode)});
        }
    }

    return listing;
}

TarExeArchiveReader::TarExeArchiveReader(std::filesystem::path tool,
                                         const std::chrono::milliseconds timeLimit,
                                         const std::chrono::milliseconds idleLimit)
    : tool_(std::move(tool)), timeLimit_(timeLimit), idleLimit_(idleLimit)
{
}

ArchiveListing TarExeArchiveReader::EntriesOf(const std::filesystem::path& archive) const
{
    const Printout names = PrintedByListing(tool_, QStringLiteral("-tf"), archive, timeLimit_);
    if (names.failure != ArchiveFailure::None)
    {
        return {.failure = names.failure, .entries = {}};
    }

    const Printout details = PrintedByListing(tool_, QStringLiteral("-tvf"), archive, timeLimit_);
    if (details.failure != ArchiveFailure::None)
    {
        return {.failure = details.failure, .entries = {}};
    }

    return ListingFrom(names.text, details.text);
}

ArchiveText TarExeArchiveReader::ContentsOf(const std::filesystem::path& archive,
                                            const std::string& entry,
                                            const std::size_t most) const
{
    const qsizetype keep = most > static_cast<std::size_t>(kNoCap) ? kNoCap : static_cast<qsizetype>(most);
    const Run run = RunWithin(tool_, {QStringLiteral("-xOf"), AsText(archive), QStringLiteral("--"), Escaped(entry)},
                              timeLimit_, keep);

    const ArchiveFailure unfinished = FailureBeforeTheExit(run, tool_, ArchiveFailure::CouldNotOpen);
    if (unfinished != ArchiveFailure::None)
    {
        return {.failure = unfinished, .text = {}};
    }
    if (!run.truncated && run.exitCode != 0)
    {
        const QString message = WithoutTheEntry(QString::fromLocal8Bit(run.err), entry);

        return {.failure = PasswordOr(message, {AsText(archive), TextOf(entry)}, ArchiveFailure::CouldNotOpen),
                .text = {}};
    }

    return {.failure = ArchiveFailure::None, .text = run.out.left(keep).toStdString()};
}

ArchiveFailure TarExeArchiveReader::ExtractAll(const std::filesystem::path& archive,
                                               const std::vector<ArchiveEntry>& listed,
                                               const std::filesystem::path& into,
                                               const std::function<bool(const CopyProgress&)>& onProgress) const
{
    if (!IsAnEmptyStagingFolder(into))
    {
        return ArchiveFailure::CouldNotExtract;
    }

    const std::filesystem::path whole = std::filesystem::absolute(archive);
    const std::filesystem::path landing = std::filesystem::absolute(into);

    QProcess process;
    process.setStandardOutputFile(QProcess::nullDevice());
    process.start(AsText(tool_), {QStringLiteral("-xvkf"), AsText(whole), QStringLiteral("-C"), AsText(landing)});
    if (!process.waitForStarted(kStartTimeMs))
    {
        return FailureToStart(tool_, ArchiveFailure::CouldNotExtract);
    }
    process.closeWriteChannel();

    ExtractionWatch watch(process, listed, onProgress, idleLimit_);
    const Ending ending = watch.Follow();
    if (ending != Ending::Finished)
    {
        Reap(process);

        return ending == Ending::Stalled ? ArchiveFailure::TookTooLong : ArchiveFailure::Cancelled;
    }

    if (ExitCodeOf(process) == 0)
    {
        return ArchiveFailure::None;
    }

    const QString message = WithoutMemberNames(QString::fromLocal8Bit(watch.Printed()));

    return PasswordOr(message, NamesInTheMessages(whole, landing, listed), ArchiveFailure::CouldNotExtract);
}

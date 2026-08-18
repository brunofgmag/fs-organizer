#ifndef FS_ORGANIZER_TESTS_DOUBLES_FAKE_MANUAL_SOURCE_H
#define FS_ORGANIZER_TESTS_DOUBLES_FAKE_MANUAL_SOURCE_H

#include <algorithm>
#include <filesystem>
#include <set>
#include <string>
#include <vector>

#include "application/ManualCopy.h"
#include "application/ports/ManualSource.h"
#include "domain/support/PathUtils.h"

class FakeManualSource final : public ManualSource
{
public:
    [[nodiscard]] std::filesystem::path WhereTheManualWouldBe(const std::string& language) const override
    {
        return ManualFileIn(PathFromUtf8("C:/AppData/fs-organizer/manual"), version, language);
    }

    [[nodiscard]] bool TheManualIsHere(const std::string& language) const override
    {
        return here.contains(ManualLanguageFor(language));
    }

    void FetchTheManual(const std::string& language) override
    {
        asked.push_back(ManualLanguageFor(language));
    }

    void AddObserver(ManualSourceObserver* observer) override
    {
        observers.push_back(observer);
    }

    void RemoveObserver(ManualSourceObserver* observer) override
    {
        std::erase(observers, observer);
    }

    void AnswerWithTheManual()
    {
        here.insert(asked.back());

        Say(true, WhereTheManualWouldBe(asked.back()), {});
    }

    void AnswerWithAFailure(const std::string& error)
    {
        Say(false, {}, error);
    }

    std::string version = "0.52.0";
    std::set<std::string> here{};
    std::vector<std::string> asked{};
    std::vector<ManualSourceObserver*> observers{};

private:
    void Say(const bool ok, const std::filesystem::path& file, const std::string& error) const
    {
        for (ManualSourceObserver* observer : observers)
        {
            observer->OnManualFetched(ok, file, error);
        }
    }
};

#endif // FS_ORGANIZER_TESTS_DOUBLES_FAKE_MANUAL_SOURCE_H

#ifndef FS_ORGANIZER_TESTS_DOUBLES_FAKE_BISECTION_STORE_H
#define FS_ORGANIZER_TESTS_DOUBLES_FAKE_BISECTION_STORE_H

#include <map>
#include <optional>
#include <string>

#include "application/ports/BisectionStore.h"

class FakeBisectionStore final : public BisectionStore
{
public:
    [[nodiscard]] std::optional<BisectionRun> Load(const std::string& profileId) const override
    {
        const auto run = byProfile_.find(profileId);

        return run == byProfile_.end() ? std::nullopt : std::optional(run->second);
    }

    [[nodiscard]] bool Save(const std::string& profileId, const BisectionRun& run) override
    {
        if (refusing_)
        {
            return false;
        }

        byProfile_[profileId] = run;
        ++saves;

        return true;
    }

    void Forget(const std::string& profileId) override
    {
        byProfile_.erase(profileId);
        ++forgets;
    }

    void RefuseEveryWrite()
    {
        refusing_ = true;
    }

    std::size_t saves = 0;
    std::size_t forgets = 0;

private:
    std::map<std::string, BisectionRun> byProfile_;
    bool refusing_ = false;
};

#endif // FS_ORGANIZER_TESTS_DOUBLES_FAKE_BISECTION_STORE_H

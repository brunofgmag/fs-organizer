#ifndef FS_ORGANIZER_APPLICATION_PORTS_MANUAL_SOURCE_H
#define FS_ORGANIZER_APPLICATION_PORTS_MANUAL_SOURCE_H

#include <filesystem>
#include <string>

class ManualSourceObserver
{
public:
    virtual ~ManualSourceObserver() = default;

    virtual void OnManualFetched(bool ok, const std::filesystem::path& file, const std::string& error) = 0;
};

class ManualSource
{
public:
    virtual ~ManualSource() = default;

    [[nodiscard]] virtual std::filesystem::path WhereTheManualWouldBe(const std::string& language) const = 0;

    [[nodiscard]] virtual bool TheManualIsHere(const std::string& language) const = 0;

    virtual void FetchTheManual(const std::string& language) = 0;

    virtual void AddObserver(ManualSourceObserver* observer) = 0;

    virtual void RemoveObserver(ManualSourceObserver* observer) = 0;
};

class NoManualToFetch final : public ManualSource
{
public:
    [[nodiscard]] std::filesystem::path WhereTheManualWouldBe(const std::string&) const override
    {
        return {};
    }

    [[nodiscard]] bool TheManualIsHere(const std::string&) const override
    {
        return false;
    }

    void FetchTheManual(const std::string&) override
    {
    }

    void AddObserver(ManualSourceObserver*) override
    {
    }

    void RemoveObserver(ManualSourceObserver*) override
    {
    }
};

#endif // FS_ORGANIZER_APPLICATION_PORTS_MANUAL_SOURCE_H

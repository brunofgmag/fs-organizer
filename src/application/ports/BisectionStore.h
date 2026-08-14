#ifndef FS_ORGANIZER_APPLICATION_PORTS_BISECTION_STORE_H
#define FS_ORGANIZER_APPLICATION_PORTS_BISECTION_STORE_H

#include <optional>
#include <string>

#include "domain/bisection/BisectionRounds.h"

class BisectionStore
{
public:
    virtual ~BisectionStore() = default;

    [[nodiscard]] virtual std::optional<BisectionRun> Load(const std::string& profileId) const = 0;

    [[nodiscard]] virtual bool Save(const std::string& profileId, const BisectionRun& run) = 0;

    virtual void Forget(const std::string& profileId) = 0;
};

#endif // FS_ORGANIZER_APPLICATION_PORTS_BISECTION_STORE_H

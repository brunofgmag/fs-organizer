#ifndef FS_ORGANIZER_INFRASTRUCTURE_SIM_CHILD_NAMED_IGNORING_CASE_H
#define FS_ORGANIZER_INFRASTRUCTURE_SIM_CHILD_NAMED_IGNORING_CASE_H

#include <filesystem>
#include <optional>
#include <string>

[[nodiscard]] std::optional<std::filesystem::path> ChildNamedIgnoringCase(const std::filesystem::path& folder,
                                                                          const std::string& name);

#endif // FS_ORGANIZER_INFRASTRUCTURE_SIM_CHILD_NAMED_IGNORING_CASE_H

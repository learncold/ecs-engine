#pragma once

#include <cstddef>
#include <memory>
#include <stdexcept>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include "Engine/ECS/registry.h"

namespace engine::measurement {
class TimingRecorder;
}

namespace engine::ecs {

class System {
 public:
  virtual ~System() = default;

  // Convenience entry point for uninstrumented calls to an individual system.
  void Update(Registry& registry, float delta_seconds);
  virtual void Update(Registry& registry, float delta_seconds,
                      measurement::TimingRecorder& timing) = 0;

  // Report only separately owned storage. The manager counts the system object.
  virtual void AppendStorageReport(measurement::StorageReport&) const {}
};

class SystemManager {
 public:
  template <typename SystemType, typename... Args>
  SystemType& Add(Args&&... args) {
    return AddImpl<SystemType>({}, std::forward<Args>(args)...);
  }

  // Consecutive systems with the same section share one start/stop interval.
  // Labels are owned; register systems before Update and never mutate the list
  // during iteration. No label allocation occurs in the Update path.
  template <typename SystemType, typename... Args>
  SystemType& AddTimed(std::string_view section, Args&&... args) {
    if (section.empty()) {
      throw std::invalid_argument("Timed system section must not be empty");
    }
    return AddImpl<SystemType>(section, std::forward<Args>(args)...);
  }

  void Update(Registry& registry, float delta_seconds);
  void Update(Registry& registry, float delta_seconds,
              measurement::TimingRecorder& timing);

  void Clear() { systems_.clear(); }

  [[nodiscard]] std::size_t Size() const { return systems_.size(); }

  void AppendStorageReport(measurement::StorageReport& report,
                           std::string_view prefix = "systems") const;

 private:
  struct Entry {
    std::unique_ptr<System> system;
    std::size_t object_bytes;
    // A char vector exposes exact buffer size/capacity without string SSO
    // assumptions. Its control fields are already included in sizeof(Entry).
    std::vector<char> timing_section;

    [[nodiscard]] std::string_view Section() const {
      return timing_section.empty() ? std::string_view{}
                                    : std::string_view(timing_section.data(),
                                                       timing_section.size());
    }
  };

  template <typename SystemType, typename... Args>
  SystemType& AddImpl(std::string_view section, Args&&... args) {
    static_assert(std::is_base_of_v<System, SystemType>,
                  "SystemType must derive from engine::ecs::System");
    auto system = std::make_unique<SystemType>(std::forward<Args>(args)...);
    SystemType& reference = *system;
    std::vector<char> label;
    if (!section.empty()) label.assign(section.begin(), section.end());
    systems_.push_back(
        Entry{std::move(system), sizeof(SystemType), std::move(label)});
    return reference;
  }

  std::vector<Entry> systems_;
};

}  // namespace engine::ecs

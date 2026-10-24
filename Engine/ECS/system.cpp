#include "Engine/ECS/system.h"

#include <string>

#include "Engine/Measurement/timing_recorder.h"

namespace engine::ecs {

void System::Update(Registry& registry, float delta_seconds) {
  measurement::TimingRecorder disabled;
  disabled.set_enabled(false);
  Update(registry, delta_seconds, disabled);
}

void SystemManager::Update(Registry& registry, float delta_seconds) {
  measurement::TimingRecorder disabled;
  disabled.set_enabled(false);
  Update(registry, delta_seconds, disabled);
}

void SystemManager::Update(Registry& registry, float delta_seconds,
                           measurement::TimingRecorder& timing) {
  std::string_view current_section;
  for (const auto& entry : systems_) {
    const auto next_section = entry.Section();
    if (next_section != current_section) {
      if (!current_section.empty()) timing.stop(current_section);
      current_section = next_section;
      if (!current_section.empty()) timing.start(current_section);
    }
    entry.system->Update(registry, delta_seconds, timing);
  }
  if (!current_section.empty()) timing.stop(current_section);
}

void SystemManager::AppendStorageReport(measurement::StorageReport& report,
                                        std::string_view prefix) const {
  report.add_vector(std::string(prefix) + ".entries", systems_);
  for (std::size_t index = 0; index < systems_.size(); ++index) {
    const auto& entry = systems_[index];
    const std::string name = std::string(prefix) + "." + std::to_string(index);
    report.add_bytes(name + ".object", entry.object_bytes, entry.object_bytes);
    report.add_vector(name + ".timing_section", entry.timing_section);
    entry.system->AppendStorageReport(report);
  }
}

}  // namespace engine::ecs

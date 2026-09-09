#pragma once

#include <cstddef>

namespace app::ui {

class PerformancePanel {
 public:
  bool Draw(std::size_t boid_count, bool& vsync_enabled) const;
};

}  // namespace app::ui

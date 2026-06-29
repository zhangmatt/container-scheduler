#pragma once

#include "common/model.h"

#include <chrono>

namespace mini_borg {

class HealthChecker {
 public:
  [[nodiscard]] static bool IsHeartbeatFresh(
      const NodeInfo& node, TimePoint now,
      std::chrono::seconds heartbeat_timeout);
};

}  // namespace mini_borg

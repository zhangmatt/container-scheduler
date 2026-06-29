#include "agent/health.h"

namespace mini_borg {

bool HealthChecker::IsHeartbeatFresh(
    const NodeInfo& node, TimePoint now,
    std::chrono::seconds heartbeat_timeout) {
  return now - node.last_heartbeat <= heartbeat_timeout;
}

}  // namespace mini_borg

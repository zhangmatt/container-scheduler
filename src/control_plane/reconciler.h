#pragma once

#include "control_plane/scheduler.h"
#include "control_plane/state_store.h"

#include <chrono>
#include <string>
#include <vector>

namespace mini_borg {

struct ReconcileReport {
  std::vector<std::string> unhealthy_nodes;
  std::vector<std::string> requeued_tasks;
  std::vector<ScheduleDecision> schedule_decisions;
};

class Reconciler {
 public:
  Reconciler(StateStore& state, Scheduler& scheduler,
             std::chrono::seconds heartbeat_timeout);

  [[nodiscard]] ReconcileReport Reconcile(TimePoint now);

 private:
  StateStore& state_;
  Scheduler& scheduler_;
  std::chrono::seconds heartbeat_timeout_;
};

}  // namespace mini_borg

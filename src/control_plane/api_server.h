#pragma once

#include "control_plane/scheduler.h"
#include "control_plane/state_store.h"

#include <string>
#include <vector>

namespace mini_borg {

struct SubmitResult {
  std::vector<std::string> task_ids;
  std::vector<ScheduleDecision> schedule_decisions;
};

class ApiServer {
 public:
  ApiServer(StateStore& state, Scheduler& scheduler);

  [[nodiscard]] SubmitResult SubmitTask(const TaskSpec& spec);
  [[nodiscard]] std::vector<NodeInfo> ListNodes() const;
  [[nodiscard]] std::vector<TaskInfo> ListTasks() const;
  [[nodiscard]] bool KillTask(const std::string& task_id);

 private:
  StateStore& state_;
  Scheduler& scheduler_;
};

}  // namespace mini_borg

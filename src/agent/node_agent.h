#pragma once

#include "agent/runtime.h"
#include "control_plane/state_store.h"

#include <string>
#include <vector>

namespace mini_borg {

struct AgentSyncReport {
  std::vector<std::string> launched_tasks;
  std::vector<std::string> failed_tasks;
};

class NodeAgent {
 public:
  NodeAgent(StateStore& state, Runtime& runtime, std::string node_id,
            ResourceQuantity capacity);

  void Register(TimePoint now);
  bool Heartbeat(TimePoint now);
  [[nodiscard]] AgentSyncReport SyncAssignedTasks();
  [[nodiscard]] bool StopTask(const std::string& task_id);

 private:
  StateStore& state_;
  Runtime& runtime_;
  std::string node_id_;
  ResourceQuantity capacity_;
};

}  // namespace mini_borg

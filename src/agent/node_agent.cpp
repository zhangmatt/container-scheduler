#include "agent/node_agent.h"

#include <utility>

namespace mini_borg {

NodeAgent::NodeAgent(StateStore& state, Runtime& runtime, std::string node_id,
                     ResourceQuantity capacity)
    : state_(state),
      runtime_(runtime),
      node_id_(std::move(node_id)),
      capacity_(capacity) {}

void NodeAgent::Register(TimePoint now) {
  NodeInfo node;
  node.id = node_id_;
  node.capacity = capacity_;
  node.health = NodeHealth::Healthy;
  node.last_heartbeat = now;
  state_.UpsertNode(node);
}

bool NodeAgent::Heartbeat(TimePoint now) {
  return state_.TouchNodeHeartbeat(node_id_, now);
}

AgentSyncReport NodeAgent::SyncAssignedTasks() {
  AgentSyncReport report;
  for (const auto& task : state_.TasksOnNode(node_id_)) {
    const auto status = runtime_.Status(task.id);
    if (status == RuntimeTaskState::Running) {
      continue;
    }

    auto result = runtime_.Launch(task);
    if (result.ok) {
      report.launched_tasks.push_back(task.id);
    } else {
      state_.UnbindTask(task.id, TaskState::Pending);
      report.failed_tasks.push_back(task.id);
    }
  }
  return report;
}

bool NodeAgent::StopTask(const std::string& task_id) {
  const auto stopped = runtime_.Stop(task_id);
  if (!stopped.ok) {
    return false;
  }
  return state_.UnbindTask(task_id, TaskState::Pending);
}

}  // namespace mini_borg

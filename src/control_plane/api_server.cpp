#include "control_plane/api_server.h"

#include <stdexcept>

namespace mini_borg {

ApiServer::ApiServer(StateStore& state, Scheduler& scheduler)
    : state_(state), scheduler_(scheduler) {}

SubmitResult ApiServer::SubmitTask(const TaskSpec& spec) {
  if (spec.name.empty()) {
    throw std::invalid_argument("task name is required");
  }
  if (spec.replicas <= 0) {
    throw std::invalid_argument("replicas must be positive");
  }
  if (!spec.resources.IsNonNegative() || spec.resources.IsZero()) {
    throw std::invalid_argument("resource request must be positive");
  }

  SubmitResult result;
  result.task_ids.reserve(static_cast<std::size_t>(spec.replicas));
  for (int replica = 0; replica < spec.replicas; ++replica) {
    const auto task_id = spec.name + "-" + std::to_string(replica);
    if (state_.GetTask(task_id).has_value()) {
      throw std::invalid_argument("task already exists: " + task_id);
    }
    TaskInfo task;
    task.id = task_id;
    task.name = spec.name;
    task.tenant = spec.tenant;
    task.command = spec.command;
    task.resources = spec.resources;
    task.priority = spec.priority;
    task.state = TaskState::Pending;
    task.desired = true;
    state_.UpsertTask(task);
    result.task_ids.push_back(task_id);
  }

  result.schedule_decisions = scheduler_.ScheduleAll();
  return result;
}

std::vector<NodeInfo> ApiServer::ListNodes() const { return state_.ListNodes(); }

std::vector<TaskInfo> ApiServer::ListTasks() const { return state_.ListTasks(); }

bool ApiServer::KillTask(const std::string& task_id) {
  auto task = state_.GetTask(task_id);
  if (!task.has_value()) {
    return false;
  }
  task->desired = false;
  task->state = TaskState::Killed;
  task->node_id.reset();
  state_.UpsertTask(*task);
  return true;
}

}  // namespace mini_borg

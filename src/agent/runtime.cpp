#include "agent/runtime.h"

namespace mini_borg {

RuntimeResult ProcessRuntime::Launch(const TaskInfo& task) {
  if (task.command.empty()) {
    return {false, "task command is empty"};
  }
  tasks_[task.id] = RuntimeTaskState::Running;
  return {true, "task launched"};
}

RuntimeResult ProcessRuntime::Stop(const std::string& task_id) {
  auto it = tasks_.find(task_id);
  if (it == tasks_.end()) {
    return {false, "task is not running"};
  }
  it->second = RuntimeTaskState::Exited;
  return {true, "task stopped"};
}

RuntimeTaskState ProcessRuntime::Status(const std::string& task_id) const {
  auto it = tasks_.find(task_id);
  if (it == tasks_.end()) {
    return RuntimeTaskState::NotFound;
  }
  return it->second;
}

RuntimeResult DockerRuntime::Launch(const TaskInfo&) {
  return {false,
          "DockerRuntime is an integration boundary in this build; use "
          "ProcessRuntime for deterministic tests"};
}

RuntimeResult DockerRuntime::Stop(const std::string&) {
  return {false,
          "DockerRuntime is an integration boundary in this build; use "
          "ProcessRuntime for deterministic tests"};
}

RuntimeTaskState DockerRuntime::Status(const std::string&) const {
  return RuntimeTaskState::NotFound;
}

std::string ToString(RuntimeTaskState state) {
  switch (state) {
    case RuntimeTaskState::NotFound:
      return "not_found";
    case RuntimeTaskState::Running:
      return "running";
    case RuntimeTaskState::Exited:
      return "exited";
  }
  return "unknown";
}

}  // namespace mini_borg

#include "common/model.h"

namespace mini_borg {

std::string ToString(NodeHealth health) {
  switch (health) {
    case NodeHealth::Healthy:
      return "healthy";
    case NodeHealth::Unhealthy:
      return "unhealthy";
  }
  return "unknown";
}

std::string ToString(TaskState state) {
  switch (state) {
    case TaskState::Pending:
      return "pending";
    case TaskState::Running:
      return "running";
    case TaskState::Succeeded:
      return "succeeded";
    case TaskState::Failed:
      return "failed";
    case TaskState::Killed:
      return "killed";
  }
  return "unknown";
}

}  // namespace mini_borg

#pragma once

#include "common/resources.h"

#include <chrono>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace mini_borg {

using Clock = std::chrono::steady_clock;
using TimePoint = Clock::time_point;

enum class NodeHealth {
  Healthy,
  Unhealthy,
};

enum class TaskState {
  Pending,
  Running,
  Succeeded,
  Failed,
  Killed,
};

struct NodeInfo {
  std::string id;
  ResourceQuantity capacity;
  NodeHealth health{NodeHealth::Healthy};
  TimePoint last_heartbeat{};
  std::map<std::string, std::string> labels;
};

struct TaskSpec {
  std::string name;
  std::string tenant{"default"};
  std::vector<std::string> command;
  ResourceQuantity resources;
  int priority{0};
  int replicas{1};
};

struct TaskInfo {
  std::string id;
  std::string name;
  std::string tenant{"default"};
  std::vector<std::string> command;
  ResourceQuantity resources;
  int priority{0};
  TaskState state{TaskState::Pending};
  std::optional<std::string> node_id;
  bool desired{true};
  int preemptions{0};
};

[[nodiscard]] std::string ToString(NodeHealth health);
[[nodiscard]] std::string ToString(TaskState state);

}  // namespace mini_borg

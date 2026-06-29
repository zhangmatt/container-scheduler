#pragma once

#include "control_plane/state_store.h"

#include <optional>
#include <set>
#include <string>
#include <vector>

namespace mini_borg {

enum class ScheduleStatus {
  Placed,
  PlacedWithPreemption,
  AlreadyRunning,
  NotFound,
  InvalidTask,
  QuotaExceeded,
  NoHealthyNodes,
  NoFeasibleNode,
};

struct ScheduleDecision {
  std::string task_id;
  ScheduleStatus status{ScheduleStatus::NoFeasibleNode};
  std::optional<std::string> node_id;
  std::vector<std::string> preempted_task_ids;
  std::string reason;
};

[[nodiscard]] std::string ToString(ScheduleStatus status);

class Scheduler {
 public:
  explicit Scheduler(StateStore& state);

  [[nodiscard]] ScheduleDecision ScheduleOne(const std::string& task_id);
  [[nodiscard]] std::vector<ScheduleDecision> ScheduleAll();

  struct NodeChoice {
    std::string node_id;
    double utilization_after{0.0};
    ResourceQuantity remaining_after;
  };

  struct PreemptionPlan {
    std::string node_id;
    std::vector<std::string> victim_ids;
    int max_victim_priority{0};
    int priority_sum{0};
    double utilization_after{0.0};
  };

 private:
  [[nodiscard]] std::optional<std::vector<std::string>>
  PlanQuotaPreemptions(const TaskInfo& task) const;
  [[nodiscard]] std::optional<NodeChoice> FindBestFitNode(
      const TaskInfo& task, const std::set<std::string>& assumed_preempted) const;
  [[nodiscard]] std::optional<PreemptionPlan> FindPreemptionPlan(
      const TaskInfo& task, const std::set<std::string>& assumed_preempted) const;
  [[nodiscard]] ResourceQuantity ProjectedAllocatedOnNode(
      const std::string& node_id,
      const std::set<std::string>& assumed_preempted) const;
  [[nodiscard]] bool HasHealthyNode() const;
  void ApplyPreemptions(const std::vector<std::string>& victim_ids);

  StateStore& state_;
};

}  // namespace mini_borg

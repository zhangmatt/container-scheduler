#include "control_plane/scheduler.h"

#include <algorithm>
#include <limits>
#include <numeric>
#include <tuple>

namespace mini_borg {
namespace {

bool BetterNodeChoice(const Scheduler::NodeChoice& candidate,
                      const Scheduler::NodeChoice& incumbent) {
  constexpr double kEpsilon = 1e-9;
  if (candidate.utilization_after > incumbent.utilization_after + kEpsilon) {
    return true;
  }
  if (incumbent.utilization_after > candidate.utilization_after + kEpsilon) {
    return false;
  }

  const auto candidate_waste = candidate.remaining_after.cpu_millis +
                               candidate.remaining_after.memory_mb;
  const auto incumbent_waste = incumbent.remaining_after.cpu_millis +
                               incumbent.remaining_after.memory_mb;
  if (candidate_waste != incumbent_waste) {
    return candidate_waste < incumbent_waste;
  }
  return candidate.node_id < incumbent.node_id;
}

bool BetterPreemptionPlan(const Scheduler::PreemptionPlan& candidate,
                          const Scheduler::PreemptionPlan& incumbent) {
  const auto candidate_key =
      std::tuple{candidate.max_victim_priority, candidate.priority_sum,
                 candidate.victim_ids.size(), -candidate.utilization_after,
                 candidate.node_id};
  const auto incumbent_key =
      std::tuple{incumbent.max_victim_priority, incumbent.priority_sum,
                 incumbent.victim_ids.size(), -incumbent.utilization_after,
                 incumbent.node_id};
  return candidate_key < incumbent_key;
}

void AppendUnique(std::vector<std::string>& target,
                  const std::vector<std::string>& additions) {
  for (const auto& id : additions) {
    if (std::find(target.begin(), target.end(), id) == target.end()) {
      target.push_back(id);
    }
  }
}

}  // namespace

std::string ToString(ScheduleStatus status) {
  switch (status) {
    case ScheduleStatus::Placed:
      return "placed";
    case ScheduleStatus::PlacedWithPreemption:
      return "placed_with_preemption";
    case ScheduleStatus::AlreadyRunning:
      return "already_running";
    case ScheduleStatus::NotFound:
      return "not_found";
    case ScheduleStatus::InvalidTask:
      return "invalid_task";
    case ScheduleStatus::QuotaExceeded:
      return "quota_exceeded";
    case ScheduleStatus::NoHealthyNodes:
      return "no_healthy_nodes";
    case ScheduleStatus::NoFeasibleNode:
      return "no_feasible_node";
  }
  return "unknown";
}

Scheduler::Scheduler(StateStore& state) : state_(state) {}

ScheduleDecision Scheduler::ScheduleOne(const std::string& task_id) {
  ScheduleDecision decision;
  decision.task_id = task_id;

  auto task = state_.GetTask(task_id);
  if (!task.has_value()) {
    decision.status = ScheduleStatus::NotFound;
    decision.reason = "task does not exist";
    return decision;
  }

  if (task->state == TaskState::Running) {
    decision.status = ScheduleStatus::AlreadyRunning;
    decision.node_id = task->node_id;
    decision.reason = "task is already bound";
    return decision;
  }

  if (!task->desired || task->state == TaskState::Killed ||
      task->state == TaskState::Succeeded) {
    decision.status = ScheduleStatus::InvalidTask;
    decision.reason = "task is not schedulable";
    return decision;
  }

  if (!task->resources.IsNonNegative() || task->resources.IsZero()) {
    decision.status = ScheduleStatus::InvalidTask;
    decision.reason = "task resource request must be positive";
    return decision;
  }

  auto quota_victims = PlanQuotaPreemptions(*task);
  if (!quota_victims.has_value()) {
    decision.status = ScheduleStatus::QuotaExceeded;
    decision.reason = "tenant quota cannot fit task";
    return decision;
  }

  std::set<std::string> assumed_preempted(quota_victims->begin(),
                                          quota_victims->end());

  if (auto node = FindBestFitNode(*task, assumed_preempted)) {
    decision.node_id = node->node_id;
    decision.preempted_task_ids = *quota_victims;
    ApplyPreemptions(decision.preempted_task_ids);
    state_.BindTask(task_id, node->node_id);
    decision.status = decision.preempted_task_ids.empty()
                          ? ScheduleStatus::Placed
                          : ScheduleStatus::PlacedWithPreemption;
    decision.reason = "best-fit node selected";
    return decision;
  }

  if (auto plan = FindPreemptionPlan(*task, assumed_preempted)) {
    decision.node_id = plan->node_id;
    decision.preempted_task_ids = *quota_victims;
    AppendUnique(decision.preempted_task_ids, plan->victim_ids);
    ApplyPreemptions(decision.preempted_task_ids);
    state_.BindTask(task_id, plan->node_id);
    decision.status = ScheduleStatus::PlacedWithPreemption;
    decision.reason = "lower-priority tasks preempted";
    return decision;
  }

  decision.status =
      HasHealthyNode() ? ScheduleStatus::NoFeasibleNode : ScheduleStatus::NoHealthyNodes;
  decision.reason = "no node can satisfy the request";
  return decision;
}

std::vector<ScheduleDecision> Scheduler::ScheduleAll() {
  std::vector<ScheduleDecision> decisions;
  const auto initial_task_count = state_.ListTasks().size();
  const auto max_rounds = initial_task_count + 1;

  for (std::size_t round = 0; round < max_rounds; ++round) {
    auto tasks = state_.ListTasks();
    std::vector<TaskInfo> pending;
    for (const auto& task : tasks) {
      if (task.desired && task.state == TaskState::Pending) {
        pending.push_back(task);
      }
    }

    if (pending.empty()) {
      break;
    }

    std::sort(pending.begin(), pending.end(),
              [](const TaskInfo& lhs, const TaskInfo& rhs) {
                if (lhs.priority != rhs.priority) {
                  return lhs.priority > rhs.priority;
                }
                return lhs.id < rhs.id;
              });

    bool placed_any = false;
    for (const auto& task : pending) {
      auto decision = ScheduleOne(task.id);
      if (decision.status == ScheduleStatus::Placed ||
          decision.status == ScheduleStatus::PlacedWithPreemption) {
        placed_any = true;
      }
      decisions.push_back(std::move(decision));
    }

    if (!placed_any) {
      break;
    }
  }

  return decisions;
}

std::optional<std::vector<std::string>> Scheduler::PlanQuotaPreemptions(
    const TaskInfo& task) const {
  const auto quota = state_.TenantQuota(task.tenant);
  if (!quota.has_value()) {
    return std::vector<std::string>{};
  }

  const auto allocated = state_.AllocatedToTenant(task.tenant);
  if ((allocated + task.resources).FitsIn(*quota)) {
    return std::vector<std::string>{};
  }

  auto tasks = state_.ListTasks();
  std::vector<TaskInfo> candidates;
  for (const auto& candidate : tasks) {
    if (candidate.tenant == task.tenant &&
        candidate.state == TaskState::Running &&
        candidate.priority < task.priority) {
      candidates.push_back(candidate);
    }
  }

  std::sort(candidates.begin(), candidates.end(),
            [](const TaskInfo& lhs, const TaskInfo& rhs) {
              if (lhs.priority != rhs.priority) {
                return lhs.priority < rhs.priority;
              }
              return lhs.id < rhs.id;
            });

  ResourceQuantity freed;
  std::vector<std::string> victims;
  for (const auto& candidate : candidates) {
    victims.push_back(candidate.id);
    freed = freed + candidate.resources;
    if ((allocated - freed + task.resources).FitsIn(*quota)) {
      return victims;
    }
  }

  return std::nullopt;
}

std::optional<Scheduler::NodeChoice> Scheduler::FindBestFitNode(
    const TaskInfo& task, const std::set<std::string>& assumed_preempted) const {
  std::optional<NodeChoice> best;
  for (const auto& node : state_.ListNodes()) {
    if (node.health != NodeHealth::Healthy) {
      continue;
    }

    const auto allocated = ProjectedAllocatedOnNode(node.id, assumed_preempted);
    const auto free = node.capacity - allocated;
    if (!task.resources.FitsIn(free)) {
      continue;
    }

    const auto used_after = allocated + task.resources;
    const auto remaining_after = node.capacity - used_after;
    NodeChoice candidate{node.id, DominantShare(used_after, node.capacity),
                         remaining_after};
    if (!best.has_value() || BetterNodeChoice(candidate, *best)) {
      best = candidate;
    }
  }
  return best;
}

std::optional<Scheduler::PreemptionPlan> Scheduler::FindPreemptionPlan(
    const TaskInfo& task, const std::set<std::string>& assumed_preempted) const {
  std::optional<PreemptionPlan> best;
  for (const auto& node : state_.ListNodes()) {
    if (node.health != NodeHealth::Healthy) {
      continue;
    }

    auto allocated = ProjectedAllocatedOnNode(node.id, assumed_preempted);
    auto free = node.capacity - allocated;
    if (task.resources.FitsIn(free)) {
      continue;
    }

    auto running = state_.TasksOnNode(node.id);
    std::vector<TaskInfo> candidates;
    for (const auto& candidate : running) {
      if (assumed_preempted.contains(candidate.id)) {
        continue;
      }
      if (candidate.priority < task.priority) {
        candidates.push_back(candidate);
      }
    }

    std::sort(candidates.begin(), candidates.end(),
              [](const TaskInfo& lhs, const TaskInfo& rhs) {
                if (lhs.priority != rhs.priority) {
                  return lhs.priority < rhs.priority;
                }
                return lhs.id < rhs.id;
              });

    std::vector<std::string> victims;
    int max_priority = std::numeric_limits<int>::min();
    int priority_sum = 0;
    for (const auto& victim : candidates) {
      victims.push_back(victim.id);
      allocated = allocated - victim.resources;
      free = node.capacity - allocated;
      max_priority = std::max(max_priority, victim.priority);
      priority_sum += victim.priority;
      if (task.resources.FitsIn(free)) {
        const auto used_after = allocated + task.resources;
        PreemptionPlan candidate{node.id, victims, max_priority, priority_sum,
                                 DominantShare(used_after, node.capacity)};
        if (!best.has_value() || BetterPreemptionPlan(candidate, *best)) {
          best = candidate;
        }
        break;
      }
    }
  }
  return best;
}

ResourceQuantity Scheduler::ProjectedAllocatedOnNode(
    const std::string& node_id,
    const std::set<std::string>& assumed_preempted) const {
  ResourceQuantity allocated;
  for (const auto& task : state_.TasksOnNode(node_id)) {
    if (!assumed_preempted.contains(task.id)) {
      allocated = allocated + task.resources;
    }
  }
  return allocated;
}

bool Scheduler::HasHealthyNode() const {
  for (const auto& node : state_.ListNodes()) {
    if (node.health == NodeHealth::Healthy) {
      return true;
    }
  }
  return false;
}

void Scheduler::ApplyPreemptions(const std::vector<std::string>& victim_ids) {
  for (const auto& id : victim_ids) {
    state_.PreemptTask(id);
  }
}

}  // namespace mini_borg

#include "control_plane/reconciler.h"

#include <algorithm>

namespace mini_borg {

Reconciler::Reconciler(StateStore& state, Scheduler& scheduler,
                       std::chrono::seconds heartbeat_timeout)
    : state_(state),
      scheduler_(scheduler),
      heartbeat_timeout_(heartbeat_timeout) {}

ReconcileReport Reconciler::Reconcile(TimePoint now) {
  ReconcileReport report;

  for (const auto& node : state_.ListNodes()) {
    if (node.health == NodeHealth::Healthy &&
        now - node.last_heartbeat > heartbeat_timeout_) {
      state_.SetNodeHealth(node.id, NodeHealth::Unhealthy);
      report.unhealthy_nodes.push_back(node.id);

      for (const auto& task : state_.TasksOnNode(node.id)) {
        if (task.desired) {
          state_.UnbindTask(task.id, TaskState::Pending);
          report.requeued_tasks.push_back(task.id);
        }
      }
    }
  }

  for (const auto& task : state_.ListTasks()) {
    if (task.desired && task.state == TaskState::Failed) {
      state_.UnbindTask(task.id, TaskState::Pending);
      report.requeued_tasks.push_back(task.id);
    }
  }

  std::sort(report.unhealthy_nodes.begin(), report.unhealthy_nodes.end());
  std::sort(report.requeued_tasks.begin(), report.requeued_tasks.end());
  report.schedule_decisions = scheduler_.ScheduleAll();
  return report;
}

}  // namespace mini_borg

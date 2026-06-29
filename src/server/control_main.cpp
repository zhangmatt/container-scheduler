#include "agent/node_agent.h"
#include "agent/runtime.h"
#include "control_plane/api_server.h"
#include "control_plane/reconciler.h"
#include "control_plane/scheduler.h"
#include "control_plane/state_store.h"

#include <chrono>
#include <iostream>

int main(int argc, char** argv) {
  using namespace mini_borg;

  InMemoryStateStore state;
  Scheduler scheduler(state);
  ApiServer api(state, scheduler);
  Reconciler reconciler(state, scheduler, std::chrono::seconds(15));

  ProcessRuntime runtime_a;
  ProcessRuntime runtime_b;
  ProcessRuntime runtime_c;
  const auto now = Clock::now();
  NodeAgent agent_a(state, runtime_a, "node-a", {4000, 8192});
  NodeAgent agent_b(state, runtime_b, "node-b", {4000, 8192});
  NodeAgent agent_c(state, runtime_c, "node-c", {4000, 8192});
  agent_a.Register(now);
  agent_b.Register(now);
  agent_c.Register(now);

  const bool demo =
      argc > 1 && std::string(argv[1]) == std::string("--demo");
  if (demo) {
    TaskSpec spec;
    spec.name = "batch";
    spec.tenant = "demo";
    spec.command = {"sleep", "60"};
    spec.resources = {1000, 512};
    spec.priority = 10;
    spec.replicas = 6;
    [[maybe_unused]] const auto submit_result = api.SubmitTask(spec);
    [[maybe_unused]] const auto sync_a = agent_a.SyncAssignedTasks();
    [[maybe_unused]] const auto sync_b = agent_b.SyncAssignedTasks();
    [[maybe_unused]] const auto sync_c = agent_c.SyncAssignedTasks();
    [[maybe_unused]] const auto reconcile_report =
        reconciler.Reconcile(now + std::chrono::seconds(1));
  }

  std::cout << "nodes\n";
  for (const auto& node : api.ListNodes()) {
    std::cout << "  " << node.id << " " << ToString(node.health)
              << " capacity=" << node.capacity.ToString()
              << " allocated=" << state.AllocatedOnNode(node.id).ToString()
              << "\n";
  }

  std::cout << "tasks\n";
  for (const auto& task : api.ListTasks()) {
    std::cout << "  " << task.id << " " << ToString(task.state);
    if (task.node_id.has_value()) {
      std::cout << " node=" << *task.node_id;
    }
    std::cout << " priority=" << task.priority << "\n";
  }

  return 0;
}

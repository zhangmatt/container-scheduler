#include "agent/node_agent.h"
#include "agent/runtime.h"
#include "control_plane/api_server.h"
#include "control_plane/scheduler.h"
#include "control_plane/state_store.h"

#include <iostream>
#include <string>

int main(int argc, char** argv) {
  using namespace mini_borg;

  if (argc <= 1 || std::string(argv[1]) != "demo") {
    std::cout << "usage: borgctl demo\n";
    return 0;
  }

  InMemoryStateStore state;
  Scheduler scheduler(state);
  ApiServer api(state, scheduler);
  ProcessRuntime runtime;
  NodeAgent agent(state, runtime, "node-a", {4000, 8192});
  agent.Register(Clock::now());

  TaskSpec spec;
  spec.name = "hello";
  spec.tenant = "default";
  spec.command = {"echo", "hello"};
  spec.resources = {500, 128};
  spec.priority = 1;
  spec.replicas = 1;

  const auto result = api.SubmitTask(spec);
  [[maybe_unused]] const auto sync_report = agent.SyncAssignedTasks();

  for (const auto& id : result.task_ids) {
    const auto task = state.GetTask(id);
    if (task.has_value()) {
      std::cout << task->id << " " << ToString(task->state);
      if (task->node_id.has_value()) {
        std::cout << " node=" << *task->node_id;
      }
      std::cout << "\n";
    }
  }

  return 0;
}

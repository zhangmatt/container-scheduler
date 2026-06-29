#include "test_suite.h"

#include "control_plane/api_server.h"
#include "control_plane/reconciler.h"
#include "control_plane/scheduler.h"
#include "control_plane/state_store.h"

#include <chrono>
#include <string>

using namespace mini_borg;

namespace {

void RegisterNode(InMemoryStateStore& state, const std::string& id,
                  ResourceQuantity capacity, TimePoint now) {
  NodeInfo node;
  node.id = id;
  node.capacity = capacity;
  node.health = NodeHealth::Healthy;
  node.last_heartbeat = now;
  state.UpsertNode(node);
}

TaskSpec SimTask(int index) {
  TaskSpec spec;
  spec.name = "sim-" + std::to_string(index);
  spec.tenant = index % 2 == 0 ? "batch" : "serving";
  spec.command = {"sleep", "1"};
  spec.resources = {500 + (index % 4) * 250, 256 + (index % 5) * 128};
  spec.priority = index % 10;
  spec.replicas = 1;
  return spec;
}

void LargeSimulationPlacesAndRecoversFromNodeFailure() {
  InMemoryStateStore state;
  const auto t0 = TimePoint{};
  for (int i = 0; i < 25; ++i) {
    RegisterNode(state, "node-" + std::to_string(i), {8000, 32768}, t0);
  }

  Scheduler scheduler(state);
  ApiServer api(state, scheduler);
  Reconciler reconciler(state, scheduler, std::chrono::seconds(15));

  for (int i = 0; i < 200; ++i) {
    [[maybe_unused]] const auto result = api.SubmitTask(SimTask(i));
  }

  for (const auto& task : state.ListTasks()) {
    ASSERT_EQ(task.state, TaskState::Running);
    ASSERT_TRUE(task.node_id.has_value());
  }

  for (const auto& node : state.ListNodes()) {
    ASSERT_TRUE(state.AllocatedOnNode(node.id).FitsIn(node.capacity));
  }

  const auto failed_node_tasks = state.TasksOnNode("node-0");
  ASSERT_TRUE(!failed_node_tasks.empty());
  for (int i = 1; i < 25; ++i) {
    state.TouchNodeHeartbeat("node-" + std::to_string(i),
                             t0 + std::chrono::seconds(19));
  }

  const auto report = reconciler.Reconcile(t0 + std::chrono::seconds(20));
  ASSERT_EQ(report.unhealthy_nodes.size(), 1U);
  ASSERT_EQ(report.unhealthy_nodes[0], std::string("node-0"));

  for (const auto& old_task : failed_node_tasks) {
    const auto task = state.GetTask(old_task.id);
    ASSERT_TRUE(task.has_value());
    ASSERT_EQ(task->state, TaskState::Running);
    ASSERT_TRUE(task->node_id.has_value());
    ASSERT_TRUE(*task->node_id != "node-0");
  }

  for (const auto& node : state.ListNodes()) {
    ASSERT_TRUE(state.AllocatedOnNode(node.id).FitsIn(node.capacity));
  }
}

}  // namespace

std::vector<NamedTest> SimulationTests() {
  return {{"large simulation places and recovers from node failure",
           LargeSimulationPlacesAndRecoversFromNodeFailure}};
}

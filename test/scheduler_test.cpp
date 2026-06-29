#include "test_suite.h"

#include "agent/node_agent.h"
#include "agent/runtime.h"
#include "control_plane/api_server.h"
#include "control_plane/reconciler.h"
#include "control_plane/scheduler.h"
#include "control_plane/state_store.h"

#include <chrono>
#include <utility>

using namespace mini_borg;

namespace {

void RegisterNode(InMemoryStateStore& state, const std::string& id,
                  ResourceQuantity capacity, TimePoint now = TimePoint{}) {
  NodeInfo node;
  node.id = id;
  node.capacity = capacity;
  node.health = NodeHealth::Healthy;
  node.last_heartbeat = now;
  state.UpsertNode(node);
}

TaskSpec Spec(std::string name, ResourceQuantity resources, int priority,
              int replicas = 1, std::string tenant = "default") {
  TaskSpec spec;
  spec.name = std::move(name);
  spec.tenant = std::move(tenant);
  spec.command = {"sleep", "1"};
  spec.resources = resources;
  spec.priority = priority;
  spec.replicas = replicas;
  return spec;
}

void Submit(ApiServer& api, const TaskSpec& spec) {
  [[maybe_unused]] const auto result = api.SubmitTask(spec);
}

void NodeRegistrationTracksHealthyCapacity() {
  InMemoryStateStore state;
  ProcessRuntime runtime_a;
  ProcessRuntime runtime_b;
  ProcessRuntime runtime_c;
  const auto now = TimePoint{};

  NodeAgent agent_a(state, runtime_a, "node-a", {4000, 8192});
  NodeAgent agent_b(state, runtime_b, "node-b", {8000, 16384});
  NodeAgent agent_c(state, runtime_c, "node-c", {2000, 4096});
  agent_a.Register(now);
  agent_b.Register(now);
  agent_c.Register(now);

  const auto nodes = state.ListNodes();
  ASSERT_EQ(nodes.size(), 3U);
  ASSERT_EQ(nodes[0].id, std::string("node-a"));
  ASSERT_EQ(nodes[0].health, NodeHealth::Healthy);
  ASSERT_EQ(nodes[0].capacity, (ResourceQuantity{4000, 8192}));
  ASSERT_EQ(nodes[1].capacity, (ResourceQuantity{8000, 16384}));
  ASSERT_EQ(nodes[2].capacity, (ResourceQuantity{2000, 4096}));
}

void BestFitBinPacksWithoutOvercommit() {
  InMemoryStateStore state;
  RegisterNode(state, "node-a", {4000, 8192});
  RegisterNode(state, "node-b", {4000, 8192});
  RegisterNode(state, "node-c", {4000, 8192});

  Scheduler scheduler(state);
  ApiServer api(state, scheduler);
  Submit(api, Spec("worker", {1000, 512}, 10, 6));

  ASSERT_EQ(state.AllocatedOnNode("node-a"),
            (ResourceQuantity{4000, 2048}));
  ASSERT_EQ(state.AllocatedOnNode("node-b"),
            (ResourceQuantity{2000, 1024}));
  ASSERT_EQ(state.AllocatedOnNode("node-c"), (ResourceQuantity{0, 0}));

  for (const auto& node : state.ListNodes()) {
    ASSERT_TRUE(state.AllocatedOnNode(node.id).FitsIn(node.capacity));
  }
}

void HighPriorityTaskPreemptsLowerPriorityWork() {
  InMemoryStateStore state;
  RegisterNode(state, "node-a", {4000, 8192});

  Scheduler scheduler(state);
  ApiServer api(state, scheduler);
  Submit(api, Spec("low", {2000, 512}, 1, 2));
  Submit(api, Spec("high", {3000, 512}, 100, 1));

  const auto high = state.GetTask("high-0");
  const auto low0 = state.GetTask("low-0");
  const auto low1 = state.GetTask("low-1");
  ASSERT_TRUE(high.has_value());
  ASSERT_TRUE(low0.has_value());
  ASSERT_TRUE(low1.has_value());
  ASSERT_EQ(high->state, TaskState::Running);
  ASSERT_EQ(high->node_id, std::optional<std::string>{"node-a"});
  ASSERT_EQ(low0->state, TaskState::Pending);
  ASSERT_EQ(low1->state, TaskState::Pending);
  ASSERT_EQ(low0->preemptions, 1);
  ASSERT_EQ(low1->preemptions, 1);
}

void TenantQuotaBlocksAndCanPreemptWithinTenant() {
  InMemoryStateStore state;
  RegisterNode(state, "node-a", {4000, 8192});
  state.SetTenantQuota("tenant-a", {2000, 8192});

  Scheduler scheduler(state);
  ApiServer api(state, scheduler);
  Submit(api, Spec("quota-low", {2000, 512}, 1, 1, "tenant-a"));
  Submit(api, Spec("quota-denied", {1000, 512}, 1, 1, "tenant-a"));

  auto denied = state.GetTask("quota-denied-0");
  ASSERT_TRUE(denied.has_value());
  ASSERT_EQ(denied->state, TaskState::Pending);

  Submit(api, Spec("quota-high", {1000, 512}, 10, 1, "tenant-a"));

  const auto low = state.GetTask("quota-low-0");
  const auto high = state.GetTask("quota-high-0");
  denied = state.GetTask("quota-denied-0");
  ASSERT_TRUE(low.has_value());
  ASSERT_TRUE(high.has_value());
  ASSERT_TRUE(denied.has_value());
  ASSERT_EQ(low->state, TaskState::Pending);
  ASSERT_EQ(low->preemptions, 1);
  ASSERT_EQ(high->state, TaskState::Running);
  ASSERT_EQ(denied->state, TaskState::Running);
  ASSERT_TRUE(state.AllocatedToTenant("tenant-a").FitsIn({2000, 8192}));
}

void ReconcilerReschedulesTasksFromMissedHeartbeatNode() {
  InMemoryStateStore state;
  const auto t0 = TimePoint{};
  RegisterNode(state, "node-a", {4000, 8192}, t0);
  RegisterNode(state, "node-b", {4000, 8192}, t0);

  Scheduler scheduler(state);
  ApiServer api(state, scheduler);
  Reconciler reconciler(state, scheduler, std::chrono::seconds(15));
  Submit(api, Spec("service", {1000, 512}, 5, 1));
  ASSERT_EQ(state.GetTask("service-0")->node_id,
            std::optional<std::string>{"node-a"});

  state.TouchNodeHeartbeat("node-b", t0 + std::chrono::seconds(10));
  const auto report = reconciler.Reconcile(t0 + std::chrono::seconds(20));
  ASSERT_EQ(report.unhealthy_nodes.size(), 1U);
  ASSERT_EQ(report.unhealthy_nodes[0], std::string("node-a"));

  const auto task = state.GetTask("service-0");
  ASSERT_TRUE(task.has_value());
  ASSERT_EQ(task->state, TaskState::Running);
  ASSERT_EQ(task->node_id, std::optional<std::string>{"node-b"});
}

void NodeAgentLaunchesAssignedTasks() {
  InMemoryStateStore state;
  ProcessRuntime runtime;
  NodeAgent agent(state, runtime, "node-a", {4000, 8192});
  agent.Register(TimePoint{});

  Scheduler scheduler(state);
  ApiServer api(state, scheduler);
  Submit(api, Spec("launch", {500, 128}, 1, 1));

  const auto report = agent.SyncAssignedTasks();
  ASSERT_EQ(report.launched_tasks.size(), 1U);
  ASSERT_EQ(runtime.Status("launch-0"), RuntimeTaskState::Running);
}

}  // namespace

std::vector<NamedTest> SchedulerTests() {
  return {
      {"node registration tracks healthy capacity",
       NodeRegistrationTracksHealthyCapacity},
      {"best-fit bin-packs without overcommit", BestFitBinPacksWithoutOvercommit},
      {"high-priority task preempts lower-priority work",
       HighPriorityTaskPreemptsLowerPriorityWork},
      {"tenant quota blocks and can preempt within tenant",
       TenantQuotaBlocksAndCanPreemptWithinTenant},
      {"reconciler reschedules tasks from missed heartbeat node",
       ReconcilerReschedulesTasksFromMissedHeartbeatNode},
      {"node agent launches assigned tasks", NodeAgentLaunchesAssignedTasks},
  };
}

#include "agent/node_agent.h"
#include "agent/runtime.h"
#include "control_plane/state_store.h"

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>

int main(int argc, char** argv) {
  using namespace mini_borg;

  const std::string node_id = argc > 1 ? argv[1] : "node-local";
  const auto cpu = argc > 2 ? std::stoll(argv[2]) : 4000;
  const auto memory = argc > 3 ? std::stoll(argv[3]) : 8192;

  InMemoryStateStore state;
  ProcessRuntime runtime;
  NodeAgent agent(state, runtime, node_id, {cpu, memory});
  const auto now = Clock::now();
  agent.Register(now);
  agent.Heartbeat(now + std::chrono::seconds(1));

  const auto node = state.GetNode(node_id);
  if (!node.has_value()) {
    std::cerr << "failed to register " << node_id << "\n";
    return 1;
  }

  std::cout << "registered " << node->id << " capacity="
            << node->capacity.ToString() << " health="
            << ToString(node->health) << "\n";
  return 0;
}

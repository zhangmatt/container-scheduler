# codex.md — Container Scheduler (Borg-style)

## Project Goal
Build a distributed cluster scheduler in modern C++ that places and manages containerized tasks across a fleet of worker nodes, modeled on the concepts in Google's **Borg** (and Kubernetes' scheduler/controller design). It must do resource-aware placement, enforce quotas and priorities, preempt lower-priority work, and reschedule failed tasks via health checking. "Borg-style" = the *concepts*, not a Borg clone.

## Tech Stack
- **Language:** C++20
- **RPC:** gRPC + Protocol Buffers between control plane and agents
- **Cluster state:** etcd (via its gRPC API) for persistent, watchable scheduler state — or a pluggable interface with an etcd backend
- **Workers:** node agents that launch tasks as Docker containers (Docker Engine API) or, to keep it simple and deterministic, as local processes behind a `Runtime` interface with a Docker implementation
- **Build:** CMake; docker-compose to bring up a control plane + N agents
- **Testing:** GoogleTest; a simulation harness that fakes nodes/tasks to test placement and preemption logic deterministically

## Repository Structure
```
mini-borg/
  proto/scheduler.proto      # SubmitTask, NodeHeartbeat, TaskStatus, etc.
  src/
    control_plane/
      scheduler.{h,cpp}      # placement: bin-packing, filters, priority/preemption
      reconciler.{h,cpp}     # control loop: desired vs. observed state
      state_store.{h,cpp}    # etcd-backed cluster state (nodes, tasks, allocations)
      api_server.{h,cpp}     # gRPC: task submission, status queries
    agent/
      node_agent.{h,cpp}     # registers node, sends heartbeats, runs tasks
      runtime.{h,cpp}        # Runtime interface (+ Docker + process impls)
      health.{h,cpp}         # liveness checks, reports task state
    common/resources.{h,cpp} # CPU/mem request & capacity model
    server/control_main.cpp
    server/agent_main.cpp
    cli/borgctl.cpp          # submit/list/kill tasks
  test/
    scheduler_test.cpp       # placement + preemption unit tests
    sim_test.cpp             # many nodes/tasks, failures
  CMakeLists.txt
  docker-compose.yml
  README.md
```

## Core Architecture
- **Declarative / reconciliation model:** clients submit a *desired* task spec (resource requests, priority, replica count). The scheduler decides placement; the reconciler continuously drives observed cluster state toward desired state. This is the Borg/Kubernetes control-loop philosophy.
- **Control plane:** API server (accepts specs) → scheduler (assigns tasks to nodes) → reconciler (watches state, reschedules on failure) → state store (etcd, the single source of truth).
- **Agents:** each worker node runs an agent that registers capacity, heartbeats, launches/kills tasks via the Runtime, and reports health/status.
- **Scheduling cycle:** filter nodes that fit the request (capacity, constraints) → score them (bin-packing: prefer the most-utilized node that still fits, to reduce fragmentation) → bind task to node → agent runs it.
- **Preemption:** if no node fits a high-priority task, evict the lowest-priority tasks whose freed resources make room; preempted tasks return to the pending queue.

## Implementation Phases (BUILD IN THIS ORDER)

### Phase 1 — Cluster State + Node Registration
- Resource model (CPU/mem requests + node capacity).
- Agents register with the control plane and send periodic heartbeats; state store tracks nodes and their free capacity.
- **Acceptance test:** start control plane + 3 agents; control plane lists 3 healthy nodes with correct capacity.

### Phase 2 — Basic Placement (Bin-Packing)
- SubmitTask → scheduler filters feasible nodes → scores (best-fit bin-packing) → binds → agent launches.
- **Acceptance test:** submit tasks summing to more than one node's capacity; they pack onto fewest nodes without overcommit.

### Phase 3 — Health Checking + Rescheduling
- Agents report task liveness; missed heartbeats / dead tasks mark the task failed.
- Reconciler detects under-replication and reschedules onto a healthy node.
- **Acceptance test:** kill a node mid-run; its tasks are rescheduled elsewhere and converge to desired replica count.

### Phase 4 — Priorities, Quotas, Preemption
- Per-task priority and per-tenant quota.
- When a high-priority task can't be placed, preempt lowest-priority tasks to make room; preempted tasks requeue.
- **Acceptance test:** fill the cluster with low-priority tasks, submit a high-priority one → low-priority tasks are evicted and the high-priority task runs.

### Phase 5 — etcd-Backed State + Horizontal Scale
- Persist cluster state to etcd; use watches for change propagation; ensure the control plane can restart and recover state.
- Simulate thousands of tasks/nodes in the sim harness to show the scheduling loop scales.
- **Acceptance test:** restart the control plane; it recovers full cluster state from etcd and keeps reconciling.

### Phase 6 — Docker Runtime + CLI Polish
- Real Docker runtime implementation behind the Runtime interface.
- `borgctl` submit/list/kill; status reflects real container state.

## Key Design Decisions & Interview Defense
- **Why a reconciliation (control-loop) model?** Declarative desired-state + continuous reconciliation is self-healing and idempotent — the same loop that handles initial placement handles failures, scale changes, and restarts. This is the core Borg/Kubernetes insight. Contrast with imperative "do X now" scheduling, which is brittle under failure.
- **Why bin-packing / best-fit?** Packing tasks onto fewer, fuller nodes reduces resource fragmentation and lets idle nodes be reclaimed/scaled down. Discuss the tradeoff: tight packing risks contention and harder preemption; spreading improves fault isolation. Borg leans toward packing with headroom.
- **How does preemption stay safe/fair?** Preempt strictly by priority, lowest first, only enough to fit the pending task; requeue evicted tasks. Explain priority inversion risks and quota enforcement to stop one tenant starving others.
- **Why etcd as the source of truth?** It's a consistent (Raft-backed) watchable key-value store — exactly what a control plane needs for durable state + change notifications. Note the tie-in: etcd itself is a Raft system (cross-reference your Raft project).
- **What happens when the control plane crashes?** Stateless-ish control plane + durable state in etcd means it recovers by reading state and resuming reconciliation; running tasks keep running on agents meanwhile.
- **Scheduling at scale:** discuss why per-task full-cluster scans get expensive and how real schedulers cache node state / shard scheduling / use optimistic concurrency.

## Definition of Done
- control plane + N agents via docker-compose; tasks place, fail over, and preempt correctly.
- State survives a control-plane restart (etcd).
- README has an architecture diagram and the rationale above.
- You can whiteboard the scheduling cycle and a preemption scenario.
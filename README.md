# Mini Borg

A small C++20 cluster scheduler inspired by Google Borg and Kubernetes control
loops. It places tasks across worker nodes, tracks cluster state, reschedules
failed work, enforces tenant quotas, and preempts lower-priority tasks when
higher-priority work needs room.

I built this to make the distributed-systems work explicit: scheduling,
reconciliation, resource accounting, failure recovery, and control-plane
interfaces in C++, with deterministic tests instead of a demo-only prototype.

## What It Shows

- **C++20 systems code:** scheduler, state store, reconciler, node agent, runtime
  interface, CLI/server entrypoints.
- **Distributed-systems design:** desired state, health checks, failover,
  bin-packing placement, priority preemption, quota enforcement.
- **Infra-facing APIs:** Protocol Buffers contract, pluggable state store shaped
  for etcd, runtime boundary for Docker or local processes.
- **Testability:** simulation tests cover placement, preemption, quotas, and
  node failure recovery without requiring a live cluster.

## Architecture

```text
client / borgctl
      |
      v
  ApiServer ---> Scheduler ---> StateStore
                    ^              |
                    |              v
              Reconciler <--- NodeAgent ---> Runtime
```

The control plane accepts task specs, records desired state, schedules work onto
healthy nodes, and keeps reconciling when nodes or tasks fail. Agents register
capacity, heartbeat, and launch assigned tasks through a runtime interface.

## Implemented

- Node registration and heartbeat-based health tracking.
- Best-fit bin-packing scheduler with CPU and memory requests.
- Reconciliation loop that requeues work from unhealthy nodes.
- Tenant quotas and priority-based preemption.
- Deterministic `ProcessRuntime` plus a `DockerRuntime` integration boundary.
- `proto/scheduler.proto` for the gRPC API shape.
- CMake build, Docker Compose scaffold, CLI/demo binaries, and simulation tests.

## Build And Test

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

Or:

```sh
make test
```

## Run The Demo

```sh
./build/control-plane --demo
./build/borgctl demo
./build/node-agent node-a 4000 8192
```

`docker compose up --build` builds the binaries and starts a demo control plane,
three demo agents, and an etcd container reserved for the durable backend.

## Current Scope

The scheduling core is implemented and tested in-process. gRPC, etcd persistence,
and real Docker execution are represented as interfaces and scaffolding so the
core algorithms stay easy to inspect and verify.

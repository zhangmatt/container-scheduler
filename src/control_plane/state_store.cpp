#include "control_plane/state_store.h"

#include <algorithm>

namespace mini_borg {

void InMemoryStateStore::UpsertNode(const NodeInfo& node) {
  std::lock_guard<std::mutex> lock(mutex_);
  nodes_[node.id] = node;
}

bool InMemoryStateStore::TouchNodeHeartbeat(const std::string& node_id,
                                            TimePoint heartbeat_at) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = nodes_.find(node_id);
  if (it == nodes_.end()) {
    return false;
  }
  it->second.last_heartbeat = heartbeat_at;
  it->second.health = NodeHealth::Healthy;
  return true;
}

bool InMemoryStateStore::SetNodeHealth(const std::string& node_id,
                                       NodeHealth health) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = nodes_.find(node_id);
  if (it == nodes_.end()) {
    return false;
  }
  it->second.health = health;
  return true;
}

std::optional<NodeInfo> InMemoryStateStore::GetNode(
    const std::string& node_id) const {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = nodes_.find(node_id);
  if (it == nodes_.end()) {
    return std::nullopt;
  }
  return it->second;
}

std::vector<NodeInfo> InMemoryStateStore::ListNodes() const {
  std::lock_guard<std::mutex> lock(mutex_);
  std::vector<NodeInfo> result;
  result.reserve(nodes_.size());
  for (const auto& [_, node] : nodes_) {
    result.push_back(node);
  }
  std::sort(result.begin(), result.end(),
            [](const NodeInfo& lhs, const NodeInfo& rhs) {
              return lhs.id < rhs.id;
            });
  return result;
}

void InMemoryStateStore::UpsertTask(const TaskInfo& task) {
  std::lock_guard<std::mutex> lock(mutex_);
  tasks_[task.id] = task;
}

bool InMemoryStateStore::BindTask(const std::string& task_id,
                                  const std::string& node_id) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto task = tasks_.find(task_id);
  if (task == tasks_.end() || nodes_.find(node_id) == nodes_.end()) {
    return false;
  }
  task->second.node_id = node_id;
  task->second.state = TaskState::Running;
  return true;
}

bool InMemoryStateStore::UnbindTask(const std::string& task_id,
                                    TaskState next_state) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto task = tasks_.find(task_id);
  if (task == tasks_.end()) {
    return false;
  }
  task->second.node_id.reset();
  task->second.state = next_state;
  return true;
}

bool InMemoryStateStore::PreemptTask(const std::string& task_id) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto task = tasks_.find(task_id);
  if (task == tasks_.end() || task->second.state != TaskState::Running) {
    return false;
  }
  task->second.node_id.reset();
  task->second.state = TaskState::Pending;
  ++task->second.preemptions;
  return true;
}

bool InMemoryStateStore::SetTaskState(const std::string& task_id,
                                      TaskState state) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto task = tasks_.find(task_id);
  if (task == tasks_.end()) {
    return false;
  }
  task->second.state = state;
  if (state == TaskState::Pending || state == TaskState::Killed ||
      state == TaskState::Failed || state == TaskState::Succeeded) {
    task->second.node_id.reset();
  }
  return true;
}

bool InMemoryStateStore::DeleteTask(const std::string& task_id) {
  std::lock_guard<std::mutex> lock(mutex_);
  return tasks_.erase(task_id) > 0;
}

std::optional<TaskInfo> InMemoryStateStore::GetTask(
    const std::string& task_id) const {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = tasks_.find(task_id);
  if (it == tasks_.end()) {
    return std::nullopt;
  }
  return it->second;
}

std::vector<TaskInfo> InMemoryStateStore::ListTasks() const {
  std::lock_guard<std::mutex> lock(mutex_);
  std::vector<TaskInfo> result;
  result.reserve(tasks_.size());
  for (const auto& [_, task] : tasks_) {
    result.push_back(task);
  }
  std::sort(result.begin(), result.end(),
            [](const TaskInfo& lhs, const TaskInfo& rhs) {
              return lhs.id < rhs.id;
            });
  return result;
}

std::vector<TaskInfo> InMemoryStateStore::TasksOnNode(
    const std::string& node_id) const {
  std::lock_guard<std::mutex> lock(mutex_);
  std::vector<TaskInfo> result;
  for (const auto& [_, task] : tasks_) {
    if (task.node_id == node_id && task.state == TaskState::Running) {
      result.push_back(task);
    }
  }
  std::sort(result.begin(), result.end(),
            [](const TaskInfo& lhs, const TaskInfo& rhs) {
              return lhs.id < rhs.id;
            });
  return result;
}

ResourceQuantity InMemoryStateStore::AllocatedOnNode(
    const std::string& node_id) const {
  std::lock_guard<std::mutex> lock(mutex_);
  return AllocatedOnNodeLocked(node_id);
}

ResourceQuantity InMemoryStateStore::AllocatedToTenant(
    const std::string& tenant) const {
  std::lock_guard<std::mutex> lock(mutex_);
  return AllocatedToTenantLocked(tenant);
}

void InMemoryStateStore::SetTenantQuota(const std::string& tenant,
                                        ResourceQuantity quota) {
  std::lock_guard<std::mutex> lock(mutex_);
  quotas_[tenant] = quota;
}

std::optional<ResourceQuantity> InMemoryStateStore::TenantQuota(
    const std::string& tenant) const {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = quotas_.find(tenant);
  if (it == quotas_.end()) {
    return std::nullopt;
  }
  return it->second;
}

ResourceQuantity InMemoryStateStore::AllocatedOnNodeLocked(
    const std::string& node_id) const {
  ResourceQuantity allocated;
  for (const auto& [_, task] : tasks_) {
    if (task.state == TaskState::Running && task.node_id == node_id) {
      allocated = allocated + task.resources;
    }
  }
  return allocated;
}

ResourceQuantity InMemoryStateStore::AllocatedToTenantLocked(
    const std::string& tenant) const {
  ResourceQuantity allocated;
  for (const auto& [_, task] : tasks_) {
    if (task.state == TaskState::Running && task.tenant == tenant) {
      allocated = allocated + task.resources;
    }
  }
  return allocated;
}

}  // namespace mini_borg

#pragma once

#include "common/model.h"

#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace mini_borg {

class StateStore {
 public:
  virtual ~StateStore() = default;

  virtual void UpsertNode(const NodeInfo& node) = 0;
  virtual bool TouchNodeHeartbeat(const std::string& node_id,
                                  TimePoint heartbeat_at) = 0;
  virtual bool SetNodeHealth(const std::string& node_id,
                             NodeHealth health) = 0;
  [[nodiscard]] virtual std::optional<NodeInfo> GetNode(
      const std::string& node_id) const = 0;
  [[nodiscard]] virtual std::vector<NodeInfo> ListNodes() const = 0;

  virtual void UpsertTask(const TaskInfo& task) = 0;
  virtual bool BindTask(const std::string& task_id,
                        const std::string& node_id) = 0;
  virtual bool UnbindTask(const std::string& task_id,
                          TaskState next_state) = 0;
  virtual bool PreemptTask(const std::string& task_id) = 0;
  virtual bool SetTaskState(const std::string& task_id,
                            TaskState state) = 0;
  virtual bool DeleteTask(const std::string& task_id) = 0;
  [[nodiscard]] virtual std::optional<TaskInfo> GetTask(
      const std::string& task_id) const = 0;
  [[nodiscard]] virtual std::vector<TaskInfo> ListTasks() const = 0;
  [[nodiscard]] virtual std::vector<TaskInfo> TasksOnNode(
      const std::string& node_id) const = 0;

  [[nodiscard]] virtual ResourceQuantity AllocatedOnNode(
      const std::string& node_id) const = 0;
  [[nodiscard]] virtual ResourceQuantity AllocatedToTenant(
      const std::string& tenant) const = 0;

  virtual void SetTenantQuota(const std::string& tenant,
                              ResourceQuantity quota) = 0;
  [[nodiscard]] virtual std::optional<ResourceQuantity> TenantQuota(
      const std::string& tenant) const = 0;
};

class InMemoryStateStore final : public StateStore {
 public:
  void UpsertNode(const NodeInfo& node) override;
  bool TouchNodeHeartbeat(const std::string& node_id,
                          TimePoint heartbeat_at) override;
  bool SetNodeHealth(const std::string& node_id, NodeHealth health) override;
  [[nodiscard]] std::optional<NodeInfo> GetNode(
      const std::string& node_id) const override;
  [[nodiscard]] std::vector<NodeInfo> ListNodes() const override;

  void UpsertTask(const TaskInfo& task) override;
  bool BindTask(const std::string& task_id,
                const std::string& node_id) override;
  bool UnbindTask(const std::string& task_id, TaskState next_state) override;
  bool PreemptTask(const std::string& task_id) override;
  bool SetTaskState(const std::string& task_id, TaskState state) override;
  bool DeleteTask(const std::string& task_id) override;
  [[nodiscard]] std::optional<TaskInfo> GetTask(
      const std::string& task_id) const override;
  [[nodiscard]] std::vector<TaskInfo> ListTasks() const override;
  [[nodiscard]] std::vector<TaskInfo> TasksOnNode(
      const std::string& node_id) const override;

  [[nodiscard]] ResourceQuantity AllocatedOnNode(
      const std::string& node_id) const override;
  [[nodiscard]] ResourceQuantity AllocatedToTenant(
      const std::string& tenant) const override;

  void SetTenantQuota(const std::string& tenant,
                      ResourceQuantity quota) override;
  [[nodiscard]] std::optional<ResourceQuantity> TenantQuota(
      const std::string& tenant) const override;

 private:
  [[nodiscard]] ResourceQuantity AllocatedOnNodeLocked(
      const std::string& node_id) const;
  [[nodiscard]] ResourceQuantity AllocatedToTenantLocked(
      const std::string& tenant) const;

  mutable std::mutex mutex_;
  std::unordered_map<std::string, NodeInfo> nodes_;
  std::unordered_map<std::string, TaskInfo> tasks_;
  std::unordered_map<std::string, ResourceQuantity> quotas_;
};

}  // namespace mini_borg

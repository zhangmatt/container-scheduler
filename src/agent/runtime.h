#pragma once

#include "common/model.h"

#include <map>
#include <string>

namespace mini_borg {

enum class RuntimeTaskState {
  NotFound,
  Running,
  Exited,
};

struct RuntimeResult {
  bool ok{false};
  std::string message;
};

class Runtime {
 public:
  virtual ~Runtime() = default;

  virtual RuntimeResult Launch(const TaskInfo& task) = 0;
  virtual RuntimeResult Stop(const std::string& task_id) = 0;
  [[nodiscard]] virtual RuntimeTaskState Status(
      const std::string& task_id) const = 0;
};

class ProcessRuntime final : public Runtime {
 public:
  RuntimeResult Launch(const TaskInfo& task) override;
  RuntimeResult Stop(const std::string& task_id) override;
  [[nodiscard]] RuntimeTaskState Status(const std::string& task_id) const override;

 private:
  std::map<std::string, RuntimeTaskState> tasks_;
};

class DockerRuntime final : public Runtime {
 public:
  RuntimeResult Launch(const TaskInfo& task) override;
  RuntimeResult Stop(const std::string& task_id) override;
  [[nodiscard]] RuntimeTaskState Status(const std::string& task_id) const override;
};

[[nodiscard]] std::string ToString(RuntimeTaskState state);

}  // namespace mini_borg

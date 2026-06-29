#pragma once

#include <cstdint>
#include <string>

namespace mini_borg {

struct ResourceQuantity {
  std::int64_t cpu_millis{0};
  std::int64_t memory_mb{0};

  [[nodiscard]] bool IsNonNegative() const;
  [[nodiscard]] bool IsZero() const;
  [[nodiscard]] bool FitsIn(const ResourceQuantity& capacity) const;
  [[nodiscard]] std::string ToString() const;
};

[[nodiscard]] ResourceQuantity operator+(const ResourceQuantity& lhs,
                                         const ResourceQuantity& rhs);
[[nodiscard]] ResourceQuantity operator-(const ResourceQuantity& lhs,
                                         const ResourceQuantity& rhs);
[[nodiscard]] bool operator==(const ResourceQuantity& lhs,
                              const ResourceQuantity& rhs);
[[nodiscard]] bool operator!=(const ResourceQuantity& lhs,
                              const ResourceQuantity& rhs);

[[nodiscard]] double DominantShare(const ResourceQuantity& used,
                                   const ResourceQuantity& capacity);

}  // namespace mini_borg

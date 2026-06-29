#include "common/resources.h"

#include <algorithm>
#include <limits>
#include <sstream>

namespace mini_borg {

bool ResourceQuantity::IsNonNegative() const {
  return cpu_millis >= 0 && memory_mb >= 0;
}

bool ResourceQuantity::IsZero() const {
  return cpu_millis == 0 && memory_mb == 0;
}

bool ResourceQuantity::FitsIn(const ResourceQuantity& capacity) const {
  return IsNonNegative() && cpu_millis <= capacity.cpu_millis &&
         memory_mb <= capacity.memory_mb;
}

std::string ResourceQuantity::ToString() const {
  std::ostringstream out;
  out << cpu_millis << "m CPU, " << memory_mb << "MiB";
  return out.str();
}

ResourceQuantity operator+(const ResourceQuantity& lhs,
                           const ResourceQuantity& rhs) {
  return {lhs.cpu_millis + rhs.cpu_millis, lhs.memory_mb + rhs.memory_mb};
}

ResourceQuantity operator-(const ResourceQuantity& lhs,
                           const ResourceQuantity& rhs) {
  return {lhs.cpu_millis - rhs.cpu_millis, lhs.memory_mb - rhs.memory_mb};
}

bool operator==(const ResourceQuantity& lhs, const ResourceQuantity& rhs) {
  return lhs.cpu_millis == rhs.cpu_millis && lhs.memory_mb == rhs.memory_mb;
}

bool operator!=(const ResourceQuantity& lhs, const ResourceQuantity& rhs) {
  return !(lhs == rhs);
}

double DominantShare(const ResourceQuantity& used,
                     const ResourceQuantity& capacity) {
  const auto share = [](std::int64_t value, std::int64_t total) {
    if (total <= 0) {
      return value > 0 ? std::numeric_limits<double>::infinity() : 0.0;
    }
    return static_cast<double>(value) / static_cast<double>(total);
  };

  return std::max(share(used.cpu_millis, capacity.cpu_millis),
                  share(used.memory_mb, capacity.memory_mb));
}

}  // namespace mini_borg

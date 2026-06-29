#pragma once

#include <functional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

struct NamedTest {
  std::string name;
  std::function<void()> run;
};

std::vector<NamedTest> SchedulerTests();
std::vector<NamedTest> SimulationTests();

inline void AssertTrue(bool condition, const char* expression, const char* file,
                       int line) {
  if (condition) {
    return;
  }
  std::ostringstream out;
  out << file << ":" << line << " assertion failed: " << expression;
  throw std::runtime_error(out.str());
}

template <typename Lhs, typename Rhs>
void AssertEqual(const Lhs& lhs, const Rhs& rhs, const char* lhs_expression,
                 const char* rhs_expression, const char* file, int line) {
  if (lhs == rhs) {
    return;
  }
  std::ostringstream out;
  out << file << ":" << line << " assertion failed: " << lhs_expression
      << " == " << rhs_expression;
  throw std::runtime_error(out.str());
}

#define ASSERT_TRUE(expression) \
  AssertTrue((expression), #expression, __FILE__, __LINE__)

#define ASSERT_EQ(lhs, rhs) \
  AssertEqual((lhs), (rhs), #lhs, #rhs, __FILE__, __LINE__)

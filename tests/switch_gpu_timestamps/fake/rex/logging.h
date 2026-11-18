#pragma once
#include <format>
#include <string>
#include <vector>
extern std::vector<std::string> test_logs;
template<class... Args> void TestLog(std::format_string<Args...> fmt, Args&&... args) {
  test_logs.push_back(std::format(fmt, std::forward<Args>(args)...));
}
#define REXLOG_INFO(...) TestLog(__VA_ARGS__)
#define REXLOG_WARN(...) TestLog(__VA_ARGS__)

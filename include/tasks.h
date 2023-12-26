#pragma once

#include <thread>
#include <vector>

namespace tasks {
void move(std::vector<std::thread> &threads);
void track(std::vector<std::thread> &threads);
void match(std::vector<std::thread> &threads);
void capture(std::vector<std::thread> &threads);
} // namespace tasks

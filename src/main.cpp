#include "global.h"
#include "tasks.h"
#include "threads.h"

#include "util/vtconsole.h"

#include <cstdlib>
#include <iostream>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/opencv.hpp>
#include <string>
#include <thread>

int run_task(const std::string task) {
  global::init_devices();
  // Register signal handlers
  global::init_signal();
  // Create general pipes
  Context ctx;
  // Task specific threads
  // if (task == "tune")
  //   tasks::tune(ctx);
  // else if (task == "track")
  //   tasks::track(ctx);
  // else if (task == "capture")
  //   tasks::capture(ctx);
  // else
  if (task == "match")
    tasks::match(ctx);
  else {
    std::cerr << "[main] Unknown task: " << task << std::endl;
    exit(1);
  }
  // General threads
  ctx.threads.push_back({"capture/wide", threads::capture_wide(ctx)});
  ctx.threads.push_back({"capture/fovea", threads::capture_fovea(ctx)});
  ctx.threads.push_back({"mems/tx", threads::mems_tx(ctx)});
  ctx.threads.push_back({"mems/rx", threads::mems_rx(ctx)});
  // Wait for threads to terminate
  for (auto &el : ctx.threads) {
    std::cerr << "[main] Waiting for " << el.name << std::endl;
    el.thread.join();
  }
  ctx.threads.clear();
  // Recover signal handlers
  global::deinit_signal();
  return 0;
}

int kiosk();

int main(const int argc, const char **argv) {
  global::init_display();
  int ret_val = 0;
  if (argc < 2)
    ret_val = kiosk();
  else
    ret_val = run_task(argv[1]);
  global::deinit_devices();
  std::cerr << "[main] terminated with code " << ret_val << std::endl;
  return ret_val;
}

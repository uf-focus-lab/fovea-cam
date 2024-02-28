#include "global.h"
#include "outfile.h"
#include "tasks.h"
#include "threads.h"

#include "util/vtconsole.h"

#include <iostream>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/opencv.hpp>
#include <string>

int run_task(const std::string task) {
  // Initialize outfile prefix
  outfile::init(task);
  // Create general pipes
  Context ctx;
  // General threads
  std::cerr << "[main] Launching worker threads" << task << std::endl;
  ctx.threads.push_back({"capture/wide", threads::capture_wide(ctx)});
  ctx.threads.push_back({"capture/fovea", threads::capture_fovea(ctx)});
  ctx.threads.push_back({"mems/tx", threads::mems_tx(ctx)});
  ctx.threads.push_back({"mems/rx", threads::mems_rx(ctx)});
  // Task specific threads
  std::cerr << "[main] Launching task: " << task << std::endl;
  if (task == "tune")
    tasks::tune(ctx);
  else if (task == "align")
    tasks::align(ctx);
  else if (task == "checker")
    tasks::checker(ctx);
  else if (task == "aruco")
    tasks::aruco(ctx);
  else if (task == "match")
    tasks::match(ctx);
  else if (task == "track")
    tasks::track(ctx);
  else if (task == "stabilize")
    tasks::stabilize(ctx);
  else if (task == "test-outfile") {
    for (int i = 1; i < 20; i++) {
      outfile::Item *item =
          new outfile::Item("hello" + std::to_string(i) + ".txt");
      outfile::items.push_back(item);
      *item->fs << "Hello, world " << i << " !" << std::endl;
    }
    global::flag_term = true;
    global::flag_back = true;
  } else {
    std::cerr << "[main] Unknown task: " << task << std::endl;
    global::flag_term = true;
    global::flag_back = true;
    ctx.close();
  }
  std::cerr << "[main] Task " << task << " finished" << std::endl;
  // Wait for threads to terminate
  ctx.join();
  // Reset terminate flag if flag_back is also set
  if (global::flag_back) {
    global::flag_term = false;
    global::flag_back = false;
  }
  // Confirm out files to save
  outfile::conclude();
  // Save config
  global::save_config();
  std::cerr << "[main] Terminating" << std::endl;
  return 0;
}

int kiosk();
void splash();

int main(const int argc, const char **argv) {
  // Register signal handlers
  global::init_signal();
  global::load_config();
  global::init_display();
  // Initialize devices
  global::init_devices();
  int ret_val;
  if (argc < 2)
    ret_val = kiosk();
  else
    ret_val = run_task(argv[1]);
  splash();
  global::deinit_devices();
  global::save_config();
  global::deinit_signal();
  std::cerr << "[main] terminated with code " << ret_val << std::endl;
  return ret_val;
}

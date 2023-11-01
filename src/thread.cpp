#include "threads.h"

namespace thread {

// Pointers to environment variables
struct Env env;
// Signal to kill all threads
bool flag_exit = false;

} // namespace thread

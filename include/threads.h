#pragma once

#include "global.h"

using namespace Spinnaker;

namespace threads {

std::thread mems_tx(Context &);

std::thread mems_rx(Context &);

std::thread capture_wide(Context &);

std::thread capture_fovea(Context &);

} // namespace threads

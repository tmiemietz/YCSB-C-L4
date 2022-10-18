/* Benchmark server using only IPC for communication.
 *
 * Author: Viktor Reusch
 */

#pragma once

#include "utils.h"

namespace sqlite {
namespace ipc {

void registerServer(Registry &);

} // namespace ipc
} // namespace sqlite

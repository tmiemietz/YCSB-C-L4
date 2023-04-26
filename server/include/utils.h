/* Utils for sqlite-server.
 *
 * Author: Viktor Reusch
 */

#pragma once

#include <iostream>
#include <pthread-l4.h>
#include <vector>

#include <l4/re/env>
#include <l4/re/util/br_manager>
#include <l4/re/util/cap_alloc>
#include <l4/re/util/object_registry>
#include <l4/sys/cxx/ipc_epiface>
#include <l4/sys/scheduler>
#include <l4/util/cpu.h>

namespace sqlite {

// Standard size for datasapces used for exchanging information during the
// benchmark, currently set to 1 MiB.
static const size_t YCSBC_DS_SIZE = 1 << 20;

typedef L4Re::Util::Registry_server<L4Re::Util::Br_manager_hooks> Registry;

} // namespace sqlite

namespace ycsbc {

// Print APIC id for debugging purposes.
// On QEMU, this is the same as the CPU index.
static inline void print_apic() {
  if (0) {
    unsigned long eax, ebx = 0, ecx, edx;
    l4util_cpu_cpuid(0x1, &eax, &ebx, &ecx, &edx);
    std::cout << "thread running with APIC id " << (ebx >> 24) << std::endl;
  }
}

// Return a vector with an ascending list of the identifiers of all online CPUS.
static inline std::vector<l4_umword_t> online_cpus() {
  constexpr size_t CPUS_PER_MAP = sizeof(l4_sched_cpu_set_t::map) * 8;

  l4_umword_t offset = 0;
  l4_umword_t cpu_max{};
  l4_sched_cpu_set_t set{};
  L4Re::Env::env()->scheduler()->info(&cpu_max, &set);
  // std::cout << "Maximum number of CPUs: " << cpu_max << std::endl;

  std::vector<l4_umword_t> cpus{};
  cpus.reserve(cpu_max);
  while (offset < cpu_max) {
    if (offset) {
      set.set(0, offset);
      L4Re::Env::env()->scheduler()->info(nullptr, &set);
    }
    for (unsigned i = 0; i < CPUS_PER_MAP; i++) {
      if ((set.map >> i) & 0x1)
        cpus.push_back(offset + i);
    }
    offset += CPUS_PER_MAP;
  }
  return cpus;
}

// Migrate this pthread thread to the specified CPU.
static inline void migrate(l4_umword_t cpu) {
  // Cannot use pthread_setaffinity_np here. It ignores CPUs with id >= 64.
  // 2 is the default pthread priority in L4.
  auto sp = l4_sched_param(2);
  sp.affinity = l4_sched_cpu_set(cpu, 0);
  if (l4_error(L4Re::Env::env()->scheduler()->run_thread(
          Pthread::L4::cap(pthread_self()), sp)))
    throw std::runtime_error{"failed to migrate thread"};
  ycsbc::print_apic();
}

} // namespace ycsbc

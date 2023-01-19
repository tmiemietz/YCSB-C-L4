//
//  ycsbc.cc
//  YCSB-C
//
//  Created by Jinglei Ren on 12/19/14.
//  Copyright (c) 2014 Jinglei Ren <jinglei@ren.systems>.
//  Copyright (c) 2022 Viktor Reusch.
//

#include <cstring>
#include <string>
#include <iostream>
#include <sstream>
#include <vector>
#include <future>
#include <pthread-l4.h>

#include <l4/sys/scheduler>
#include <l4/re/env>

#include "core/utils.h"
#include "core/timer.h"
#include "core/client.h"
#include "core/core_workload.h"
#include "db/db_factory.h"
#include "utils.h"

using namespace std;

void UsageMessage(const char *command);
bool StrStartWith(const char *str, const char *pre);
string ParseCommandLine(int argc, const char *argv[], utils::Properties &props);

static int DelegateClient(ycsbc::DB *db, ycsbc::CoreWorkload *wl, 
    const int num_ops, bool is_loading, l4_umword_t cpu) {
  // Migrate this thread to the specified CPU.
  // std::async uses pthreads internally.
  // Cannot use pthread_setaffinity_np here. It ignores CPUs with id >= 64.
  // 2 is the default pthread priority in L4.
  auto sp = l4_sched_param(2);
  sp.affinity = l4_sched_cpu_set(cpu, 0);
  assert(!l4_error(L4Re::Env::env()->scheduler()->run_thread(Pthread::L4::cap(pthread_self()), sp)));
  ycsbc::print_apic();

  void *ctx = db->Init();
  ycsbc::Client client(*db, *wl, ctx);
  int oks = 0;
  for (int i = 0; i < num_ops; ++i) {
    if (is_loading) {
      oks += client.DoInsert();
    } else {
      oks += client.DoTransaction();
    }
  }
  db->Close(ctx);
  return oks;
}

int main(const int argc, const char *argv[]) {
  utils::Properties props;
  string file_name = ParseCommandLine(argc, argv, props);

  ycsbc::DB *db = ycsbc::DBFactory::CreateDB(props);
  if (!db) {
    cout << "Unknown database name " << props["dbname"] << endl;
    exit(0);
  }

  ycsbc::CoreWorkload wl;
  wl.Init(props);

  db->CreateSchema(wl.Tables());

  const int num_threads = stoi(props.GetProperty("threadcount", "1"));

  // Query online CPUs.
  std::vector<l4_umword_t> cpus = ycsbc::online_cpus();
  assert(!cpus.empty());

  bool avoid_boot_cpu;
  istringstream(props.GetProperty("avoid-boot-cpu", "0")) >> avoid_boot_cpu;
  if (avoid_boot_cpu && cpus.size() > 1)
    // Remove first/boot CPU.
    cpus.erase(cpus.begin());

  bool migrate_rr;
  istringstream(props.GetProperty("migrate-rr", "0")) >> migrate_rr;
  if (!migrate_rr)
    // Only use the first CPU in the list.
    cpus.resize(1);

  // Loads data
  vector<future<int>> actual_ops;
  int total_ops = stoi(props[ycsbc::CoreWorkload::RECORD_COUNT_PROPERTY]);
  for (int i = 0; i < num_threads; ++i) {
    actual_ops.emplace_back(async(launch::async,
        DelegateClient, db, &wl, total_ops / num_threads, true, cpus[i % cpus.size()]));
  }

  assert((int)actual_ops.size() == num_threads);

  int sum = 0;
  for (auto &n : actual_ops) {
    assert(n.valid());
    sum += n.get();
  }
  cerr << "# Loading records:\t" << sum << endl;

  // Peforms transactions
  actual_ops.clear();
  total_ops = stoi(props[ycsbc::CoreWorkload::OPERATION_COUNT_PROPERTY]);
  utils::Timer<double> timer;
  timer.Start();
  for (int i = 0; i < num_threads; ++i) {
    actual_ops.emplace_back(async(launch::async,
        DelegateClient, db, &wl, total_ops / num_threads, false, cpus[i % cpus.size()]));
  }
  assert((int)actual_ops.size() == num_threads);

  sum = 0;
  for (auto &n : actual_ops) {
    assert(n.valid());
    sum += n.get();
  }
  double duration = timer.End();
  cerr << "# Transaction throughput (KTPS)" << endl;
  cerr << props["dbname"] << '\t' << file_name << '\t' << num_threads << '\t';
  cerr << total_ops / duration / 1000 << endl;
}

string ParseCommandLine(int argc, const char *argv[], utils::Properties &props) {
  int argindex = 1;
  string filename;
  while (argindex < argc && StrStartWith(argv[argindex], "-")) {
    if (strcmp(argv[argindex], "-threads") == 0) {
      argindex++;
      if (argindex >= argc) {
        UsageMessage(argv[0]);
        exit(0);
      }
      props.SetProperty("threadcount", argv[argindex]);
      argindex++;
    } else if (strcmp(argv[argindex], "-db") == 0) {
      argindex++;
      if (argindex >= argc) {
        UsageMessage(argv[0]);
        exit(0);
      }
      props.SetProperty("dbname", argv[argindex]);
      argindex++;
    } else if (strcmp(argv[argindex], "-host") == 0) {
      argindex++;
      if (argindex >= argc) {
        UsageMessage(argv[0]);
        exit(0);
      }
      props.SetProperty("host", argv[argindex]);
      argindex++;
    } else if (strcmp(argv[argindex], "-port") == 0) {
      argindex++;
      if (argindex >= argc) {
        UsageMessage(argv[0]);
        exit(0);
      }
      props.SetProperty("port", argv[argindex]);
      argindex++;
    } else if (strcmp(argv[argindex], "-slaves") == 0) {
      argindex++;
      if (argindex >= argc) {
        UsageMessage(argv[0]);
        exit(0);
      }
      props.SetProperty("slaves", argv[argindex]);
      argindex++;
    } else if (strcmp(argv[argindex], "-P") == 0) {
      argindex++;
      if (argindex >= argc) {
        UsageMessage(argv[0]);
        exit(0);
      }
      filename.assign(argv[argindex]);
      ifstream input(argv[argindex]);
      try {
        props.Load(input);
      } catch (const string &message) {
        cout << message << endl;
        exit(0);
      }
      input.close();
      argindex++;
    } else if (strcmp(argv[argindex], "-migrate-rr") == 0) {
      argindex++;
      props.SetProperty("migrate-rr", "1");
    } else if (strcmp(argv[argindex], "-avoid-boot-cpu") == 0) {
      argindex++;
      props.SetProperty("avoid-boot-cpu", "1");
    } else {
      cout << "Unknown option '" << argv[argindex] << "'" << endl;
      exit(0);
    }
  }

  if (argindex == 1 || argindex != argc) {
    UsageMessage(argv[0]);
    exit(0);
  }

  return filename;
}

void UsageMessage(const char *command) {
  cout << "Usage: " << command << " [options]" << endl;
  cout << "Options:" << endl;
  cout << "  -threads n: execute using n threads (default: 1)" << endl;
  cout << "  -db dbname: specify the name of the DB to use (default: basic)" << endl;
  cout << "  -P propertyfile: load properties from the given file. Multiple files can" << endl;
  cout << "                   be specified, and will be processed in the order specified" << endl;
  cout << "  -migrate-rr: assign threads round-robin to CPUs" << endl;
  cout << "  -avoid-boot-cpu: do not migrate threads to the boot CPU" << endl;
}

inline bool StrStartWith(const char *str, const char *pre) {
  return strncmp(str, pre, strlen(pre)) == 0;
}


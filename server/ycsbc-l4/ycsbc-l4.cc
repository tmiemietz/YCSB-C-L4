//
//  ycsbc.cc
//  YCSB-C
//
//  Created by Jinglei Ren on 12/19/14.
//  Copyright (c) 2014 Jinglei Ren <jinglei@ren.systems>.
//  Copyright (c) 2022, 2023 Viktor Reusch and Till Miemietz.
//

#include <cstring>
#include <string>
#include <iostream>
#include <sstream>
#include <vector>
#include <future>

#include "core/utils.h"
#include "core/timer.h"
#include "core/client.h"
#include "core/core_workload.h"
#include "db/db_factory.h"
#include "utils.h"

using namespace std;

/*
 * For clarification, save the type of memory allocator used in this benchmark.
 * We can use compile time config definitions to acquire the type of allocator
 * this binary was linked against.
 */
#if defined(CONFIG_YCSB_MALLOC_TLSF)
const string MALLOC_IMPL = "TLSF";
#elif defined(CONFIG_YCSB_MALLOC_JEMALLOC)
const string MALLOC_IMPL = "jemalloc";
#else
const string MALLOC_IMPL = "system standard allocator";
#endif

void UsageMessage(const char *command);
bool StrStartWith(const char *str, const char *pre);
string ParseCommandLine(int argc, const char *argv[], utils::Properties &props);

static int DelegateClient(ycsbc::DB *db, ycsbc::CoreWorkload *wl, 
    const int num_ops, bool is_loading, l4_umword_t cpu, l4_umword_t db_cpu) {
  // Migrate this thread to the specified CPU.
  // std::async uses pthreads internally.
  ycsbc::migrate(cpu);

  void *ctx = db->Init(db_cpu);
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

  cout << "Starting YCSB benchmark..." << endl;
  cout << "==========================" << endl << endl;
  cout << "Using allocator " << MALLOC_IMPL 
       << " (info is reliable only on L4, beware of DB using another allocator)"
       << endl;

  ycsbc::DB *db = ycsbc::DBFactory::CreateDB(props);
  if (!db) {
    cout << "Unknown database name " << props["dbname"] << endl;
    exit(0);
  }

  cout << "Benchmarking DB: " << props["dbname"] << endl;
  ycsbc::CoreWorkload wl;
  wl.Init(props);

  db->CreateSchema(wl.Tables());

  const int num_threads = stoi(props.GetProperty("threadcount", "1"));

  // Query online CPUs.
  std::vector<l4_umword_t> cpus = ycsbc::online_cpus();
  if (cpus.empty())
    throw std::runtime_error{"cpu list empty"};

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

  bool disperse;
  istringstream(props.GetProperty("disperse", "0")) >> disperse;
  auto select_cpus = +[](std::vector<l4_umword_t> const &cpus, int i) {
    l4_umword_t cpu = cpus[i % cpus.size()];
    return make_pair(cpu, cpu);
  };
  if (disperse)
    select_cpus = +[](std::vector<l4_umword_t> const &cpus, int i) {
      return make_pair(cpus[(2 * i) % cpus.size()],
                       cpus[(2 * i + 1) % cpus.size()]);
    };

  // Loads data
  vector<future<int>> actual_ops;
  int total_ops = stoi(props[ycsbc::CoreWorkload::RECORD_COUNT_PROPERTY]);
  for (int i = 0; i < num_threads; ++i) {
    auto selected_cpus = select_cpus(cpus, i);
    actual_ops.emplace_back(async(launch::async,
        DelegateClient, db, &wl, total_ops / num_threads, true,
        selected_cpus.first, selected_cpus.second));
  }

  assert((int)actual_ops.size() == num_threads);

  int sum = 0;
  for (auto &n : actual_ops) {
    assert(n.valid());
    sum += n.get();
  }
  cerr << endl;
  cerr << "# Loading records:\t" << sum << endl;

  // Peforms transactions
  actual_ops.clear();
  total_ops = stoi(props[ycsbc::CoreWorkload::OPERATION_COUNT_PROPERTY]);
  utils::Timer<double> timer;
  timer.Start();
  for (int i = 0; i < num_threads; ++i) {
    auto selected_cpus = select_cpus(cpus, i);
    actual_ops.emplace_back(async(launch::async,
        DelegateClient, db, &wl, total_ops / num_threads, false,
        selected_cpus.first, selected_cpus.second));
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

  return 0;
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
    } else if (strcmp(argv[argindex], "-disperse") == 0) {
      argindex++;
      props.SetProperty("disperse", "1");
    } else {
      cout << "Unknown option '" << argv[argindex] << "'" << endl;
      exit(0);
    }
  }

  if (argindex == 1 || argindex != argc) {
    UsageMessage(argv[0]);
    exit(0);
  }

  auto const& properties = props.properties();
  bool should_disperse = properties.find("disperse") != properties.end();
  bool should_migrate = properties.find("migrate-rr") != properties.end();
  string dbname = props.GetProperty("dbname", "none");
  if (should_disperse &&
      (!should_migrate || (dbname != "sqlite_ipc" && dbname != "sqlite_shm"))) {
    cout << "Argument -disperse not allowed" << endl;
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
  cout << "  -disperse: assign communicating ycsb and db threads to different CPUs" << endl;
  cout << "              (for sqlite_ipc and sqlite_shm)" << endl;
}

inline bool StrStartWith(const char *str, const char *pre) {
  return strncmp(str, pre, strlen(pre)) == 0;
}


# YCSB-C-L4

Yahoo! Cloud Serving Benchmark (YCSB, https://github.com/brianfrankcooper/YCSB/wiki)
in C++.
This project is a fork of the YCSB-C project (https://github.com/basicthinker/YCSB-C).
This version is ported to run on the L4 operating system, supporting the
built-in database types as well as databases built on top of L4's sqlite
library.

### Building

You can build this YCSB variant just as any other L4 module. For details on the
L4 build system see also <https://l4re.org/doc/l4re_build_system.html>.

E.g. if you would compile only this module in your L4, go to the root directory
of this module and run

```
make O=<path to L4 build dir>
```

In case of encountering errors during compilation, you may want to additionally
specify `V=1` to turn on verbose logging on the build procedure.

### Running Benchmarks

In order to run the benchmarks, some configurations require you to start
a database server before starting the actual benchmark binary. In particular,
this holds for the following database backends:

| Database Backend | Server         | Location Inside This Repo |
|------------------|----------------|---------------------------|
| sqlite\_ipc      | sqlite-ipc-srv | server/sqlite-ipc-srv     |
| sqlite\_shm      | sqlite-shm-srv | server/sqlite-shm-srv     |

See the respective subdirectories for detailed instructions about the single
backend servers.

The `ycsbc-l4` binary is started with the following syntax:

```
ycsbc-l4 -P <path to workload file> -db <database backend name> -threads <cnt>
```

Different database backends may define additional command line options (see 
below).


#### Available database backends

This is a list of all database backends for YCSB-C-L4 and their peculiarities.

##### Basic DB

A dummy database for debugging purposes that merely prints the operations 
received to `stdout`.

- Database backend name: `basic`
- Special options: none
- Required capabilities for ycsbc-l4: none

##### LockStl DB

In-memory database that is based on a thread-safe hash table. Thread-safety is
achieved by the use of standard locks.

- Database backend name: `lock_stl`
- Special options: none
- Required capabilities for ycsbc-l4: none

##### SqliteLib DB

Sqlite database instance that is hosted in the same address space as the
benchmark application (simple use of the sqlite library). Currently, the
database is run in memory with shared caches and the `memory` journaling mode.

- Database backend name: `sqlite_lib`
- Special options: none
- Required capabilities for ycsbc-l4: none

##### SqliteIpc DB

Sqlite database instance that runs in a different process. The communication
between the benchmark process and the `sqlite_ipc` server is done via IPC.
However, note that this only applies to the very invocation of server functions.
All parameters needed for database operations as well as the results of any
queries are transmitted through shared memory windows that are established 
upon startup of a benchmark thread. Note that for each thread of the benchmark
client, a corresponding handler thread on the server side will be spawned.
Since the server internally uses the same library for accessing `sqlite` as the
`sqlite_lib` backend does, the configuration of `sqlite` (database location 
etc.) is equal to that of the library version of `sqlite`.

- Database backend name: `sqlite_ipc`
- Special options: none
- Required capabilities for ycsbc-l4:
    - `ipc`: Client-side end of a communication channel to the Sqlite IPC 
       server.

##### SqliteShm DB

Sqlite database instance that runs in a different process. The communication
between the benchmark process and the `sqlite_shm` server is mainly done via
shared memory (dataspaces).
Only the dataspace capabilities are exchanged via IPC gates.
The kind of operation to perform, all parameters needed for this database
operation, and the results of any queries are transmitted through shared memory
windows that are established upon startup of a benchmark thread.
The first byte of these dataspaces is used to notify the other side about new
messages.
A new message is detected by busy-waiting on this specific byte.
Note that for each thread of the benchmark client, a corresponding handler
thread on the server side will be spawned.
Since the server internally uses the same library for accessing `sqlite` as the
`sqlite_lib` backend does, the configuration of `sqlite` (database location
etc.) is equal to that of the library version of `sqlite`.

- Database backend name: `sqlite_shm`
- Special options: none
- Required capabilities for ycsbc-l4:
    - `shm`: Client-side end of a communication channel to the Sqlite shared
      memory server.

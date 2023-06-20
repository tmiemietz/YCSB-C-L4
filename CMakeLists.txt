# With this file, you can also build this project under Linux using CMake.
# cmake -B build && cmake --build build && ./build/ycsb -db sqlite_lib -P
#   workloads/workloada.spec

# Only tested with the following version.
# You are welcome to change this to a lower version if you are able to test it.
cmake_minimum_required(VERSION 3.26.3)
project(YCSB-C)

# SQLite needs to be installed.
find_package(SQLite3 REQUIRED)

# Set build type to release by default
if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Make Release build type use L4's settings
set(CMAKE_BUILD_TYPE_RELEASE "-O2 -fomit-frame-pointer -fPIE")

# This only supports the lib variant of the SQLite benchmarks.
add_executable(ycsb
    server/ycsbc-l4/ycsbc-l4.cc server/ycsbc-l4/core/core_workload.cc
    server/ycsbc-l4/db/db_factory.cc server/ycsbc-l4/db/hashtable_db.cc
    server/lib/sqlite-lib-db/sqlite_lib_db.cc
)

target_include_directories(ycsb PRIVATE
    server/ycsbc-l4 server/include ${SQLite3_INCLUDE_DIRS}
)
# NO_L4 is used to disable L4-specific code.
target_compile_definitions(ycsb PRIVATE NO_L4)
# Linking SQLite dynamically here as it is easier under many distributions.
target_link_libraries(ycsb PRIVATE ${SQLite3_LIBRARIES})

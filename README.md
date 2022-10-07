# YCSB-C

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

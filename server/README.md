# Rationale for the File Structure of this Project

This package consists of potentially many independent servers. However, none
of these servers will be of particular use outside of the YCSBC context, so we 
decided to keep them together in one package instead of creating multiple L4 
modules.

The core server, i.e., the benchmark application itself can be found in the 
`ycsbc-l4` directory. Beside this, we need several servers for hosting
databases or KVS instances, for configurations of YCSBC where those shall not
be directly integrated into the benchmark application itself, i.e., when one
wants to separate the benchmark and the database in distinct processes. This
might be useful when the performance impact of IPC should be evaluated.

However, there often will be the desire to benchmark both a database directly
integrated into the benchmark as well as an other instance of the exact same 
database running in a separate process. For both cases the code for accessing 
the database is (and for a correct comparison *should be*) equal (e.g. SQL 
commands etc.). This is why we chose to extract the code for *driving* a 
database into a per-database static library that can be linked into both the 
benchmark application directly as well as into a standalone database server.

The `lib` directory hosts the code of these adapter libraries, with one 
subdirectory for each database type supported by YCSBC-L4. The respective header
files for working with these libraries are located inside the `include` 
directory. Furthermore, each supported standalone database should have a 
separate server implementation, put inside a directory with a `-srv` suffix.

The build infrastructure of the existing sqlite example can be used in order to
copy a working infrastructure with custom include paths and extra libraries.

PKGDIR		?= ../..
L4DIR		?= $(PKGDIR)/../..

TARGET		= ycsbc-l4 

# libc_support_misc is needed for a dummy implementation of getpid and calls
# alike, which is in turn needed for linking the ycsbc-l4 object files
# against libsqlite3.a
REQUIRES_LIBS = libstdc++ libsupc++ sqlite libc_support_misc

# Apparently, we need to define __STDC_LIMIT_MACROS to get all definitions from
# stdint.h, see also https://stackoverflow.com/questions/25561677/intmax-max-was-not-declared-in-this-scope?noredirect=1&lq=1
# Seems like the L4 build system also sets -std=gnu++11 (see with make V=1)
CFLAGS		= -std=c++11 -g -Wall -pthread
CXXFLAGS	= -std=c++11 -g -Wall -pthread -D__STDC_LIMIT_MACROS

# Include non-global header files (private to this package)
PRIVATE_INCDIR	= $(PKGDIR)/server/include \
			      $(SRC_DIR)

# We have to include the private sqlitelibdb library from this package
PRIVATE_LIBDIR  = $(PKGDIR_OBJ)/server/lib/sqlite-lib-db/OBJ-$(SYSTEM)

EXTRA_LIBS      = -lycsbc_sqlitelibdb -lycsbc_serializer

SRC_CC		= ycsbc-l4.cc \
			  core/core_workload.cc \
			  db/db_factory.cc \
			  db/hashtable_db.cc \
			  db/sqlite_ipc_db.cc

include $(L4DIR)/mk/prog.mk

PKGDIR		?= ../..
L4DIR		?= $(PKGDIR)/../..

TARGET		= ycsbc-l4

REQUIRES_LIBS = libstdc++ libsupc++

# Apparently, we need to define __STDC_LIMIT_MACROS to get all definitions from
# stdint.h, see also https://stackoverflow.com/questions/25561677/intmax-max-was-not-declared-in-this-scope?noredirect=1&lq=1
# Seems like the L4 build system also sets -std=gnu++11 (see with make V=1)
CFLAGS		= -std=c++11 -g -Wall -pthread 
CXXFLAGS	= -std=c++11 -g -Wall -pthread -D__STDC_LIMIT_MACROS

PRIVATE_INCDIR	= $(SRC_DIR)

SRC_CC		= ycsbc-l4.cc \
			  core/core_workload.cc \
			  db/db_factory.cc \
			  db/hashtable_db.cc

include $(L4DIR)/mk/prog.mk

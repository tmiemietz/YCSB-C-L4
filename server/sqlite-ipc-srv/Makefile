PKGDIR ?= ../..
L4DIR  ?= $(PKGDIR)/../..

TARGET = sqlite-ipc-srv

# Choose malloc backend according to configuration
ifeq ($(CONFIG_YCSB_MALLOC_TLSF),y)
REQUIRES_LIBS = libc_be_mem_tlsf
else ifeq ($(CONFIG_YCSB_MALLOC_JEMALLOC),y)
REQUIRES_LIBS = jemalloc
else
REQUIRES_LIBS :=
endif

REQUIRES_LIBS += libstdc++ libsupc++ sqlite libc_support_misc

PRIVATE_INCDIR = $(PKGDIR)/server/include

# We have to include the private sqlitelibdb library from this package
PRIVATE_LIBDIR  = $(PKGDIR_OBJ)/server/lib/sqlite-lib-db/OBJ-$(SYSTEM)

EXTRA_LIBS      = -lycsbc_sqlitelibdb -lycsbc_serializer

SRC_CC = ipc.cc

include $(L4DIR)/mk/prog.mk

PKGDIR ?= ../..
L4DIR  ?= $(PKGDIR)/../..

TARGET = sqlite-server

REQUIRES_LIBS = libstdc++ libsupc++ sqlite libc_support_misc

PRIVATE_INCDIR = $(PKGDIR)/server/include

# We have to include the private sqlitelibdb library from this package
PRIVATE_LIBDIR  = $(PKGDIR_OBJ)/server/lib/sqlite-lib-db/OBJ-$(SYSTEM)

EXTRA_LIBS      = -lycsbc_sqlitelibdb -lycsbc_serializer

SRC_CC = main.cc ipc.cc

include $(L4DIR)/mk/prog.mk

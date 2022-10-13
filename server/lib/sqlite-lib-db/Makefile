PKGDIR			?= ../../..
L4DIR			?= $(PKGDIR)/../..

REQUIRES_LIBS   = libstdc++ libsupc++ sqlite libc_support_misc

TARGET			= libycsbc_sqlitelibdb.a
PRIVATE_INCDIR  = $(PKGDIR)/server/include

SRC_CC			= sqlite_lib_db.cc

include $(L4DIR)/mk/lib.mk

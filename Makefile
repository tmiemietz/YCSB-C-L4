PKGDIR		= .
L4DIR		?= $(PKGDIR)/../..

TARGET := server workloads

include $(L4DIR)/mk/subdir.mk

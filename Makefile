include ../Makefile.param

###########SMP SRCS & INC ############
SMP_SRCS := $(wildcard $(PWD)/src/*.c)
SMP_INC := -I$(PWD)/src

TARGET := HisiLive
TARGET_PATH := $(PWD)/

include $(PWD)/../$(ARM_ARCH)_$(OSTYPE).mak
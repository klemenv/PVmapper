TOP = ..
include $(TOP)/configure/CONFIG

USR_CXXFLAGS += -std=c++17 -g -ggdb -O0 -DDEBUG

SRCS += logging.cpp
SRCS += config.cpp
SRCS += proto_ca.cpp
SRCS += connection.cpp
SRCS += searcher.cpp
SRCS += listener.cpp
SRCS += iocguard.cpp
SRCS += dispatcher.cpp
SRCS += connmgr.cpp
SRCS += main.cpp

PROD_HOST = pvmapper

include $(TOP)/configure/RULES

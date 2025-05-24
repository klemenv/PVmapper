TOP = ..
include $(TOP)/configure/CONFIG

USR_CXXFLAGS += -std=c++17 -g -ggdb -O0 -DDEBUG

# From PCAS
PROD_LIBS += cas gdd

PROD_LIBS += $(EPICS_BASE_HOST_LIBS)
#PROD_INCLUDES += -I$(EPICS_BASE)/include

SRCS += logging.cpp
SRCS += proto_ca.cpp
SRCS += connection.cpp
SRCS += searcher.cpp
SRCS += listener.cpp
SRCS += directory.cpp
SRCS += main.cpp
#SRCS += requestHandler.cpp
SRCS += util.cpp
#SRCS += proto_ca.cpp
#SRCS += connmgr.cpp

PROD_HOST = cans

include $(TOP)/configure/RULES

TOP = ..
include $(TOP)/configure/CONFIG

USR_CXXFLAGS += -std=c++11 -g -ggdb -O0 -DDEBUG

# From PCAS
PROD_LIBS += cas gdd

PROD_LIBS += $(EPICS_BASE_HOST_LIBS)
#PROD_INCLUDES += -I$(EPICS_BASE)/include

SRCS += main.cpp
SRCS += requestHandler.cpp
SRCS += directory.cpp
SRCS += logging.cpp
SRCS += util.cpp

PROD_HOST = cans

include $(TOP)/configure/RULES

all: udp-client udp-server
CONTIKI=../../..

CFLAGS += -DPROJECT_CONF_H=\"project-conf.h\"


APPS+=powertrace
ifdef SERVER_REPLY
CFLAGS+=-DSERVER_REPLY=$(SERVER_REPLY)
endif
ifdef PERIOD
CFLAGS+=-DPERIOD=$(PERIOD)
endif

CONTIKI_WITH_IPV6 = 1
include $(CONTIKI)/Makefile.include

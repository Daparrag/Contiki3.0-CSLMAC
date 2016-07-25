CONTIKI=../..
CONTIKI_PROJECT = capture glossy
CFLAGS += -DPROJECT_CONF_H=\"project-conf.h\"

APPS = deployment
CONTIKI_WITH_RIME = 1
MODULES += core/net/mac/tsch

all: $(CONTIKI_PROJECT)

include $(CONTIKI)/Makefile.include

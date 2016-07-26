CONTIKI=../../..
CONTIKI_PROJECT = probing
CFLAGS += -DPROJECT_CONF_H=\"project-conf.h\"

PROJECTDIRS += ../tools
PROJECT_SOURCEFILES += node-id.c \
  orchestra-rule-eb-common.c \
  orchestra-rule-eb-common-txonly.c \
  orchestra-rule-eb-per-time-source-listen-all.c \
  orchestra-rule-unicast-per-time-source.c

DEPLOYMENT_COOJA=1
DEPLOYMENT_MOTES=2
DEPLOYMENT_TWIST=3
DEPLOYMENT_INDRIYA=4
DEPLOYMENT_NESTESTBED=5
DEPLOYMENT_IOTLAB=6
DEPLOYMENT_IOTLAB_LIL=7
DEPLOYMENT_IOTLAB_STR=8
DEPLOYMENT_IOTLAB_PAR=9
ifdef DEP
CFLAGS+= -DDEPLOYMENT=$(DEP)
endif

APPS = deployment orchestra
CONTIKI_WITH_RIME = 1
MODULES += core/net/mac/tsch

all: $(CONTIKI_PROJECT)

include $(CONTIKI)/Makefile.include

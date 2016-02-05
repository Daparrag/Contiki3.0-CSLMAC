#all: er-coap-server er-coap-client
all: er-coap-server

CONTIKI = ../../..

# automatically build RESTful resources
REST_RESOURCES_DIR = ./resources
REST_RESOURCES_FILES = $(notdir $(shell find $(REST_RESOURCES_DIR) -name '*.c'))

PROJECTDIRS += $(REST_RESOURCES_DIR)
PROJECT_SOURCEFILES += $(REST_RESOURCES_FILES)


CFLAGS += -DPROJECT_CONF_H=\"project-conf.h\"
CFLAGS += -DUIP_CONF_BUFFER_SIZE=1024

# linker optimizations
SMALL=1

# REST Engine shall use Erbium CoAP implementation
APPS += er-coap
APPS += rest-engine

CONTIKI_WITH_IPV6 = 1
include $(CONTIKI)/Makefile.include

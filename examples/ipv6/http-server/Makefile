CONTIKI_PROJECT=http-server
all: $(CONTIKI_PROJECT) 

CONTIKI=../../..

#linker optimizations
SMALL=1

CFLAGS += -DPROJECT_CONF_H=\"project-conf.h\"
PROJECT_SOURCEFILES += httpd-simple.c
CFLAGS += -DUIP_CONF_TCP=1
CFLAGS += -DWEBSERVER=1

ifeq ($(PREFIX),)
 PREFIX = aaaa::1/64
endif

CONTIKI_WITH_IPV6 = 1
include $(CONTIKI)/Makefile.include

$(CONTIKI)/tools/tunslip6:      $(CONTIKI)/tools/tunslip6.c
	(cd $(CONTIKI)/tools && $(MAKE) tunslip6)

connect-router: $(CONTIKI)/tools/tunslip6
	sudo $(CONTIKI)/tools/tunslip6 $(PREFIX)

connect-router-cooja:   $(CONTIKI)/tools/tunslip6
	sudo $(CONTIKI)/tools/tunslip6 -a 127.0.0.1 $(PREFIX)

#include "contiki.h"
#include "contiki-lib.h"
#include "contiki-net.h"
#include "net/ip/uip.h"
#include "net/ipv6/uip-ds6.h"
#include "net/rpl/rpl.h"

#include "net/netstack.h"
#include "dev/button-sensor.h"
#include "dev/slip.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "net/ip/uip-debug.h"
#include "httpd-simple.h"

static const char *TOP = "<html><head><title>ContikiRPL</title></head><body>\n";
static const char *SCRIPT = "<script src=\"script.js\"></script>\n";
static const char *BOTTOM = "</body></html>\n";
static char buf[512];
static int blen;
#define ADD(...) do {                                                   \
    blen += snprintf(&buf[blen], sizeof(buf) - blen, __VA_ARGS__);      \
  } while(0)


PROCESS(webserver_process, "IoT-LAB Web server");
PROCESS_THREAD(webserver_process, ev, data)
{
  PROCESS_BEGIN();

  httpd_init();

  while(1) {
    PROCESS_WAIT_EVENT_UNTIL(ev == tcpip_event);
    httpd_appcall(data);
  }
  
  PROCESS_END();
}
AUTOSTART_PROCESSES(&webserver_process);

/*---------------------------------------------------------------------------*/
static void
ipaddr_add(const uip_ipaddr_t *addr)
{
  uint16_t a;
  int i, f;
  for(i = 0, f = 0; i < sizeof(uip_ipaddr_t); i += 2) {
    a = (addr->u8[i] << 8) + addr->u8[i + 1];
    if(a == 0 && f >= 0) {
      if(f++ == 0) ADD("::");
    } else {
      if(f > 0) {
        f = -1;
      } else if(i > 0) {
        ADD(":");
      }
      ADD("%x", a);
    }
  }
}
/*---------------------------------------------------------------------------*/
static
PT_THREAD(generate_script(struct httpd_state *s))
{
  PSOCK_BEGIN(&s->sout);
  SEND_STRING(&s->sout, "\
  onload=function() {\
	p=location.host.replace(/::.*/,'::').substr(1);\
	a=document.getElementsByTagName('a');\
	for(i=0;i<a.length;i++) {\
		txt=a[i].innerHTML.replace(/^FE80::/,p);\
		a[i].href='http://['+txt+']';\
	}\
  }");
  PSOCK_END(&s->sout);
}
/*---------------------------------------------------------------------------*/
static
PT_THREAD(generate_routes(struct httpd_state *s))
{
  static uip_ds6_route_t *r;
  static uip_ds6_nbr_t *nbr;

  static uip_ipaddr_t *preferred_parent_ip;
  { /* assume we have only one instance */
    rpl_dag_t *dag = rpl_get_any_dag();
    preferred_parent_ip = rpl_get_parent_ipaddr(dag->preferred_parent);
  }

  PSOCK_BEGIN(&s->sout);

  SEND_STRING(&s->sout, TOP);
  SEND_STRING(&s->sout, SCRIPT);
  blen = 0;
  ADD("Neighbors<pre>");
  for(nbr = nbr_table_head(ds6_neighbors);
      nbr != NULL;
      nbr = nbr_table_next(ds6_neighbors, nbr)) {
      ipaddr_add(&nbr->ipaddr);
      if (uip_ipaddr_cmp(&nbr->ipaddr, preferred_parent_ip)) {
        ADD(" PREFERRED");
      }
      ADD("\n");
      if(blen > sizeof(buf) - 45) {
        SEND_STRING(&s->sout, buf);
        blen = 0;
      }
  }
  ADD("</pre>\nDefault Route<pre>\n");
  SEND_STRING(&s->sout, buf);
  blen = 0;
  ipaddr_add(uip_ds6_defrt_choose());
  ADD("\n"); 
  ADD("</pre>Routes<pre>");
  SEND_STRING(&s->sout, buf);
  blen = 0;
  for(r = uip_ds6_route_head(); r != NULL; r = uip_ds6_route_next(r)) {
    ipaddr_add(&r->ipaddr);
    ADD("/%u (via ", r->length);
    ipaddr_add(uip_ds6_route_nexthop(r));
    if(1 || (r->state.lifetime < 600)) {
      ADD(") %lus\n", (unsigned long)r->state.lifetime);
    } else {
      ADD(")\n");
    }
    SEND_STRING(&s->sout, buf);
    blen = 0;
  }
  ADD("</pre>");
  SEND_STRING(&s->sout, buf);
  SEND_STRING(&s->sout, BOTTOM);

  PSOCK_END(&s->sout);
}
/*---------------------------------------------------------------------------*/
httpd_simple_script_t
httpd_simple_get_script(const char *name)
{
  if (!strcmp("script.js", name))
    return generate_script;
  else 
    return generate_routes;
}

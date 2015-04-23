#include "contiki.h"
#include "contiki-net.h"
#include "drivers/unique_id.h"

void set_rime_addr()
{
#if RIMEADDR_SIZE == 2
    uint16_t short_uid = platform_uid();
    rimeaddr_node_addr.u8[0] = 0xff & (short_uid >> 8);
    rimeaddr_node_addr.u8[1] = 0xff & (short_uid);
#else

#define IOTLAB_UID_ADDR 0
#if !(IOTLAB_UID_ADDR)
    /* Company 3 Bytes */
    rimeaddr_node_addr.u8[0] = 0x01;
    rimeaddr_node_addr.u8[1] = 0x23;
    rimeaddr_node_addr.u8[2] = 0x45;

    /* Platform identifier */
    rimeaddr_node_addr.u8[3] = 0x01;

    /* Generate 4 remaining bytes using uid of processor */
    // use bytes 8-11 to ensure uniqueness (tested empirically)
    int i;
    for (i = 0; i < 4; i++)
        rimeaddr_node_addr.u8[i+4] = uid->uid8[i+8];

#else
    memset(&rimeaddr_node_addr, 0, sizeof(rimeaddr_node_addr));
    uint16_t short_uid = platform_uid();
    rimeaddr_node_addr.u8[0] = 0x02;
    rimeaddr_node_addr.u8[6] = 0xff & (short_uid >> 8);
    rimeaddr_node_addr.u8[7] = 0xff & (short_uid);

#endif
#endif

}

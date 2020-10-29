#include <stdio.h>
#include <string.h>

#include "iot_udp_app.h"
#include "dhcpc.h"
#include "uip.h"
#include "uiplib.h"
#include "iot_api.h"
#ifdef CONFIG_SOFTAP
#include "dhcpd.h"
#endif

extern uint8 gCurrentAddress[];
extern IOT_ADAPTER       IoTpAd;
struct iot_udp_app_state udp_app_state;

#ifdef CONFIG_SOFTAP
extern struct uip_dhcpd_conn uip_dhpcd_conns[UIP_DHCPD_CONNS];
#endif


void
iot_udp_app_init(void)
{
#ifdef CONFIG_SOFTAP
    dhcpd_init();
#else
    dhcpc_init(gCurrentAddress, 6);
#endif

    /********* Customer APP start. **********/

    /********* Customer APP end. **********/

    return;
}

void
iot_udp_appcall(void)
{
    UIP_UDP_CONN *udp_conn = uip_udp_conn;
    u16_t lport, rport;

    lport=HTONS(udp_conn->lport);
    rport=HTONS(udp_conn->rport);

    if (lport == DHCPC_CLIENT_PORT) {
        handle_dhcp();
    }
    /* Customer APP start. */
#ifdef CONFIG_SOFTAP
    else if (lport == DHCPC_SERVER_PORT) {
        handle_dhcpd();
    }
#endif
    /* Customer APP end. */

    return;
}


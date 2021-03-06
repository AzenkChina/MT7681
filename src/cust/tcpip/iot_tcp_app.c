#include "iot_tcp_app.h"
#include "uip.h"
#include "uiplib.h"
#include "iot_api.h"
#include "string.h"
#include "uip_timer.h"
#include "iot_custom.h"
#if (UART_SUPPORT == 1)
#include "bmd.h"
#include "uart_sw.h"
#endif

#if (UART_SUPPORT == 1)
extern UARTStruct UARTPort;
#endif
#if (ATCMD_RECOVERY_SUPPORT==0)
extern volatile uint8 gCommandSupport;
extern volatile bool  PRINT_FLAG;
#endif
/*---------------------------------------------------------------------------*/
/*
 * The initialization function. We must explicitly call this function
 * from the system initialization code, some time after uip_init() is
 * called.
 */
void
iot_tcp_app_init(void)
{
#if TCP_CLI_APP1_ENABLE
    tcp_cli_app1_init();
#endif

#if TCP_SRV_APP1_ENABLE
    uip_listen(HTONS(TCP_SRV_APP1_LOCAL_PORT));
#endif

#if TCP_SRV_APP2_ENABLE
    uip_listen(HTONS(TCP_SRV_APP2_LOCAL_PORT));
#endif

#if TCP_SRV_APP3_ENABLE
    uip_listen(HTONS(TCP_SRV_APP3_LOCAL_PORT));
#endif

}

/*---------------------------------------------------------------------------*/
/*
 * In mt76xx_tcp_app.h we have defined the UIP_APPCALL macro to
 * mt7681_tcp_appcall so that this funcion is uIP's application
 * function. This function is called whenever an uIP event occurs
 * (e.g. when a new connection is established, new data arrives, sent
 * data is acknowledged, data needs to be retransmitted, etc.).
 */
void
iot_tcp_appcall(void)
{
    u16_t lport = HTONS(uip_conn->lport);

#if TCP_CLI_APP1_ENABLE
    if (lport == TCP_CLI_APP1_LOCAL_PORT) {
        handle_tcp_cli_app1();
        return;
    }
#endif

#if TCP_SRV_APP1_ENABLE
    if (lport == TCP_SRV_APP1_LOCAL_PORT) {
        handle_tcp_srv_app1();
        return;
    }
#endif

#if TCP_SRV_APP2_ENABLE
    if (lport == TCP_SRV_APP2_LOCAL_PORT) {
        handle_tcp_srv_app2();
        return;
    }
#endif

#if TCP_SRV_APP3_ENABLE
    if (lport == TCP_SRV_APP3_LOCAL_PORT) {
        handle_tcp_srv_app3();
        return;
    }
#endif

    handle_tcp_app();
}


void
handle_tcp_app(void)
{
    /*
    * The uip_conn structure has a field called "appstate" that holds
    * the application state of the connection. We make a pointer to
    * this to access it easier.
    */
    struct iot_tcp_app_state *s = &(uip_conn->appstate);

    if (uip_aborted() || uip_timedout() || uip_closed()) {
        s->state = IOT_APP_S_CLOSED;
        s->buf = NULL;
        s->len = 0;
        uip_abort();
    }

    if (uip_connected()) {
        s->state = IOT_APP_S_CONNECTED;
    }

    if (uip_acked()) {
        s->state = IOT_APP_S_DATA_ACKED;
        s->buf = NULL;
        s->len = 0;
    }

    if (uip_newdata()) {
    }

    /* check if we have data to xmit for this connection.*/
    if (uip_poll()) {
        if (s->state == IOT_APP_S_CLOSED) {
            uip_close();
        }
    }
}



#if TCP_CLI_APP1_ENABLE
void tcp_cli_app1_init(void)
{
    UIP_CONN *tcp_conn=NULL;
    uip_ipaddr_t raddr;
    uint8 iot_srv_ip[MAC_IP_LEN] = TCP_CLI_APP1_IOT_SRV_IP;

    uip_ipaddr(raddr, iot_srv_ip[0],iot_srv_ip[1], iot_srv_ip[2],iot_srv_ip[3]);

    /* Specify remote address and port here. */
    tcp_conn = uip_connect(&raddr, HTONS(TCP_CLI_APP1_REMOTE_PORT));

    if (tcp_conn) {
        tcp_conn->lport = HTONS(TCP_CLI_APP1_LOCAL_PORT);
    } else {
        printf_high("connect fail\n");
    }
}

void handle_tcp_cli_app1(void)
{
    static struct timer user_timer; //create a timer;
    static bool app_init = FALSE;

    if (uip_newdata()) {
        printf_high("TCP Client RX [%d] bytes\n", uip_datalen());
        iot_uart_output(uip_appdata, uip_datalen());
    }

    if (uip_poll()) {
        /* below codes shows how to send data to client  */
        if ((app_init == FALSE) || timer_expired(&user_timer)) {
            printf_high("TCP CLIENT APP1 uip_poll_timer_expired\n");
            uip_send("hello,this is tcp cli...", 24);
            timer_set(&user_timer, 5*CLOCK_SECOND);
            app_init = TRUE;
        }
    }
}
#endif

#if TCP_SRV_APP1_ENABLE
void handle_tcp_srv_app1(void)
{
    static uint8 result = 0xff;
    char *cptr;

    if (uip_newdata()) {
        if((uip_datalen() > 3) && (uip_datalen() < AT_CMD_MAX_LEN)) {
#if (ATCMD_RECOVERY_SUPPORT==0)
            gCommandSupport = 0xff;
            PRINT_FLAG = TRUE;
#endif
            cptr = (char *)uip_appdata;
            if((cptr[0] == 'A') && (cptr[1] == 'T') && (cptr[2] == '#'))
                result = iot_atcmd_parser(&cptr[3], (uip_datalen()-3));
        }
    }

    if (uip_poll()) {
        /* below codes shows how to send data to client  */
        if(result == 0) {
            uip_send("OK\n", strlen("OK\n")+1);
            result = 0xff;
        }
    }
}
#endif

#if TCP_SRV_APP2_ENABLE
void handle_tcp_srv_app2(void)
{
#if (UART_SUPPORT ==1)
    int16  i = 0;
    int16 rx_len = 0;
    BUFFER_INFO *rx_ring = &(UARTPort.Rx_Buffer);
    char *cptr;
#endif

    if (uip_newdata()) {
#if (UART_SUPPORT ==1)
#if (ATCMD_RECOVERY_SUPPORT==0)
        if(gCommandSupport == 0) {
            if(uip_datalen() > 0) {
            	iot_uart_output(uip_appdata, (int16)uip_datalen());
            }
        }
#endif
#endif
    }

    if (uip_poll()) {
        /* below codes shows how to send data to client  */
#if (UART_SUPPORT ==1)
#if (ATCMD_RECOVERY_SUPPORT==0)
        if(gCommandSupport == 0) {
            cptr = (char *)uip_appdata;
            Buf_GetBytesAvail(rx_ring, rx_len);
            if(rx_len <= 0) {
                return;
            }
            for (i = 0; i < rx_len; i++) {
                if(i >= uip_mss()) {
                    break;
                }
                Buf_Pop(rx_ring, cptr[i]);
            }
            uip_send(uip_appdata, i);
        }
#endif
#endif
    }
}
#endif

#if TCP_SRV_APP3_ENABLE
void handle_tcp_srv_app3(void)
{
	static uint8  set=0, event=0, reset=0;
	static uint8 sta = 0xff;
	static struct timer user_timer;
	char *cptr;
	uint32 gpio_set, gpio_event, gpio_reset;

    if (uip_newdata()) {
        sta = 0xff;
        cptr = (char *)uip_appdata;
        if(uip_datalen() >= 4) {
            cptr[4] = 0;
            if(strcmp(cptr, "STA0") == 0) {
                iot_gpio_output(2,  0);
                sta = 0;
            }
            else if(strcmp(cptr, "STA1") == 0) {
                iot_gpio_output(2,  1);
                sta = 1;
            }
        }
    }

	iot_gpio_output(3,  1);

    if (uip_poll()) {
		cptr = (char *)uip_appdata;
		iot_gpio_input((int32)0, &gpio_event);
		iot_gpio_input((int32)1, &gpio_set);
		iot_gpio_input((int32)3, &gpio_reset);
		if((gpio_set != set) || (gpio_event != event) || (gpio_reset != reset) || timer_expired(&user_timer)) {
			set = gpio_set;
			event = gpio_event;
			reset = gpio_reset;
			sprintf(cptr, "S%cE%cR%c\n", (set?'1':'0'), (event?'1':'0'), (reset?'1':'0'));
			uip_send(cptr, (strlen("S0E0R0\n")+1));
			timer_set(&user_timer, 5*CLOCK_SECOND);
		}
        else if(sta != 0xff) {
            sprintf(cptr, "STA%c\n", (sta?'1':'0'));
            uip_send(cptr, (strlen("STA0\n")+1));
            sta = 0xff;
        }
    }
}
#endif


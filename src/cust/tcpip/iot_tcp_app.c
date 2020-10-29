#include "iot_tcp_app.h"
#include "uip.h"
#include "uiplib.h"
#include "iot_api.h"
#include "string.h"
#include "uip_timer.h"
#if (ATCMD_SUPPORT == 0)
#if (UART_INTERRUPT == 1)
#include "bmd.h"
#include "uart_sw.h"
#endif
#endif

extern IOT_ADAPTER       IoTpAd;
extern u8_t gCurrentAddress[];
extern u16_t http_clientPort;
#if (ATCMD_SUPPORT == 0)
#if (UART_INTERRUPT == 1)
extern UARTStruct UARTPort;
#endif
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
    /* We start to listen for connections on TCP port 7681. */
    uip_listen(HTONS(IoTpAd.ComCfg.Local_TCP_Srv_Port));

#if TCP_CLI_APP1_ENABLE
    tcp_cli_app1_init();
#endif

#if TCP_SRV_APP1_ENABLE
    uip_listen(HTONS(TCP_SRV_APP1_LOCAL_PORT));
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
    u16_t lport = HTONS(uip_conn->lport);
#if (ATCMD_SUPPORT == 0)
#if (UART_INTERRUPT == 1)
    int16  i = 0;
    int16 rx_len = 0;
    BUFFER_INFO *rx_ring = &(UARTPort.Rx_Buffer);
    char *cptr;
#endif
#endif

    if (uip_aborted() || uip_timedout() || uip_closed()) {
        printf("fd %d uip_aborted.%d\n", uip_conn->fd, HTONS(uip_conn->lport));
        s->state = IOT_APP_S_CLOSED;
        s->buf = NULL;
        s->len = 0;
        uip_abort();
    }

    if (uip_connected()) {
#if (ATCMD_SUPPORT != 0)
        u8_t raddr[16];
        u8_t logon_msg[16] = "userlogon:";

        sprintf((char *)raddr, "%d.%d.%d.%d",
                uip_ipaddr1(uip_conn->ripaddr), uip_ipaddr2(uip_conn->ripaddr),
                uip_ipaddr3(uip_conn->ripaddr), uip_ipaddr4(uip_conn->ripaddr));

        printf_high("Connected fd:%d,lp:%d,ra:%s,rp:%d\n",
                    uip_conn->fd, HTONS(uip_conn->lport), raddr, HTONS(uip_conn->rport));
#endif

        s->state = IOT_APP_S_CONNECTED;
    }

    if (uip_acked()) {
        printf("uip_acked.\n");
        s->state = IOT_APP_S_DATA_ACKED;
        s->buf = NULL;
        s->len = 0;
    }

    if (uip_newdata()) {
        printf("RX fd : %d\n", uip_conn->fd);
        if (lport == IoTpAd.ComCfg.Local_TCP_Srv_Port) {
#if (UART_SUPPORT ==1)
#if (UART_INTERRUPT == 1)
            iot_uart_output(uip_appdata, (int16)uip_datalen());
#endif
#endif
        } else {
#if ATCMD_TCPIP_SUPPORT
            iot_uart_output(uip_appdata, (int16)uip_datalen());
#endif
        }
    }

    /* check if we have data to xmit for this connection.*/
    if (uip_poll()) {
#if ATCMD_TCPIP_SUPPORT
        if (s->state == IOT_APP_S_CLOSED) {
            uip_close();
        } else if (s->len > 0) {
            uip_send(s->buf, s->len);
            s->state = IOT_APP_S_DATA_SEND;
        }
#endif
#if (ATCMD_SUPPORT == 0)
        if (s->state == IOT_APP_S_CLOSED) {
            uip_close();
        } else {
#if (UART_SUPPORT ==1)
#if (UART_INTERRUPT == 1)
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
#endif
#endif
        }
#endif
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
    static bool app_init = FALSE;
    char result[32];

    memset(result, 0, sizeof(result));

    if (uip_newdata()) {
		if((uip_datalen() >= strlen("AT#300")) && memcmp(uip_appdata, "AT#300", strlen("AT#300")) == 0) {
			strcpy(result, "Baudrate 300");
		}
		else if((uip_datalen() >= strlen("AT#600")) && memcmp(uip_appdata, "AT#600", strlen("AT#600")) == 0) {
			strcpy(result, "Baudrate 600");
		}
		else if((uip_datalen() >= strlen("AT#1200")) && memcmp(uip_appdata, "AT#1200", strlen("AT#1200")) == 0) {
			strcpy(result, "Baudrate 1200");
		}
		else if((uip_datalen() >= strlen("AT#2400")) && memcmp(uip_appdata, "AT#2400", strlen("AT#2400")) == 0) {
			strcpy(result, "Baudrate 2400");
		}
		else if((uip_datalen() >= strlen("AT#4800")) && memcmp(uip_appdata, "AT#4800", strlen("AT#4800")) == 0) {
			strcpy(result, "Baudrate 4800");
		}
		else if((uip_datalen() >= strlen("AT#7200")) && memcmp(uip_appdata, "AT#7200", strlen("AT#7200")) == 0) {
			strcpy(result, "Baudrate 7200");
		}
		else if((uip_datalen() >= strlen("AT#9600")) && memcmp(uip_appdata, "AT#9600", strlen("AT#9600")) == 0) {
			strcpy(result, "Baudrate 9600");
		}
		else if((uip_datalen() >= strlen("AT#14400")) && memcmp(uip_appdata, "AT#14400", strlen("AT#14400")) == 0) {
			strcpy(result, "Baudrate 14400");
		}
		else if((uip_datalen() >= strlen("AT#19200")) && memcmp(uip_appdata, "AT#19200", strlen("AT#19200")) == 0) {
			strcpy(result, "Baudrate 19200");
		}
		else if((uip_datalen() >= strlen("AT#28800")) && memcmp(uip_appdata, "AT#28800", strlen("AT#28800")) == 0) {
			strcpy(result, "Baudrate 28800");
		}
		else if((uip_datalen() >= strlen("AT#33900")) && memcmp(uip_appdata, "AT#33900", strlen("AT#33900")) == 0) {
			strcpy(result, "Baudrate 33900");
		}
		else if((uip_datalen() >= strlen("AT#38400")) && memcmp(uip_appdata, "AT#38400", strlen("AT#38400")) == 0) {
			strcpy(result, "Baudrate 38400");
		}
		else if((uip_datalen() >= strlen("AT#57600")) && memcmp(uip_appdata, "AT#57600", strlen("AT#57600")) == 0) {
			strcpy(result, "Baudrate 57600");
		}
		else if((uip_datalen() >= strlen("AT#115200")) && memcmp(uip_appdata, "AT#115200", strlen("AT#115200")) == 0) {
			strcpy(result, "Baudrate 115200");
		}
		else if((uip_datalen() >= strlen("AT#7")) && memcmp(uip_appdata, "AT#7", strlen("AT#7")) == 0) {
			strcpy(result, "Data bit 7");
		}
		else if((uip_datalen() >= strlen("AT#8")) && memcmp(uip_appdata, "AT#8", strlen("AT#8")) == 0) {
			strcpy(result, "Data bit 8");
		}
		else if((uip_datalen() >= strlen("AT#none")) && memcmp(uip_appdata, "AT#none", strlen("AT#none")) == 0) {
			strcpy(result, "Parity none");
		}
		else if((uip_datalen() >= strlen("AT#even")) && memcmp(uip_appdata, "AT#even", strlen("AT#even")) == 0) {
			strcpy(result, "Parity even");
		}
		else if((uip_datalen() >= strlen("AT#odd")) && memcmp(uip_appdata, "AT#odd", strlen("AT#odd")) == 0) {
			strcpy(result, "Parity odd");
		}
		else if((uip_datalen() >= strlen("AT#1")) && memcmp(uip_appdata, "AT#1", strlen("AT#1")) == 0) {
			strcpy(result, "Stop bit 1");
		}
		else if((uip_datalen() >= strlen("AT#1.5")) && memcmp(uip_appdata, "AT#1.5", strlen("AT#1.5")) == 0) {
			strcpy(result, "Stop bit 1.5");
		}
		else if((uip_datalen() >= strlen("AT#2")) && memcmp(uip_appdata, "AT#2", strlen("AT#2")) == 0) {
			strcpy(result, "Stop bit 2");
		}
    }

    if (uip_poll()) {
        /* below codes shows how to send data to client  */
        if (app_init == FALSE) {
            if(strlen(result)) {
                uip_send(result, strlen(result)+1);
                app_init = TRUE;
            }
        }
    }
}
#endif


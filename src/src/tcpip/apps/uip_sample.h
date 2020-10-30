/******************************************************************************
 * MODULE NAME:     uip_sample.h
 * PROJECT CODE:    __MT7681__
 * DESCRIPTION:
 * DESIGNER:        Jinchuan.bao
 * DATE:            Nov 2014
 *
 * SOURCE CONTROL:
 *
 * LICENSE:
 *     This source code is copyright (c) 2014 Mediatek. Inc.
 *     All rights reserved.
 *
 * REVISION     HISTORY:
 *   V1.0.0     Nov 2014    - Initial Version V1.0
 *
 *
 * SOURCE:
 * ISSUES:
 *    First Implementation.
 * NOTES TO USERS:
 *
 ******************************************************************************/


#ifndef __UIPSAMPLE_H__
#define __UIPSAMPLE_H__

/***  Example for TCP SERVER APP 1 ****/
#define TCP_SRV_APP1_ENABLE       1

/***  Example for TCP SERVER APP 2 ****/
#define TCP_SRV_APP2_ENABLE       1

/***  Example for TCP SERVER APP 3 ****/
#define TCP_SRV_APP3_ENABLE       1

/***  Example for TCP CLIENT APP 1 ****/
/*This APP make MT7681 as a TCP Client,  it will send TCP packet contrains "hello..."
    message from local port9999 to IoT server port7777 every 5sec,
   and dump the received messages from IoTserver port7777*/
#define TCP_CLI_APP1_ENABLE       0


/**************************************************/
/******************TCP SERVER**********************/
/**************************************************/
#if TCP_SRV_APP1_ENABLE
#define TCP_SRV_APP1_LOCAL_PORT     9998   /*The Local TCP Server Port if MT7681 as a TCP server*/

void handle_tcp_srv_app1(void);
#endif

#if TCP_SRV_APP2_ENABLE
#define TCP_SRV_APP2_LOCAL_PORT     4059   /*The Local TCP Server Port if MT7681 as a TCP server*/

void handle_tcp_srv_app2(void);
#endif

#if TCP_SRV_APP3_ENABLE
#define TCP_SRV_APP3_LOCAL_PORT     4060   /*The Local TCP Server Port if MT7681 as a TCP server*/

void handle_tcp_srv_app3(void);
#endif


/**************************************************/
/******************TCP CLIENT**********************/
/**************************************************/
#if TCP_CLI_APP1_ENABLE
#define TCP_CLI_APP1_LOCAL_PORT       9999   /*The Local TCP Client Port if MT7681 as a TCP client */
#define TCP_CLI_APP1_REMOTE_PORT    7777   /*The Remote TCP Server Port in the WAN/LAN */
#define TCP_CLI_APP1_IOT_SRV_IP        {192,168,100,1}  /*The Remote TCP Server IP Address in the WAN/LAN */

void tcp_cli_app1_init(void);
void handle_tcp_cli_app1(void);
#endif

#endif /* __UIPSAMPLE_H__ */
/** @} */

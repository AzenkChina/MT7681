#ifndef _IOT_CUSTOM_H_
#define _IOT_CUSTOM_H_


/******************************************************************************
* MODULE NAME:     iot_custom.h
* PROJECT CODE:    __MT7681__
* DESCRIPTION:
* DESIGNER:
* DATE:            Jan 2014
*
* SOURCE CONTROL:
*
* LICENSE:
*     This source code is copyright (c) 2014 Mediatek. Inc.
*     All rights reserved.
*
* REVISION     HISTORY:
*   V0.0.1     Jan 2014    - Initial Version V0.0.1
*
*
* SOURCE:
* ISSUES:
*    First Implementation.
* NOTES TO USERS:
*
******************************************************************************/

/******************************************************************************
 * MACRO DEFINITION
 ******************************************************************************/
#include "xip_ovly.h"

/*for record AT cmd Option count*/
#define        MAX_OPTION_COUNT         (15)
#define        AT_CMD_MAX_LEN             (150)

#if (ATCMD_RECOVERY_SUPPORT == 1)
#define     FW_VERISON_CUST         "DLMSConnector 1.0 r1"
#elif (CONFIG_SOFTAP == 1)
#define     FW_VERISON_CUST         "DLMSConnector 1.0 r1"
#else
#define     FW_VERISON_CUST         "DLMSConnector 1.0 r1"
#endif

/*Default setting of User Config Block*/
#define DEFAULT_VENDOR_NEME          "Azenk"
#define DEFAULT_PRODUCT_TYPE         "Proxy"
#define DEFAULT_PRODUCT_NAME        "DLMSConnector"
#define DEFAULT_DHCP_HOST_NAME    DEFAULT_PRODUCT_NAME


/*Default setting of Common Config Block*/
#define DEFAULT_BOOT_FW_IDX                    0    /*1: BootFromAP,   other: BootFromSTA*/
#define DEFAULT_RECOVERY_MODE_STATUS  0    /*not used*/
#define DEFAULT_IO_MODE                             0    /*not used*/

#define DEFAULT_UART_BAUDRATE            UART_BAUD_115200
#define DEFAULT_UART_DATA_BITS           len_8
#define DEFAULT_UART_PARITY                 pa_none
#define DEFAULT_UART_STOP_BITS           sb_1

#define DEFAULT_TCP_UDP_CS                  1       /*0: UDP,  1:TCP  (Default 3*Client, 1*Server is Open)*/
#define DEFAULT_IOT_TCP_SRV_PORT      9998    /*The IoT Server TCP Port  in the internet */
#define DEFAULT_LOCAL_TCP_SRV_PORT  9998    /*The TCP Port  if 7681 as a TCP server */
#define DEFAULT_IOT_UDP_SRV_PORT      9998    /*The IoT Server UDP Port  in the internet */
#define DEFAULT_LOCAL_UDP_SRV_PORT  9998    /*The UDP  Port  if 7681 as a UDP server */

#define DEFAULT_USE_DHCP                      1        /*0: Static IP,  1:Dynamic IP*/
#define DEFAULT_STATIC_IP                     {192,168,100,80}
#define DEFAULT_SUBNET_MASK_IP         {255,255,255,0}
#define DEFAULT_DNS_IP                          {192,168,100,1}
#define DEFAULT_GATEWAY_IP                 {192,168,100,1}
#define DEFAULT_IOT_SERVER_IP             {192,168,100,1}
#define DEFAULT_IOT_CMD_PWD               {0xFF,0xFF,0xFF,0xFF}



/******************************************************************************
 * MACRO DEFINITION  for AT Comamnd
 ******************************************************************************/
#define        AT_CMD_PREFIX_MAX_LEN    12
#define        AT_CMD_PREFIX               "AT#"
#define        AT_CMD_PREFIX2            "iwpriv ra0 "

/* AT command Type */
/* if one or more paramter following the command type*/
/* please reserve a space after command type*/
#define        AT_CMD_UART                 "Uart "
#define        AT_CMD_CHANNEL           "Channel "
#define        AT_CMD_SOFTAP_CFG     "SoftAPConf "
#define        AT_CMD_STA_CFG           "StaConf "
#define        AT_CMD_VER                    "Ver"
#define        AT_CMD_REBOOT             "Reboot"
#define        AT_CMD_DEFAULT           "Default"

#define        AT_CMD_UPDATEFW        "UpdateFW"

#define        AT_CMD_ATE_CAL            "ATECAL "
#define        AT_CMD_EFUSE_SET        "EFUSE "
#define        AT_CMD_FLASH_SET        "FLASH "


/*Only for Debug*/
#define        AT_CMD_MAC_SET           "MAC "
#define        AT_CMD_BBP_SET            "BBP "
#define        AT_CMD_RF_SET              "RF "

#define        AT_CMD_AP                      "AP"
#define        AT_CMD_STA                    "STA"

#define        AT_CMD_CMD               "CMD"
#define        AT_CMD_DATA              "DATA"

/*Default output value for GPIO0~4*/
#define DEFAULT_GPIO04_OUTVAL            (1)

/*Frequency for hardware timer 1 interrupt,  1 ~ 10*/
#define TICK_HZ_HWTIMER1                      (10)         /* T=1/TICK_HZ_HWTIMER1  */

/*Sample for Rx packet Signal strength calculate */
#define IOT_SMP_RSSI_COLLECT                (0)          /*(0)-disable,   (1)-enable*/
#define IOT_SMP_RSSI_COLLECT_PERIOD  (1000)     /*uint:ms*/

typedef enum t_UartRxMode {
    UARTRX_ATCMD_MODE = 1,
    UARTRX_PUREDATA_MODE
} UartRxMode;


/******************************************************************************
 * FUNCTION  DEFINITION
 ******************************************************************************/
void iot_cust_ops(void) XIP_ATTRIBUTE(".xipsec0");
void iot_cust_init(void) XIP_ATTRIBUTE(".xipsec0");
void iot_cust_preinit(void) XIP_ATTRIBUTE(".xipsec0");
void iot_cust_scan_done(void) XIP_ATTRIBUTE(".xipsec0");
void iot_cust_subtask1(void);

void cust_app_init(void);
void pre_init_cfg(void);
void cust_subtask(void);

#ifdef CONFIG_STATION
void IoT_Cust_SMNT_Sta_Chg_Init(void)  XIP_ATTRIBUTE(".xipsec0");
void IoT_Cust_SM_Smnt(void);
#endif


void iot_cust_timer0_timeout_actioin(uint32 param, uint32 param2);
int32 iot_gpio_defval(void) XIP_ATTRIBUTE(".xipsec0");
void reset_cfg(void) XIP_ATTRIBUTE(".xipsec0");
bool load_com_cfg(void) XIP_ATTRIBUTE(".xipsec0");
bool load_usr_cfg(void) XIP_ATTRIBUTE(".xipsec0");
bool reset_usr_cfg(bool bUpFlash) XIP_ATTRIBUTE(".xipsec0");
bool reset_com_cfg(bool bUpFlash) XIP_ATTRIBUTE(".xipsec0");

#ifdef CONFIG_STATION
void store_sta_cfg(void)        XIP_ATTRIBUTE(".xipsec0");
bool load_sta_cfg(void)          XIP_ATTRIBUTE(".xipsec0");
bool reset_sta_cfg(void);   /*not declare this function as XIP func, to avoid system halt*/
void iot_stacfg_update(uint8 *pBSSID, uint8 *pSSID, uint8 AuthMode, uint8 *pPassword)  XIP_ATTRIBUTE(".xipsec0");
#endif

bool default_boot_cfg(void) XIP_ATTRIBUTE(".xipsec0");
bool default_uart_cfg(void) XIP_ATTRIBUTE(".xipsec0");
bool default_ip_cfg(void) XIP_ATTRIBUTE(".xipsec0");

void iot_cust_hwtimer1_hdlr(void);
uint32 iot_cust_hwtimer1_tick(void);

/******************************************************************************
 * FUNCTIONS DEFINITION  -- AT Command & ATE Calibration
 ******************************************************************************/
#if (ATCMD_ATE_SUPPORT == 1)
uint8 RTMPIoctlE2PROM(bool type, char *pBuf, int16 Len)  XIP_ATTRIBUTE(".xipsec0");
uint8 ate_cmd_cali_hdlr(char *pBuf, int16 Len) XIP_ATTRIBUTE(".xipsec0");
int16 iot_exec_atcmd_ate_cal(puchar cmd_buf, int16 Len) XIP_ATTRIBUTE(".xipsec0");
int16 iot_exec_atcmd_ate_cal2(puchar pCmdBuf, int16 AtCmdLen) XIP_ATTRIBUTE(".xipsec0");
#endif
int16 iot_atcmd_parser(puchar cmd_buf, int16 AtCmdLen) XIP_ATTRIBUTE(".xipsec0");
int16 iot_iwcmd_parser(puchar cmd_buf, int16 AtCmdLen) XIP_ATTRIBUTE(".xipsec0");
void iot_atcmd_resp_header(int8 *pHeader, size_t* plen, int8* ATcmdPrefix, int8* ATcmdType) XIP_ATTRIBUTE(".xipsec0");


#if (ATCMD_UART_SUPPORT == 1) && (UART_SUPPORT == 1)
int16 iot_exec_atcmd_uart(puchar pCmdBuf, int16 AtCmdLen) XIP_ATTRIBUTE(".xipsec0");
#endif
#if (ATCMD_FLASH_SUPPORT == 1)
int16 iot_atcmd_exec_flash(puchar pCmdBuf, int16 AtCmdLen) XIP_ATTRIBUTE(".xipsec0");
#endif
#if (ATCMD_EFUSE_SUPPORT == 1)
int16 iot_exec_atcmd_efuse_set(puchar pCmdBuf, int16 AtCmdLen)  XIP_ATTRIBUTE(".xipsec0");
#endif
#if (ATCMD_CH_SWITCH_SUPPORT == 1)
int16 iot_exec_atcmd_ch_switch(puchar pCmdBuf, int16 AtCmdLen) XIP_ATTRIBUTE(".xipsec0");
#endif
#ifdef CONFIG_SOFTAP
int16 iot_exec_atcmd_conf_softap(puchar pCmdBuf, int16 AtCmdLen)  XIP_ATTRIBUTE(".xipsec0");
#endif
#if (ATCMD_REBOOT_SUPPORT == 1)
void iot_atcmd_exec_reboot(void)  XIP_ATTRIBUTE(".xipsec0");
#endif
#if (ATCMD_GET_VER_SUPPORT == 1)
int16 iot_atcmd_exec_ver(puchar pCmdBuf) XIP_ATTRIBUTE(".xipsec0");
#endif

#endif  /* _IOT_CUSTOM_H_ */

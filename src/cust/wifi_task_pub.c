#include <config.h>
#include <types.h>
#include <stdio.h>
#include <stdlib.h>
#include <timer.h>
#include <queue.h>
#include "flash_map.h"
#include "tcpip_main.h"
#if (ATCMD_ATE_SUPPORT == 1)
#include "ate.h"
#endif
#include "iot_api.h"
#include "iot_custom.h"
#include "eeprom.h"
#include "wifi_task.h"

#ifdef CONFIG_SOFTAP
#include "ap_pub.h"
#endif

/*****************************************************************
  Defination
******************************************************************/
#define SYS_SIG_WIFI_IDLE_REQ    (uint32)(1UL << 9)
#define STA_FALLBACK_TIME        (uint32)(30*60*1000)

/*****************************************************************
  Extern Paramter
******************************************************************/
#if (ATCMD_ATE_SUPPORT == 1)
extern     ATE_INFO gATEInfo;
extern  bool  gCaliEnabled;
#endif

/*unit:ms  indicated recovery mode duration*/
#if (ATCMD_RECOVERY_SUPPORT==1)
extern uint16 gRecoveryModeTime;
#endif

extern IOT_ADAPTER   IoTpAd;
extern IOT_CUST_OP   IoTCustOp;
extern MLME_STRUCT  *pIoTMlme;

/*****************************************************************
  Global Paramter
******************************************************************/
MLME_STRUCT      *pIoTMlme;

#ifdef CONFIG_SOFTAP
AP_ADMIN_CONFIG  *pIoTApCfg;
#else
STA_ADMIN_CONFIG *pIoTStaCfg;
#endif

/*gCurrentAddress shall be set in sys_init() as the value stored in flash sta cfg region if the value in flash is valid*/
uint8 gCurrentAddress[MAC_ADDR_LEN]    =    {0x00, 0x0c, 0x43, 0x26, 0x60, 0x40};
uint8 BCAST_ADDR[MAC_ADDR_LEN]      =     {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

/*****************************************************************
  Extern Function
******************************************************************/



/*****************************************************************
  Functions
******************************************************************/

/*========================================================================
    Routine    Description:
        wifi_rx_proc -- Dequeue a buffer from RxPacketQueue which includes received packet (cmd/frame)
        After deal with the packet ,  release buffer to FreeQueue1/2 to recevieing new packet by
        Enqueue operation.

    Arguments:
    Return Value:
    Note:
========================================================================*/
void wifi_rx_proc(void)
{
    pBD_t pBufDesc;
    pQU_t pRxPktQueue;

    pRxPktQueue = iot_get_queue_info();

    /* Check amount of PKT Queue */
    while (pRxPktQueue->Amount > 0) {
        pBufDesc = apiQU_Dequeue(pRxPktQueue);
        if (pBufDesc ==NULL) {
            return;
        }

        if (pBufDesc->SrcPort == 1) {  // from WIFI
            WifiRxDoneInterruptHandle (pBufDesc);
            apiQU_Enqueue(&gFreeQueue1,pBufDesc);
        }
    }

    return;
}


/******************************************************************************
 * wifiTASK_LowPrioTask
 * Description :
 *  Low priority task of WIFI.
 ******************************************************************************/
void
wifiTASK_LowPrioTask (void)
{
    uint32 CurTime = 0;
    static uint32 PreTime = 0;
#if (ATCMD_RECOVERY_SUPPORT==1)
    static int32   RecCnt = 0;
#endif
#if (ATCMD_ATE_SUPPORT == 1)
    static bool    bCaliInited = FALSE;
#endif

    sysTASK_DeassertSignal(SYS_SIG_WIFI_IDLE_REQ);
    SwTimerHandler();
    handle_fce_int();

    /*Periodic Auto Calibration*/
    cali_period_exec();

    /*Exit Recovery or Calibration Mode Process*/
#if (ATCMD_RECOVERY_SUPPORT == 1)
    if ((RecCnt++ > gRecoveryModeTime)
#if (ATCMD_ATE_SUPPORT == 1)
        && (bCaliInited == FALSE)   /*if both Recovey and Calib mode are enabled, need detect Calib Command before reboot*/
#endif
       ) {
        RecCnt = 0;

        /*Set Flag to make loader startup STA/AP FW*/
        iot_set_rec2loader_flag();
        printf_high("<== RecoveryMode\n");

        /*if no AT#UpdateFW to trigger Xmodem , System Reboot to restart loader*/
        iot_sys_reboot();
    }
#endif

#ifdef CONFIG_SOFTAP
    iot_ap_mactbl_mgmt();
#endif

#if (ATCMD_ATE_SUPPORT == 1)
    /*ATE process*/
    if ((bCaliInited == FALSE)  &&  (gCaliEnabled == TRUE))   {
        printf_high("Enter into Calibration Mode \n");
        bCaliInited = TRUE;  /*only call one time while gCaliEnabled=true*/
        wifi_state_chg(WIFI_STATE_INIT,0);
    }

    pIoTMlme->DataEnable = 1;
    wifi_rx_proc();

    if (gCaliEnabled == TRUE)  {
        ate_period_exec();
    }

#else
    wifi_rx_proc();
    wifi_state_machine();
#endif

    cust_subtask();

    CurTime = iot_get_ms_time();
    if ((CurTime < PreTime) || (CurTime - PreTime) > (5*1000))  {
        /* Stay alive message, Print LOG per 5s */
        PreTime = CurTime;

#if (ATCMD_ATE_SUPPORT == 1)
        if (gCaliEnabled == TRUE)
            printf_high("[CalTask]%u \n", PreTime);
        else
            printf_high("[RTask]%u \n", PreTime);
#else
#if (ATCMD_SUPPORT != 0)
        printf_high("[WTask]%u \n", PreTime);
#endif
#endif
    }

    if (iot_get_fceint_state() == 0)
        sysTASK_AssertSignal(SYS_SIG_WIFI_IDLE_REQ);
}

/*========================================================================
    Routine    Description:
        check_data_valid

    Arguments:
    Return Value:    if all data is 0xFF or 0x00, we assume it is invalid
    Note:
========================================================================*/
bool check_data_valid(puchar pdata, uint16 len)
{
    uint16 i=0, No0=0, NoF=0;

    for (i=0; i<len; i++) {
        if (pdata[i] == 0x0) {
            No0++;
        } else if (pdata[i] == 0xFF) {
            NoF++;
        }
    }

    /*if all data is 0xFF or 0x00, we assume it is invalid*/
    if ((No0 == len) || (NoF == len))
        return FALSE;

    return TRUE;
}


/*========================================================================
    Routine    Description:
        wifi state machine -- management the entry for each of wifi state
        because wifi_rx_proc() maybe no need to be called in some state or substate
        a bool param "b_doRx" is be used to declare it

    Arguments:
    Return Value:
    Note:
========================================================================*/
void wifi_state_machine(void)
{
#ifdef CONFIG_STATION
    static uint32 PreTime = 0;
    static uint32 AccTime = 0;
    uint32 CurTime;
    bool b_doRx = FALSE;     /*not use and move wifi_rx_proc() before wifi_state_machine() to fix Dequeue fail*/

    CurTime = iot_get_ms_time();
    if (pIoTMlme->CurrentWifiState != WIFI_STATE_CONNED) {
        if ((CurTime > PreTime) && (CurTime - PreTime) < 2000) {
            AccTime += (CurTime - PreTime);
        }
        if (AccTime > STA_FALLBACK_TIME) {
            iot_switch_to_ap();
        }
    }
    else {
        AccTime = 0;
    }
    PreTime = CurTime;

    switch (pIoTMlme->CurrentWifiState) {
        case WIFI_STATE_INIT:
            ws_init(&b_doRx);
            break;
            /*Smart Connection SM*/
        case WIFI_STATE_SMTCNT:
            ws_smt_conn(&b_doRx);
            break;
            /*Scan SM*/
        case WIFI_STATE_SCAN:
            /*send Probe req frame at first , then listen Probe response in each channel*/
            ws_scan(&b_doRx);
            break;
            /*Auth SM*/
        case WIFI_STATE_AUTH:
            ws_auth(&b_doRx);
            break;
            /*Assoc SM*/
        case WIFI_STATE_ASSOC:
            ws_assoc(&b_doRx);
            break;
            /*4 Way handshake SM*/
        case WIFI_STATE_4WAY:
            ws_4way(&b_doRx);
            break;
            /*Connected SM*/
        case WIFI_STATE_CONNED:
            ws_connected(&b_doRx);
#if CFG_SUPPORT_TCPIP
            if (pIoTMlme->TcpInit == FALSE) {
                /*pIoTMlme->TcpInit shall be set FALSE, once call 
                   wifi_state_chg(WIFI_STATE_INIT, 0) or iot_linkdown()*/
                tcpip_init();
                /*if user add their own tcp/udp connection in other place, but not in tcpip_init()
                   it need to initial such connection here*/
            }
            tcpip_periodic_timer();
#endif
            break;
        default:
            b_doRx = TRUE;
            break;
    }
#endif

#ifdef CONFIG_SOFTAP
#if CFG_SUPPORT_TCPIP
    if (pIoTMlme->TcpInit == FALSE) {
        tcpip_init();
        /*if user add their own tcp/udp connection in other place, but not in tcpip_init()
           it need to initial such connection here*/
    }
    tcpip_periodic_timer();
#endif
#endif

    return;
}


/*========================================================================
    Routine    Description:
        wifi init state machine

    Arguments:
        [OUT]b_enable    need implement wifi_rx_proc() after this function be calling

    Return Value:
        No Return Value
    Note:
========================================================================*/

uint8    Vmac[MAC_ADDR_LEN]={0};
int32        Vcount = 0;

#ifdef CONFIG_STATION
void ws_init(OUT bool *pb_enable)
{
    *pb_enable = TRUE;
    
#if CFG_SUPPORT_4WAY_HS
    /*reset R_Counter for 4-way handshake*/
    memset(pIoTStaCfg->R_Counter, 0, LEN_KEY_DESC_REPLAY);
#endif

    pIoTMlme->ValidFlashStaCfg = load_sta_cfg();

    if (pIoTMlme->ValidFlashStaCfg != TRUE)  {
        store_sta_cfg();
        
    }

    wifi_state_chg(WIFI_STATE_SCAN, SCAN_STA_IDLE);
}

/*========================================================================
    Routine    Description:
        wifi smart connection state machine

    Arguments:
        [OUT]b_enable    need implement wifi_rx_proc() after this function be calling

    Return Value:
        No Return Value
    Note:
========================================================================*/
void ws_smt_conn(OUT bool *pb_enable)
{
}

/*========================================================================
    Routine    Description:
        go to wifi smart connection state machine

    Arguments:
    Return Value:
        No Return Value
    Note:
========================================================================*/
void ws_goto_smnt(void)
{
}

/*========================================================================
    Routine    Description:
        wifi 4 way handshake state machine

    Arguments:
        [OUT]b_enable    need implement wifi_rx_proc() after this function be calling

    Return Value:
        No Return Value
    Note:
========================================================================*/
void ws_4way(OUT bool *pb_enable)
{
    *pb_enable = TRUE;
    return;
}

/*========================================================================
    Routine    Description:
        wifi connected state machine

    Arguments:
        [OUT]b_enable    need implement wifi_rx_proc() after this function be calling

    Return Value:
        No Return Value
    Note:
========================================================================*/
void ws_connected(OUT bool *pb_enable)
{
    *pb_enable = TRUE;
}


/*========================================================================
    Routine    Description:
        load_sta_cfg

    Arguments:
    Return Value:
========================================================================*/
bool load_sta_cfg(void)
{
    bool bFlashInit = FALSE;

    printf_high("load_sta_cfg \n");

	memcpy(pIoTStaCfg->Bssid, "\x00\x0c\x43\x26\x60\x40", strlen("\x00\x0c\x43\x26\x60\x40"));
	memcpy(pIoTStaCfg->Ssid, "DLMSniffer", (strlen("DLMSniffer") + 1));
	pIoTStaCfg->SsidLen = strlen("DLMSniffer");
	memcpy(pIoTStaCfg->Passphase, "12345678", (strlen("12345678") + 1));
	pIoTStaCfg->PassphaseLen = strlen("12345678");
	pIoTStaCfg->AuthMode = Ndis802_11AuthModeWPA1PSKWPA2PSK;

    /* read settings stored on flash STA CONFIG BLOCK */
    //memset(IoTpAd.flash_rw_buf, 0, sizeof(IoTpAd.flash_rw_buf));
    spi_flash_read(FLASH_OFFSET_STA_CFG_START, IoTpAd.flash_rw_buf, sizeof(IoTpAd.flash_rw_buf));

    /*use stored flag to shrink code size*/
    if (IoTpAd.flash_rw_buf[FLASH_STA_CFG_SMNT_INFO_STORED] == SMNT_INFO_STORED)  {
        memcpy(pIoTStaCfg->Bssid,        &IoTpAd.flash_rw_buf[FLASH_STA_CFG_BSSID],        FLASH_STA_CFG_BSSID_LEN);
        memcpy(pIoTStaCfg->Ssid,         &IoTpAd.flash_rw_buf[FLASH_STA_CFG_SSID],        FLASH_STA_CFG_SSID_LEN);
        memcpy(&pIoTStaCfg->SsidLen,    &IoTpAd.flash_rw_buf[FLASH_STA_CFG_SSIDLEN],    FLASH_STA_CFG_SSIDLEN_LEN);

        if ((IoTpAd.flash_rw_buf[FLASH_STA_CFG_PASSPHASELEN] > 0x00) &&
            (IoTpAd.flash_rw_buf[FLASH_STA_CFG_PASSPHASELEN] <= CIPHER_TEXT_LEN))  {
            memcpy(pIoTStaCfg->Passphase,    &IoTpAd.flash_rw_buf[FLASH_STA_CFG_PASSPHASE],   FLASH_STA_CFG_PASSPHASE_LEN);
            memcpy(&pIoTStaCfg->PassphaseLen,&IoTpAd.flash_rw_buf[FLASH_STA_CFG_PASSPHASELEN],FLASH_STA_CFG_PASSPHASELEN_LEN);
            memcpy(&pIoTStaCfg->AuthMode,    &IoTpAd.flash_rw_buf[FLASH_STA_CFG_AUTH_MODE],   FLASH_STA_CFG_AUTH_MODE_LEN);
#if (1 == CFG_SUPPORT_4WAY_HS)
            memcpy(pIoTStaCfg->PMK,     &IoTpAd.flash_rw_buf[FLASH_STA_CFG_4WAY_PMK],    FLASH_STA_CFG_4WAY_PMK_LEN);
#endif
        }

        bFlashInit = TRUE;    /*if has valid setting in flash,  direct go Scan state, but not do smart connect*/
    }

    return bFlashInit;
}

/*========================================================================
    Routine    Description:
        store_sta_cfg

    Arguments:
    Return Value:
========================================================================*/
void store_sta_cfg(void)
{
    printf_high("store_sta_cfg \n");

    /*for shrink code size ,  current we only use 256Byte for  STA_CFG sector */
    memset(IoTpAd.flash_rw_buf, 0xff, sizeof(IoTpAd.flash_rw_buf));

    memcpy(&IoTpAd.flash_rw_buf[FLASH_STA_CFG_BSSID],          pIoTStaCfg->Bssid,            FLASH_STA_CFG_BSSID_LEN);
    memcpy(&IoTpAd.flash_rw_buf[FLASH_STA_CFG_SSID],         pIoTStaCfg->Ssid,             FLASH_STA_CFG_SSID_LEN);
    memcpy(&IoTpAd.flash_rw_buf[FLASH_STA_CFG_PASSPHASE],    pIoTStaCfg->Passphase,        FLASH_STA_CFG_PASSPHASE_LEN);
#if (1 == CFG_SUPPORT_4WAY_HS)
    memcpy(&IoTpAd.flash_rw_buf[FLASH_STA_CFG_4WAY_PMK],      pIoTStaCfg->PMK,             FLASH_STA_CFG_4WAY_PMK_LEN);
#endif
    //memcpy(&IoTpAd.flash_rw_buf[FLASH_STA_CFG_SSIDLEN],     &pIoTStaCfg->SsidLen,         FLASH_STA_CFG_SSIDLEN_LEN);
    //memcpy(&IoTpAd.flash_rw_buf[FLASH_STA_CFG_PASSPHASELEN],&pIoTStaCfg->PassphaseLen,    FLASH_STA_CFG_PASSPHASELEN_LEN);
    //memcpy(&IoTpAd.flash_rw_buf[FLASH_STA_CFG_AUTH_MODE],   &pIoTStaCfg->AuthMode,         FLASH_STA_CFG_AUTH_MODE_LEN);
    IoTpAd.flash_rw_buf[FLASH_STA_CFG_SSIDLEN]         = pIoTStaCfg->SsidLen;
    IoTpAd.flash_rw_buf[FLASH_STA_CFG_PASSPHASELEN] = pIoTStaCfg->PassphaseLen;
    IoTpAd.flash_rw_buf[FLASH_STA_CFG_AUTH_MODE]     = pIoTStaCfg->AuthMode;

    /*Indicate the smart connection info already be stored on flash
       and not do smart connection while IoT device power on, but go
       to SCAN state with the infor stored on flash     */
    IoTpAd.flash_rw_buf[FLASH_STA_CFG_SMNT_INFO_STORED] = SMNT_INFO_STORED;

    spi_flash_write(FLASH_STA_CFG_BASE, IoTpAd.flash_rw_buf, sizeof(IoTpAd.flash_rw_buf));

    memset(IoTpAd.flash_rw_buf, 0xff, sizeof(IoTpAd.flash_rw_buf));
    spi_flash_read(FLASH_OFFSET_EEP_CFG_START, IoTpAd.flash_rw_buf, sizeof(IoTpAd.flash_rw_buf));
    memcpy(&IoTpAd.flash_rw_buf[EEPROM_MAC_12_OFFSET],          pIoTStaCfg->Bssid,    FLASH_STA_CFG_BSSID_LEN);
    spi_flash_erase_sector(FLASH_OFFSET_EEP_CFG_START);
    spi_flash_write(FLASH_OFFSET_EEP_CFG_START, IoTpAd.flash_rw_buf, sizeof(IoTpAd.flash_rw_buf));
}

/*========================================================================
    Routine    Description:
        iot_stacfg_update

    Arguments:
    Return Value:
========================================================================*/
void iot_stacfg_update(uint8 *pBSSID, uint8 *pSSID, uint8 AuthMode, uint8 *pPassword)
{
    uint8 SSIDLen=0, PSWLen=0;

    /*update bssid*/
    if (NULL != pBSSID) {
            NdisZeroMemory(pIoTStaCfg->Bssid, sizeof(pIoTStaCfg->Bssid));
            NdisMoveMemory(pIoTStaCfg->Bssid, pBSSID, sizeof(pIoTStaCfg->Bssid));
    }

    /*update ssid*/
    if (NULL != pSSID) {
        SSIDLen = (uint8)strlen((char*)pSSID);
        if (SSIDLen != 0) {
            if (SSIDLen > MAX_SSID_LEN) {
                SSIDLen = MAX_SSID_LEN;
            }
            pIoTStaCfg->SsidLen = SSIDLen;
            NdisZeroMemory(pIoTStaCfg->Ssid, sizeof(pIoTStaCfg->Ssid));
            NdisMoveMemory(pIoTStaCfg->Ssid, pSSID, SSIDLen);
        }
    }

    /*updata authmode*/
    if (AuthMode < Ndis802_11AuthModeMax) {
        pIoTStaCfg->AuthMode = AuthMode;
    }

    /*updata password*/
    if (NULL != pPassword) {
        PSWLen = strlen((char*)pPassword);
        if (PSWLen != 0) {
            if (PSWLen > MAX_SSID_PASS_LEN) {
                PSWLen = MAX_SSID_PASS_LEN;
            }
            pIoTStaCfg->PassphaseLen= PSWLen;

            NdisZeroMemory(pIoTStaCfg->Passphase, sizeof(pIoTStaCfg->Passphase));
            NdisMoveMemory(pIoTStaCfg->Passphase, pPassword, PSWLen);
        }
    }
}

#endif //#ifdef CONFIG_STATION


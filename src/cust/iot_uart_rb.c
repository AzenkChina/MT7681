#include <stdio.h>
#include "string.h"
#include "types.h"
#include "iot_custom.h"
#include "iot_api.h"
#include "bmd.h"
#include "uart_sw.h"

/******************************************************************************
* MODULE NAME:     iot_uart_rb.c
* PROJECT CODE:    __MT7681__
* DESCRIPTION:
* DESIGNER:
* DATE:            Nov 2014
*
* SOURCE CONTROL:
*
* LICENSE:
*     This source code is copyright (c) 2014 Mediatek Tech. Inc.
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
#if (UART_SUPPORT ==1)
extern IOT_ADAPTER  IoTpAd;
extern char *optarg;

/*2014/05/13,terrence,define uart tx/rx ring buff */
uint8     UARTRxBuf[UARTRX_RING_LEN];    //used in UARTPort.Rx_Buffer
uint8     UARTTxBuf[UARTTX_RING_LEN];    //used in UARTPort.Tx_Buffer
UARTStruct  UARTPort;

char ATCmdPrefixAT[] = AT_CMD_PREFIX;
char ATCmdPrefixIW[] = AT_CMD_PREFIX2;
extern bool UART_TX_POLL_ENABLE;

#if (ATCMD_RECOVERY_SUPPORT==0)
extern volatile uint8 gCommandSupport;
#endif

/**************************************************************************/
/**************************************************************************/
void uart_rxbuf_init(UARTStruct *qp)
{
    PKT_DESC     *rx_desc = &(qp->Rx_desc);
    BUFFER_INFO  *rx_ring = &(qp->Rx_Buffer);

    rx_desc->pkt_num = 0;
    rx_desc->cur_num = 0;
    rx_desc->cur_type = PKT_UNKNOWN;
    Buf_init(rx_ring,(uint8 *)(UARTRxBuf),(uint16)UARTRX_RING_LEN);
}


/*
 * Write data to UART
 * API can be used in either UART Interrupt mode or Polling mode
 */
int16 iot_uart_output (uint8 *msg, int16 count)
{

    if ((msg == NULL) || (count <= 0))   {
        return 0;
    }
    return uart_put_bytes(msg,(uint16)count);
}

/*
 *  task context
 *  write user data to UART tx buffer
 *  uint8 *Buffaddr:pointer to user's data
 *  uint16 Length:  user's data length
 *  return:real length write to tx buffer
 */
uint16 uart_put_bytes(volatile uint8 *pBufAdr, volatile uint16 BufLen)
{
    uint16 len, i;
    volatile uint16 roomleft = 0;
    uint8 *ptr = pBufAdr;
    volatile uint16  size = BufLen;

    /*UART TX interrupt mode*/
    if (UART_TX_POLL_ENABLE == FALSE)  {
        /*when litter tx buff, should use polling*/
        while (size)  {
            len = 0;
            Buf_GetRoomLeft(&(UARTPort.Tx_Buffer), roomleft);

            if (roomleft)  {
                if (size <= roomleft)
                    len = size;
                else if (size > roomleft)
                    len = roomleft;

                for (i = 0; i < len; i++ )
                    Buf_Push(&(UARTPort.Tx_Buffer),*(ptr++));

                size -= len;
            }
            UART_EnableTX();
        }
        return BufLen;
    } else  {
        /*UART TX polling mode*/
        i = 0;
        while (i < BufLen) {
            UART_PutUARTByte(*(pBufAdr + i));
            i++;
        }
        return BufLen;
    }
}


/*
 * 2014/05/13,terrence add call back function in uart isr handler
 * Notice: Must not use printf_high in the RX int32 Handler and here, the Rx FIFO,RxINT and ringbuf will mess
 */
/*
 * ISR context
 * uart_tx_cb() will be called when UART tx interrupt assert
 * fucntion: fetch data from user buffer to tx ring buffer
 *
 */
void uart_tx_cb(void)
{
    //void currently

}



/*
 *  ISR context
 *  Read byte from UART FIFO, we should use it in void uart_rx_cb(void)
 *  uint8 *Byte:  a byte read from FIFO successful
 *  return: 1,read a byte successful;0,FIFO is empty
 */
extern uint16 _UART_GetByte(volatile uint8 *Byte);
uint16 uart_get_byte(volatile uint8 *Byte)
{
    return _UART_GetByte(Byte);
}


/*
 * ISR context
 * uart_rx_cb() will be called when UART rx interrupt assert,
 * then we should featch data from HW FIFO quickly.
 * fucntion: fetch data from HW FIFO to rx ring buffer
 * we should use uart_get_byte(&ch) to fetch byte from HW FIFO as quickly as possible
 */
void uart_rx_cb(void)
{
    uint16   roomleft = 0;
    PKT_FIFO     *infor;
    PKT_FIFO     *temp_info;
    uint8        ch = 0;

    PKT_DESC     *rx_desc = &(UARTPort.Rx_desc);
    BUFFER_INFO  *rx_ring = &(UARTPort.Rx_Buffer);

    static uint8  ATMatchNum = 0;
    static uint8  IWMatchNum = 0;

    /*
     * MCU only forward uart rx data to client
     * here,copy to rx ring and return
     */
 #if (ATCMD_RECOVERY_SUPPORT==0)
	if(gCommandSupport == 0) {
	    while (uart_get_byte(&ch)) {
	        Buf_Push(rx_ring,ch);
	    }
	    return;
	}
#endif

    /*
     * MCU should collect data to be packet
     */
    //normal case
    Buf_GetRoomLeft(rx_ring,roomleft);

    while (uart_get_byte(&ch)) {
        //new receive begin,detect packet header at first
        switch (rx_desc->cur_type)   {
            case PKT_UNKNOWN: {
                /**************** detect packet type ***************/
                //support more ATcmd prefix analysis
                /*case 1:AT#*/
                if (ATCmdPrefixAT[ATMatchNum] == ch) {
                    ATMatchNum++;
                } else {
                    ATMatchNum = 0;
                }
                /*case 2:iwpriv ra0*/
                if (ATCmdPrefixIW[IWMatchNum] == ch) {
                    IWMatchNum++;
                } else {
                    IWMatchNum = 0;
                }

                if ((ATMatchNum == sizeof(ATCmdPrefixAT)-1) || //match case 1: AT#
                    (IWMatchNum == sizeof(ATCmdPrefixIW)-1)) {  //match case 2:iwpriv ra0
                    rx_desc->cur_num = rx_desc->pkt_num;
                    infor = &(rx_desc->infor[rx_desc->cur_num]);
                    infor->pkt_len = 0;

                    if (ATMatchNum == sizeof(ATCmdPrefixAT)-1) {
                        rx_desc->cur_type = PKT_ATCMD;             //match case 1: AT#
                    } else if (IWMatchNum == sizeof(ATCmdPrefixIW)-1) {
                        rx_desc->cur_type = PKT_IWCMD;             //match case 2:iwpriv ra0
                    }

                    ATMatchNum = 0;
                    IWMatchNum = 0;
                    continue;
                }
            }
            break;

            case PKT_ATCMD:
            case PKT_IWCMD: {
                infor = &(rx_desc->infor[rx_desc->cur_num]);

                /*received one complete packet*/
                if (ch == '\n' || ch == '\r')  {
                    //if task has consumed some packets
                    if (rx_desc->cur_num != rx_desc->pkt_num) {
                        temp_info = infor;
                        infor     = &(rx_desc->infor[rx_desc->pkt_num]);
                        infor->pkt_len = temp_info->pkt_len;
                        temp_info->pkt_len = 0;
                        temp_info->pkt_type = PKT_UNKNOWN;
                    }

                    infor->pkt_type = rx_desc->cur_type;  // PKT_ATCMD / PKT_IWCMD;
                    rx_desc->pkt_num++;
                    rx_desc->cur_type = PKT_UNKNOWN;
                } else {
                    /*continue to receiving packet */
                    Buf_Push(rx_ring,ch);
                    roomleft--;
                    infor->pkt_len++;
                }

                /*
                 * if overflow,we discard the current packet
                 * example1:packet length > ring size
                 * example2:rx ring buff can no be freed by task as quickly as rx interrupt coming
                 */
                if ((!roomleft) || (rx_desc->pkt_num >= NUM_DESCS)) {
                    //rollback
                    Buff_RollBack(rx_ring,infor->pkt_len);

                    roomleft += infor->pkt_len;

                    infor->pkt_type = PKT_UNKNOWN;
                    infor->pkt_len = 0;
                    rx_desc->cur_type = PKT_UNKNOWN;

                    if (rx_desc->pkt_num >= NUM_DESCS) {
                        rx_desc->pkt_num--;
                    }
                }
            }
            break;
            default:
                break;
        }
    }
}


/*
 * task context
 * fucntion: dispatch and handle packet from rx ring buffer
 */
void uart_rx_dispatch(void)
{
    PKT_FIFO       *infor;
    PKT_DESC     *rx_desc = &(UARTPort.Rx_desc);
    BUFFER_INFO *rx_ring  = &(UARTPort.Rx_Buffer);

    PKT_TYPE rxpkt_type;
    uint16     rxpkt_len;
    uint16     i;
    uint8     pCmdBuf[UARTRX_RING_LEN];

    while (rx_desc->pkt_num) {
        //simulate FIFO,1st in,1st out
        infor = &(rx_desc->infor[0]);
        rxpkt_type = infor->pkt_type;
        rxpkt_len  = infor->pkt_len;
        memset(pCmdBuf, 0, UARTRX_RING_LEN);

        //copy from uart rx ring
        for (i = 0; i < rxpkt_len; i++) {
            Buf_Pop(rx_ring, pCmdBuf[i]);
        }

        //reset value
        infor->pkt_type = PKT_UNKNOWN;
        infor->pkt_len = 0;

        //shift FIFO
        for (i=0; i<(rx_desc->pkt_num)-1; i++)  {
            rx_desc->infor[i]= rx_desc->infor[i+1];
        }
        rx_desc->pkt_num--;

        //handle previous packet
        switch (rxpkt_type)  {
            case PKT_ATCMD:
                iot_atcmd_parser(pCmdBuf, rxpkt_len);
                break;
            case PKT_IWCMD:
                iot_iwcmd_parser(pCmdBuf, rxpkt_len);
                break;
                // <----custom defined packet dispatch
            default:
                break;
        }
    }
}

void iot_uart_init(void)
{
    /*configure ringbuffer*/
    Buf_init(&(UARTPort.Tx_Buffer),(uint8 *)(UARTTxBuf),(uint16)UARTTX_RING_LEN);
    uart_rxbuf_init((UARTStruct*)(&UARTPort));
}

/*This function shall be called in BSP_init()*/
void uart_cfg_fr_flash(void)
{
    UARTDCBStruct DCBUser;
    DCBUser.baud = IoTpAd.ComCfg.UART_Baudrate;
    DCBUser.dataBits = IoTpAd.ComCfg.UART_DataBits;
    DCBUser.parity = IoTpAd.ComCfg.UART_Parity;
    DCBUser.stopBits = IoTpAd.ComCfg.UART_StopBits;
    UART_SetDCBConfig(&DCBUser);
    return;
}

int32 iot_atcmd_uart_atcfg(puchar pCmdBuf, int16 AtCmdLen)
{
    int16 argc = 0;
    char *argv[MAX_OPTION_COUNT];
    char *opString = "b:w:p:s:?";
    char opt;
    long num;
    UARTDCBStruct  UART_UserConfig;
    UARTDCBStruct *DCBUser;
    char *endptr;

    UART_UserConfig.baud = UART_BAUD_115200;
    UART_UserConfig.dataBits = len_8;
    UART_UserConfig.parity = sb_1;
    UART_UserConfig.stopBits = pa_none;

    /*Select GPIO or Uartlite, Switch to UART */
    mt7681_uart_pin_set();

    DCBUser = (UARTDCBStruct *)&UART_UserConfig;
    /* Setup N81,(UART_WLS_8 | UART_NONE_PARITY | UART_1_STOP) = 0x03 */
    /* BaudRate and autoflowcontrol */
    split_string_cmd(pCmdBuf, AtCmdLen, &argc, argv);
    opt = getopt(argc, argv, opString);

	/* read settings stored on flash Common CONFIG BLOCK */
	spi_flash_read(FLASH_COM_CFG_BASE, IoTpAd.flash_rw_buf, sizeof(IoTpAd.flash_rw_buf));
	if(IoTpAd.flash_rw_buf[FLASH_COM_CFG_INFO_STORED] != COMMON_INFO_STORED) {
        reset_com_cfg(TRUE);
        spi_flash_read(FLASH_COM_CFG_BASE, IoTpAd.flash_rw_buf, sizeof(IoTpAd.flash_rw_buf));
	}

    IoTpAd.ComCfg.BootFWIdx            =  IoTpAd.flash_rw_buf[FLASH_COM_CFG_BOOT_IDX];
    IoTpAd.ComCfg.RecovModeStatus     =  IoTpAd.flash_rw_buf[FLASH_COM_CFG_RECOVERY_MODE_STATUS];
    IoTpAd.ComCfg.IOMode            =  IoTpAd.flash_rw_buf[FLASH_COM_CFG_IO_SET];

    memcpy(&IoTpAd.ComCfg.UART_Baudrate, &IoTpAd.flash_rw_buf[FLASH_COM_CFG_UART_BAUD],   FLASH_COM_CFG_UART_BAUD_LEN);
    IoTpAd.ComCfg.UART_DataBits    =  IoTpAd.flash_rw_buf[FLASH_COM_CFG_UART_DATA_BITS];
    IoTpAd.ComCfg.UART_Parity     =  IoTpAd.flash_rw_buf[FLASH_COM_CFG_UART_PARITY];
    IoTpAd.ComCfg.UART_StopBits    =  IoTpAd.flash_rw_buf[FLASH_COM_CFG_UART_STOP_BITS];

    if ((IoTpAd.ComCfg.UART_Baudrate > UART_BAUD_115200) ||
        (IoTpAd.ComCfg.UART_Parity > pa_even) ||
        ((IoTpAd.ComCfg.UART_DataBits < len_7) || (IoTpAd.ComCfg.UART_DataBits > len_8))  ||
        ((IoTpAd.ComCfg.UART_StopBits < sb_1)  || (IoTpAd.ComCfg.UART_StopBits > sb_1_5)) ) {
        default_uart_cfg();
    }

	spi_flash_erase_sector(FLASH_COM_CFG_BASE);

    while (opt != -1) {
        switch (opt)  {
            case 'b':
                num = simple_strtol(optarg,&endptr,10);
                DCBUser->baud= (uint32)num;
                IoTpAd.ComCfg.UART_Baudrate = num;
                UART_SetDCBConfig(DCBUser);
                break;
            case 'w':
                num = simple_strtol(optarg,&endptr,10);
                DCBUser->dataBits = num;
                IoTpAd.ComCfg.UART_DataBits = num;
                UART_SetDCBConfig(DCBUser);
                break;
            case 'p':
                num = simple_strtol(optarg,&endptr,10);
                DCBUser->parity = num;
                IoTpAd.ComCfg.UART_Parity = num;
                UART_SetDCBConfig(DCBUser);
                break;
            case 's':
                num = simple_strtol(optarg,&endptr,10);
                DCBUser->stopBits = num;
                IoTpAd.ComCfg.UART_StopBits = num;
                UART_SetDCBConfig(DCBUser);
                break;
            case '?':
            default:
                break;
        }
        opt = getopt(argc, argv, opString);
    }

    if ((IoTpAd.ComCfg.UART_Baudrate > UART_BAUD_115200) ||
        (IoTpAd.ComCfg.UART_Parity > pa_even) ||
        ((IoTpAd.ComCfg.UART_DataBits < len_7) || (IoTpAd.ComCfg.UART_DataBits > len_8))  ||
        ((IoTpAd.ComCfg.UART_StopBits < sb_1)  || (IoTpAd.ComCfg.UART_StopBits > sb_1_5)) ) {
        default_uart_cfg();
    }

    IoTpAd.flash_rw_buf[FLASH_COM_CFG_INFO_STORED] = COMMON_INFO_STORED;
    memcpy(&IoTpAd.flash_rw_buf[FLASH_COM_CFG_UART_BAUD],   &IoTpAd.ComCfg.UART_Baudrate, FLASH_COM_CFG_UART_BAUD_LEN);
    IoTpAd.flash_rw_buf[FLASH_COM_CFG_UART_DATA_BITS]    =  IoTpAd.ComCfg.UART_DataBits;
    IoTpAd.flash_rw_buf[FLASH_COM_CFG_UART_PARITY]     =  IoTpAd.ComCfg.UART_Parity;
    IoTpAd.flash_rw_buf[FLASH_COM_CFG_UART_STOP_BITS]    =  IoTpAd.ComCfg.UART_StopBits;

	spi_flash_write(FLASH_COM_CFG_BASE, IoTpAd.flash_rw_buf, sizeof(IoTpAd.flash_rw_buf));

    return 0;
}

#endif

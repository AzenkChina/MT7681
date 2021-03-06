#include <stdio.h>
#include "string.h"
#include "types.h"
#include "iot_api.h"
#include "config.h"
#include "flash_map.h"
#include "uip-conf.h"
#include "eeprom.h"

/******************************************************************************
* MODULE NAME:     iot_parse.c
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
*   V1.0.0     Jan 2014    - Initial Version V1.0
*
*
* SOURCE:
* ISSUES:
*    First Implementation.
* NOTES TO USERS:
*
******************************************************************************/

#if (CFG_SUPPORT_TCPIP == 0)
#define TCPUDP_SRC_PORT        (34932)
#define TCPUDP_DST_PORT        (8176)

uint8 src_addr[] ={192,168,1,10};
uint8 dest_addr[]={255,255,255,255};
#endif

extern IOT_ADAPTER   IoTpAd;
extern uint8 BCAST_ADDR[MAC_ADDR_LEN];

uint8 IoT_CmdPWD_Recv[PASSWORD_MAX_LEN] = {0xFF,0xFF,0xFF,0xFF};

uint16 preSeq = 0;
uint16 appLen = 0;

#if (IOT_PWM_SUPPORT == 1 && IOT_PWM_TYPE == 1)
/* {01011100B,00000000B,00001110B,7,1,4}; */
const IoTHardwareRresource mt7681_HardwareRresource = {0x5C, 0x00, 0x0E, 0x7, 0x1, 0x4};
#elif (IOT_PWM_SUPPORT == 1 && IOT_PWM_TYPE == 2)
/* {00011111B,00000001B,00000000B,5,1,5}; */
const IoTHardwareRresource mt7681_HardwareRresource = {0x1F, 0x01, 0x0E, 0x5, 0x1, 0x5};
#else
/* {00011111B,00000001B,00000000B,7,1,4};*/
const IoTHardwareRresource mt7681_HardwareRresource = {0x1F, 0x01, 0x00, 0x7, 0x1, 0x4};
#endif

IoTHardwareStatus mt7681_HardwareStatus = {0,0,0};

int32 iot_app_proc_func_pkt(
    DataHeader* Dataheader,
    uint8 FunctionType,
    IoTPacketInfo  *pPacketInfo)
{

    GPIO_Information *GpioData;
    uint32 GPIO_Value;
    GPIO_Information *GPIO_Information_out;
    PWM_Information *LedData;
    UART_Information *UartData;
    PWM_Information *LedData_out;
    UART_Information *UartData_out;
    uint32 gpio_input=0;
    //uint8 gpio_read=0,  Polarity=0;
    uint8 iot_buffer[IOT_BUFFER_LEN]={0};
    puchar Data;
    uint8 * Data_out;
    uint16 payload_len =0;
    int8 i=0;
    bool Need_resp = TRUE;
    Status *Status_out;
    uint16 uart_tx_length = 0;

    Data = (puchar)(Dataheader) + CP_DATA_HDR_LEN;
    Data_out = (uint8 *)(iot_buffer+CP_HDR_LEN +CP_DATA_HDR_LEN);
    GpioData = (GPIO_Information *)Data;
    LedData = (PWM_Information *)Data;
    UartData = (UART_Information *)Data;
    LedData_out = (PWM_Information *)Data_out;
    UartData_out = (UART_Information *)Data_out;
    GPIO_Information_out = (GPIO_Information *) Data_out;
    Status_out = (Status *) Data_out;

    if (memcmp(&pPacketInfo->SessionID,  IoTpAd.ComCfg.CmdPWD, PASSWORD_MAX_LEN)) {
        return 0;
    }

    switch (FunctionType) {
        case GPIO_SET_REQUEST :
            GPIO_Value = GpioData->GPIO_Value;
            if (Dataheader->Type == GPIO_INFORMATION) {
                if (pPacketInfo->Sequence != preSeq) {
                    for (i=0; i < mt7681_HardwareRresource.GPIO_Count; i++) {
                        if (((mt7681_HardwareRresource.GPIO_Rresource)&((uint32)1<<i))==0)
                            continue;
#if ((IOT_PWM_SUPPORT == 1) && (IOT_PWM_TYPE == 2))
                        /*if this GPIO has been set as PWM, cancel PWM first*/
                        iot_sw_pwm_del(i);
#endif
                        if ((GPIO_Value)&((uint32)1<<i)) {
                            iot_gpio_output(i,1);
                        } else {
                            iot_gpio_output(i,0);
                        }
                    }
                }

                IoT_build_app_response_header(iot_buffer, FUNCTION, GPIO_SET_RESPONSE, STATUS, sizeof(Status), pPacketInfo);
                Status_out->StatusCode = 0;
            }

            payload_len = (uint16)(CP_HDR_LEN+CP_DATA_HDR_LEN+sizeof(Status));
            break;

        case PWM_SET_REQUEST:
#if (IOT_PWM_SUPPORT == 1)
            if (Dataheader->Type == PWM_INFORMATION) {
                uint16 def_resolution=0;
                def_resolution = iot_cust_pwm_def_resolution();
                
                /*this is a sample,  customer maybe use other GPIO for PWM*/
                iot_sw_pwm_add(1, LedData->RedLevel,    def_resolution);
                iot_sw_pwm_add(2, LedData->GreenLevel, def_resolution);
                iot_sw_pwm_add(3, LedData->BlueLevel,    def_resolution);

                /*Notice: Only for MT7681 EVB board demo to turn off LED, as in MT7681 EVB Hardware, 
                   if dutycycle=0, MT7681 LED on.  dutycyle=resolution, MT7681 LED off.
                   Developer can delete below iot_sw_pwm_del()codes if use other MT7681 board which
                   dutycyle=0, MT7681 LED off.   dutycycle=resolution, MT7681 LED on*/
                if (LedData->RedLevel == 0)
                     iot_sw_pwm_del(1);
                if (LedData->GreenLevel == 0)
                     iot_sw_pwm_del(2);
                if (LedData->BlueLevel == 0)
                     iot_sw_pwm_del(3);


                mt7681_HardwareStatus.RedLevel = LedData->RedLevel;
                mt7681_HardwareStatus.GreenLevel = LedData->GreenLevel;
                mt7681_HardwareStatus.BlueLevel = LedData->BlueLevel;
                
                IoT_build_app_response_header(iot_buffer, FUNCTION, PWM_SET_RESPONSE, STATUS, sizeof(Status), pPacketInfo);
                Status_out->StatusCode = 0;
            }

            payload_len = (uint16)(CP_HDR_LEN+CP_DATA_HDR_LEN+sizeof(Status));
#else
            return 0;
#endif
            break;

        case UART_SET_REQUEST:
            if (Dataheader->Type == UART_INFORMATION) {
                if (pPacketInfo->Sequence != preSeq) {
                    if (Dataheader->Length != appLen - CP_HDR_LEN - CP_DATA_HDR_LEN)  {
                        Status_out->StatusCode = 1;
                    } else {
                        Status_out->StatusCode = 0;
                        uart_tx_length = Dataheader->Length;
                        iot_uart_output((uint8 *)UartData->Data, uart_tx_length);
                    }
                }
                IoT_build_app_response_header(iot_buffer, FUNCTION, UART_SET_RESPONSE, STATUS, sizeof(Status), pPacketInfo);
            }

            payload_len = (uint16)(CP_HDR_LEN+CP_DATA_HDR_LEN+sizeof(Status));
            break;

        case GPIO_GET_REQUEST:
            IoT_build_app_response_header(iot_buffer, FUNCTION, GPIO_GET_RESPONSE, GPIO_INFORMATION, sizeof(GPIO_Information), pPacketInfo);

            GPIO_Information_out->GPIO_List = 0;
            GPIO_Information_out->GPIO_Value = 0;

            for (i=0; i < mt7681_HardwareRresource.GPIO_Count; i++) {
                if (!((mt7681_HardwareRresource.GPIO_Rresource)&((uint32)1<<i)))
                    continue;
#if ((IOT_PWM_SUPPORT == 1) && (IOT_PWM_TYPE == 2))
                /*if this GPIO has been set as PWM, cancel PWM first*/
                iot_sw_pwm_del(i);
#endif
                iot_gpio_input((int32)i, &gpio_input);         /*shall change GPIO to input mode*/
                //iot_gpio_read((int32)i, &gpio_read, &Polarity);  /*Only read GPIO val and mode, not change GPIO to input mode*/

                GPIO_Information_out->GPIO_List |= ((uint32)0x01<<i);

                if (gpio_input == 1) {
                    GPIO_Information_out->GPIO_Value |= ((uint32)0x01<<i);
                }
            }

            payload_len = (uint16)(CP_HDR_LEN+CP_DATA_HDR_LEN+sizeof(GPIO_Information));
            break;

        case PWM_GET_REQUEST:
            IoT_build_app_response_header(iot_buffer, FUNCTION, PWM_GET_RESPONSE, PWM_INFORMATION, sizeof(PWM_Information), pPacketInfo);

            LedData_out->RedLevel = mt7681_HardwareStatus.RedLevel;
            LedData_out->GreenLevel = mt7681_HardwareStatus.GreenLevel;
            LedData_out->BlueLevel = mt7681_HardwareStatus.BlueLevel;
            payload_len = (uint16)(CP_HDR_LEN+CP_DATA_HDR_LEN+sizeof(PWM_Information));
            break;

        case UART_GET_REQUEST:
            return 0;
            break;
    }

    if ((Need_resp == TRUE) && (payload_len <= sizeof(iot_buffer))) {
#if CFG_SUPPORT_TCPIP
        uip_send(iot_buffer, payload_len);
#else
        iot_udp_pkt_send(src_addr, dest_addr, TCPUDP_SRC_PORT, TCPUDP_DST_PORT, iot_buffer, payload_len);
#endif /*CFG_SUPPORT_TCPIP*/
    }
    return 0;
}


int32 iot_app_proc_mgmt_pkt(
    DataHeader* Dataheader,
    uint16 ManagementType,
    IoTPacketInfo  *pPacketInfo)
{

    uint8 iot_buffer[IOT_BUFFER_LEN]={0};
    puchar Data;
    uint8 * Data_out;
    uint16 payload_len =0;
    bool Need_resp = TRUE;
    ClientCapability *ClientCapability_out;
    Status *Status_out;
    puchar PswdData;

    Data = (puchar)(Dataheader) + CP_DATA_HDR_LEN;
    Data_out = (uint8 *)(iot_buffer + CP_HDR_LEN + CP_DATA_HDR_LEN);

    ClientCapability_out = (ClientCapability *) Data_out;
    Status_out = (Status *) Data_out;
    PswdData = (uint8 *)Data;

    if (ManagementType != QUERY_CAPAB_REQUEST &&
        (memcmp(&pPacketInfo->SessionID, IoTpAd.ComCfg.CmdPWD, sizeof(IoTpAd.ComCfg.CmdPWD)))) {
        return 0;
    }

    switch (ManagementType) {
        case QUERY_CAPAB_REQUEST:
            IoT_build_app_response_header(iot_buffer, MANAGEMENT, QUERY_CAPAB_RESPONSE,
                                          CLIENT_CAPABILITY, sizeof(ClientCapability), pPacketInfo);

            ClientCapability_out->VendorNameLen = sizeof(IoTpAd.UsrCfg.VendorName);
            memcpy(ClientCapability_out->VendorName,  IoTpAd.UsrCfg.VendorName,  ClientCapability_out->VendorNameLen);

            ClientCapability_out->ProductTypeLen = sizeof(IoTpAd.UsrCfg.ProductType);
            memcpy(ClientCapability_out->ProductType, IoTpAd.UsrCfg.ProductType, ClientCapability_out->ProductTypeLen);

            ClientCapability_out->ProductNameLen = sizeof(IoTpAd.UsrCfg.ProductName);
            memcpy(ClientCapability_out->ProductName, IoTpAd.UsrCfg.ProductName, ClientCapability_out->ProductNameLen);
            payload_len = (uint16)(CP_HDR_LEN+CP_DATA_HDR_LEN+sizeof(ClientCapability));
            break;

        case CONTROL_CLIENT_OFFLINE_REQUEST:
            IoT_build_app_response_header(iot_buffer, MANAGEMENT, CONTROL_CLIENT_OFFLINE_RESPONSE, STATUS, sizeof(Status), pPacketInfo);
            Status_out->StatusCode = 0;
            payload_len = (uint16)(CP_HDR_LEN+CP_DATA_HDR_LEN+sizeof(Status));
#if CFG_SUPPORT_TCPIP
            uip_send(iot_buffer, payload_len);
#else
            iot_udp_pkt_send(src_addr, dest_addr, TCPUDP_SRC_PORT, TCPUDP_DST_PORT, iot_buffer, payload_len);
#endif /*CFG_SUPPORT_TCPIP*/

#ifdef CONFIG_STATION
            /* reset sta config in the flash, and 7681 shall go into SMNT state again*/
            reset_sta_cfg();
            iot_linkdown(0);
#endif
            Need_resp = FALSE;
            break;

        case CONTROL_PASSWORD_SET_REQUEST:
            Status_out->StatusCode = 0;
            memcpy(IoT_CmdPWD_Recv, PswdData, PASSWORD_MAX_LEN);
            IoT_build_app_response_header(iot_buffer, MANAGEMENT, CONTROL_PASSWORD_SET_RESPONSE, STATUS, sizeof(Status), pPacketInfo);
            payload_len = (uint16)(CP_HDR_LEN+CP_DATA_HDR_LEN+sizeof(Status));
            break;

        case CONTROL_PASSWORD_SET_CONFIRM:
            memcpy(IoTpAd.ComCfg.CmdPWD, IoT_CmdPWD_Recv, PASSWORD_MAX_LEN);

            /*store the Cmd Password*/
            spi_flash_write( FLASH_COM_CFG_BASE+FLASH_COM_CFG_CMD_PWD,
                             IoTpAd.ComCfg.CmdPWD,
                             sizeof(IoTpAd.ComCfg.CmdPWD));
            Need_resp = FALSE;
            break;
    }
    if (Need_resp == TRUE) {
#if CFG_SUPPORT_TCPIP
        uip_send(iot_buffer, payload_len);
#else /*CFG_SUPPORT_TCPIP*/
        iot_udp_pkt_send(src_addr, dest_addr, TCPUDP_SRC_PORT, TCPUDP_DST_PORT, iot_buffer, payload_len);
#endif
    }

    return 0;
}


int32 iot_app_proc_pkt(
    puchar packet ,
    uint16 rawpacketlength
)
{
    DataHeader * Dataheader;
    IoTPacketInfo  PacketInfo;
    IoTCtrlProtocolHeader * ProtocolHeader;
    uint8 Subtype;

    ProtocolHeader = (IoTCtrlProtocolHeader *)packet;
    Dataheader = (DataHeader *)(packet+CP_HDR_LEN);
    appLen = rawpacketlength;
    if (rawpacketlength <= 4)
        return -1;

    if (ProtocolHeader->Magic != IOT_MAGIC_NUMBER)
        return -2;

    if (MAC_ADDR_EQUAL(gCurrentAddress,ProtocolHeader->SendMAC)) {
        return -3;
    }

    if (!MAC_ADDR_EQUAL(gCurrentAddress, ProtocolHeader->ReceiveMAC) &&
        !MAC_ADDR_EQUAL(BCAST_ADDR, ProtocolHeader->ReceiveMAC)) {
        return -4;
    }
    PacketInfo.SessionID = ProtocolHeader->SessionID;
    PacketInfo.Sequence = ProtocolHeader->Sequence;

    COPY_MAC_ADDR(PacketInfo.ReceiveMAC, ProtocolHeader->ReceiveMAC);
    COPY_MAC_ADDR(PacketInfo.SendMAC, ProtocolHeader->SendMAC);

    Subtype = ProtocolHeader->SubHdr.field.SubType;

    if (ProtocolHeader->SubHdr.field.Type == FUNCTION) {
        iot_app_proc_func_pkt(Dataheader, Subtype, &PacketInfo);
    } else if (ProtocolHeader->SubHdr.field.Type == MANAGEMENT) {
        iot_app_proc_mgmt_pkt(Dataheader, Subtype, &PacketInfo);
    }

    preSeq = PacketInfo.Sequence;

    return 0;
}

/**
 * @file sam4e_eth_driver.h
 * @brief SAM4E Ethernet MAC controller
 *
 * @section License
 *
 * Copyright (C) 2010-2018 Oryx Embedded SARL. All rights reserved.
 *
 * This file is part of CycloneTCP Eval.
 *
 * This software is provided in source form for a short-term evaluation only. The
 * evaluation license expires 90 days after the date you first download the software.
 *
 * If you plan to use this software in a commercial product, you are required to
 * purchase a commercial license from Oryx Embedded SARL.
 *
 * After the 90-day evaluation period, you agree to either purchase a commercial
 * license or delete all copies of this software. If you wish to extend the
 * evaluation period, you must contact sales@oryx-embedded.com.
 *
 * This evaluation software is provided "as is" without warranty of any kind.
 * Technical support is available as an option during the evaluation period.
 *
 * @author Oryx Embedded SARL (www.oryx-embedded.com)
 * @version 1.8.2
 **/

#ifndef _SAM4E_ETH_DRIVER_H
#define _SAM4E_ETH_DRIVER_H

//Number of TX buffers
#ifndef SAM4E_ETH_TX_BUFFER_COUNT
   #define SAM4E_ETH_TX_BUFFER_COUNT 2
#elif (SAM4E_ETH_TX_BUFFER_COUNT < 1)
   #error SAM4E_ETH_TX_BUFFER_COUNT parameter is not valid
#endif

//TX buffer size
#ifndef SAM4E_ETH_TX_BUFFER_SIZE
   #define SAM4E_ETH_TX_BUFFER_SIZE 1536
#elif (SAM4E_ETH_TX_BUFFER_SIZE != 1536)
   #error SAM4E_ETH_TX_BUFFER_SIZE parameter is not valid
#endif

//Number of RX buffers
#ifndef SAM4E_ETH_RX_BUFFER_COUNT
   #define SAM4E_ETH_RX_BUFFER_COUNT 48
#elif (SAM4E_ETH_RX_BUFFER_COUNT < 12)
   #error SAM4E_ETH_RX_BUFFER_COUNT parameter is not valid
#endif

//RX buffer size
#ifndef SAM4E_ETH_RX_BUFFER_SIZE
   #define SAM4E_ETH_RX_BUFFER_SIZE 128
#elif (SAM4E_ETH_RX_BUFFER_SIZE != 128)
   #error SAM4E_ETH_RX_BUFFER_SIZE parameter is not valid
#endif

//Interrupt priority grouping
#ifndef SAM4E_ETH_IRQ_PRIORITY_GROUPING
   #define SAM4E_ETH_IRQ_PRIORITY_GROUPING 3
#elif (SAM4E_ETH_IRQ_PRIORITY_GROUPING < 0)
   #error SAM4E_ETH_IRQ_PRIORITY_GROUPING parameter is not valid
#endif

//Ethernet interrupt group priority
#ifndef SAM4E_ETH_IRQ_GROUP_PRIORITY
   #define SAM4E_ETH_IRQ_GROUP_PRIORITY 12
#elif (SAM4E_ETH_IRQ_GROUP_PRIORITY < 0)
   #error SAM4E_ETH_IRQ_GROUP_PRIORITY parameter is not valid
#endif

//Ethernet interrupt subpriority
#ifndef SAM4E_ETH_IRQ_SUB_PRIORITY
   #define SAM4E_ETH_IRQ_SUB_PRIORITY 0
#elif (SAM4E_ETH_IRQ_SUB_PRIORITY < 0)
   #error SAM4E_ETH_IRQ_SUB_PRIORITY parameter is not valid
#endif

//Legacy definitions
#ifndef PIO_PD6A_GRX1
   #define PIO_PD6A_GRX1 PIO_PD6A_GRX0
#endif

//MII signals
#define GMAC_MII_MASK (PIO_PD16A_GTX3 | \
   PIO_PD15A_GTX2 | PIO_PD14A_GRXCK | PIO_PD13A_GCOL | PIO_PD12A_GRX3 | \
   PIO_PD11A_GRX2 | PIO_PD10A_GCRS | PIO_PD9A_GMDIO | PIO_PD8A_GMDC | \
   PIO_PD7A_GRXER | PIO_PD6A_GRX1 | PIO_PD5A_GRX0 | PIO_PD4A_GRXDV | \
   PIO_PD3A_GTX1 | PIO_PD2A_GTX0 | PIO_PD1A_GTXEN | PIO_PD0A_GTXCK)

//TX buffer descriptor flags
#define GMAC_TX_USED           0x80000000
#define GMAC_TX_WRAP           0x40000000
#define GMAC_TX_RLE_ERROR      0x20000000
#define GMAC_TX_UNDERRUN_ERROR 0x10000000
#define GMAC_TX_AHB_ERROR      0x08000000
#define GMAC_TX_LATE_COL_ERROR 0x04000000
#define GMAC_TX_CHECKSUM_ERROR 0x00700000
#define GMAC_TX_NO_CRC         0x00010000
#define GMAC_TX_LAST           0x00008000
#define GMAC_TX_LENGTH         0x00003FFF

//RX buffer descriptor flags
#define GMAC_RX_ADDRESS        0xFFFFFFFC
#define GMAC_RX_WRAP           0x00000002
#define GMAC_RX_OWNERSHIP      0x00000001
#define GMAC_RX_BROADCAST      0x80000000
#define GMAC_RX_MULTICAST_HASH 0x40000000
#define GMAC_RX_UNICAST_HASH   0x20000000
#define GMAC_RX_SAR            0x08000000
#define GMAC_RX_SAR_MASK       0x06000000
#define GMAC_RX_TYPE_ID        0x01000000
#define GMAC_RX_SNAP           0x01000000
#define GMAC_RX_TYPE_ID_MASK   0x00C00000
#define GMAC_RX_CHECKSUM_VALID 0x00C00000
#define GMAC_RX_VLAN_TAG       0x00200000
#define GMAC_RX_PRIORITY_TAG   0x00100000
#define GMAC_RX_VLAN_PRIORITY  0x000E0000
#define GMAC_RX_CFI            0x00010000
#define GMAC_RX_EOF            0x00008000
#define GMAC_RX_SOF            0x00004000
#define GMAC_RX_LENGTH_MSB     0x00002000
#define GMAC_RX_BAD_FCS        0x00002000
#define GMAC_RX_LENGTH         0x00001FFF

//C++ guard
#ifdef __cplusplus
   extern "C" {
#endif


/**
 * @brief Transmit buffer descriptor
 **/

typedef struct
{
   uint32_t address;
   uint32_t status;
} Sam4eTxBufferDesc;


/**
 * @brief Receive buffer descriptor
 **/

typedef struct
{
   uint32_t address;
   uint32_t status;
} Sam4eRxBufferDesc;


//SAM4E Ethernet MAC driver
extern const NicDriver sam4eEthDriver;

//SAM4E Ethernet MAC related functions
error_t sam4eEthInit(NetInterface *interface);
void sam4eEthInitGpio(NetInterface *interface);
void sam4eEthInitBufferDesc(NetInterface *interface);

void sam4eEthTick(NetInterface *interface);

void sam4eEthEnableIrq(NetInterface *interface);
void sam4eEthDisableIrq(NetInterface *interface);
void sam4eEthEventHandler(NetInterface *interface);

error_t sam4eEthSendPacket(NetInterface *interface,
   const NetBuffer *buffer, size_t offset);

error_t sam4eEthReceivePacket(NetInterface *interface);

error_t sam4eEthSetMulticastFilter(NetInterface *interface);
error_t sam4eEthUpdateMacConfig(NetInterface *interface);

void sam4eEthWritePhyReg(uint8_t phyAddr, uint8_t regAddr, uint16_t data);
uint16_t sam4eEthReadPhyReg(uint8_t phyAddr, uint8_t regAddr);

//C++ guard
#ifdef __cplusplus
   }
#endif

#endif

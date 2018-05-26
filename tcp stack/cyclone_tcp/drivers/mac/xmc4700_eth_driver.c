/**
 * @file xmc4700_eth_driver.c
 * @brief Infineon XMC4700 Ethernet MAC controller
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

//Switch to the appropriate trace level
#define TRACE_LEVEL NIC_TRACE_LEVEL

//Dependencies
#include "xmc4700.h"
#include "core/net.h"
#include "drivers/mac/xmc4700_eth_driver.h"
#include "debug.h"

//Underlying network interface
static NetInterface *nicDriverInterface;

//IAR EWARM compiler?
#if defined(__ICCARM__)

//Transmit buffer
#pragma data_alignment = 4
#pragma location = "ETH_RAM"
static uint8_t txBuffer[XMC4700_ETH_TX_BUFFER_COUNT][XMC4700_ETH_TX_BUFFER_SIZE];
//Receive buffer
#pragma data_alignment = 4
#pragma location = "ETH_RAM"
static uint8_t rxBuffer[XMC4700_ETH_RX_BUFFER_COUNT][XMC4700_ETH_RX_BUFFER_SIZE];
//Transmit DMA descriptors
#pragma data_alignment = 4
#pragma location = "ETH_RAM"
static Xmc4700TxDmaDesc txDmaDesc[XMC4700_ETH_TX_BUFFER_COUNT];
//Receive DMA descriptors
#pragma data_alignment = 4
#pragma location = "ETH_RAM"
static Xmc4700RxDmaDesc rxDmaDesc[XMC4700_ETH_RX_BUFFER_COUNT];

//Keil MDK-ARM or GCC compiler?
#else

//Transmit buffer
static uint8_t txBuffer[XMC4700_ETH_TX_BUFFER_COUNT][XMC4700_ETH_TX_BUFFER_SIZE]
   __attribute__((aligned(4), __section__("ETH_RAM")));
//Receive buffer
static uint8_t rxBuffer[XMC4700_ETH_RX_BUFFER_COUNT][XMC4700_ETH_RX_BUFFER_SIZE]
   __attribute__((aligned(4), __section__("ETH_RAM")));
//Transmit DMA descriptors
static Xmc4700TxDmaDesc txDmaDesc[XMC4700_ETH_TX_BUFFER_COUNT]
   __attribute__((aligned(4), __section__("ETH_RAM")));
//Receive DMA descriptors
static Xmc4700RxDmaDesc rxDmaDesc[XMC4700_ETH_RX_BUFFER_COUNT]
   __attribute__((aligned(4), __section__("ETH_RAM")));

#endif

//Pointer to the current TX DMA descriptor
static Xmc4700TxDmaDesc *txCurDmaDesc;
//Pointer to the current RX DMA descriptor
static Xmc4700RxDmaDesc *rxCurDmaDesc;


/**
 * @brief XMC4700 Ethernet MAC driver
 **/

const NicDriver xmc4700EthDriver =
{
   NIC_TYPE_ETHERNET,
   ETH_MTU,
   xmc4700EthInit,
   xmc4700EthTick,
   xmc4700EthEnableIrq,
   xmc4700EthDisableIrq,
   xmc4700EthEventHandler,
   xmc4700EthSendPacket,
   xmc4700EthSetMulticastFilter,
   xmc4700EthUpdateMacConfig,
   xmc4700EthWritePhyReg,
   xmc4700EthReadPhyReg,
   TRUE,
   TRUE,
   TRUE,
   FALSE
};


/**
 * @brief XMC4700 Ethernet MAC initialization
 * @param[in] interface Underlying network interface
 * @return Error code
 **/

error_t xmc4700EthInit(NetInterface *interface)
{
   error_t error;

   //Debug message
   TRACE_INFO("Initializing XMC4700 Ethernet MAC...\r\n");

   //Save underlying network interface
   nicDriverInterface = interface;

   //Disable parity error trap
   SCU_PARITY->PETE = 0;
   //Disable unaligned access trap
   PPB->CCR &= ~PPB_CCR_UNALIGN_TRP_Msk;

   //Enable ETH0 peripheral clock
   SCU_CLK->CLKSET = SCU_CLK_CLKSET_ETH0CEN_Msk;

   //GPIO configuration
   xmc4700EthInitGpio(interface);

   //Reset ETH0 peripheral
   SCU_RESET->PRSET2 = SCU_RESET_PRSET2_ETH0RS_Msk;
   SCU_RESET->PRCLR2 = SCU_RESET_PRCLR2_ETH0RS_Msk;

   //Reset DMA controller
   ETH0->BUS_MODE |= ETH_BUS_MODE_SWR_Msk;
   //Wait for the reset to complete
   while(ETH0->BUS_MODE & ETH_BUS_MODE_SWR_Msk);

   //Adjust MDC clock range depending on ETH clock frequency
   ETH0->GMII_ADDRESS = ETH_GMII_ADDRESS_CR_DIV62;

   //PHY transceiver initialization
   error = interface->phyDriver->init(interface);
   //Failed to initialize PHY transceiver?
   if(error)
      return error;

   //Use default MAC configuration
   ETH0->MAC_CONFIGURATION = ETH_MAC_CONFIGURATION_RESERVED15_Msk |
      ETH_MAC_CONFIGURATION_DO_Msk;

   //Set the MAC address
   ETH0->MAC_ADDRESS0_LOW = interface->macAddr.w[0] | (interface->macAddr.w[1] << 16);
   ETH0->MAC_ADDRESS0_HIGH = interface->macAddr.w[2];

   //Initialize hash table
   ETH0->HASH_TABLE_LOW = 0;
   ETH0->HASH_TABLE_HIGH = 0;

   //Configure the receive filter
   ETH0->MAC_FRAME_FILTER = ETH_MAC_FRAME_FILTER_HPF_Msk | ETH_MAC_FRAME_FILTER_HMC_Msk;
   //Disable flow control
   ETH0->FLOW_CONTROL = 0;
   //Enable store and forward mode
   ETH0->OPERATION_MODE = ETH_OPERATION_MODE_RSF_Msk | ETH_OPERATION_MODE_TSF_Msk;

   //Configure DMA bus mode
   ETH0->BUS_MODE = ETH_BUS_MODE_AAL_Msk | ETH_BUS_MODE_USP_Msk |
      ETH_BUS_MODE_RPBL_1 | ETH_BUS_MODE_PR_1_1 | ETH_BUS_MODE_PBL_1;

   //Initialize DMA descriptor lists
   xmc4700EthInitDmaDesc(interface);

   //Prevent interrupts from being generated when statistic counters reach
   //half their maximum value
   ETH0->MMC_TRANSMIT_INTERRUPT_MASK = 0xFFFFFFFF;
   ETH0->MMC_RECEIVE_INTERRUPT_MASK = 0xFFFFFFFF;
   ETH0->MMC_IPC_RECEIVE_INTERRUPT_MASK = 0xFFFFFFFF;

   //Disable MAC interrupts
   ETH0->INTERRUPT_MASK = ETH_INTERRUPT_MASK_TSIM_Msk | ETH_INTERRUPT_MASK_PMTIM_Msk;

   //Enable the desired DMA interrupts
   ETH0->INTERRUPT_ENABLE = ETH_INTERRUPT_ENABLE_NIE_Msk |
      ETH_INTERRUPT_ENABLE_RIE_Msk | ETH_INTERRUPT_ENABLE_TIE_Msk;

   //Set priority grouping (6 bits for pre-emption priority, no bits for subpriority)
   NVIC_SetPriorityGrouping(XMC4700_ETH_IRQ_PRIORITY_GROUPING);

   //Configure Ethernet interrupt priority
   NVIC_SetPriority(ETH0_0_IRQn, NVIC_EncodePriority(XMC4700_ETH_IRQ_PRIORITY_GROUPING,
      XMC4700_ETH_IRQ_GROUP_PRIORITY, XMC4700_ETH_IRQ_SUB_PRIORITY));

   //Enable MAC transmission and reception
   ETH0->MAC_CONFIGURATION |= ETH_MAC_CONFIGURATION_TE_Msk | ETH_MAC_CONFIGURATION_RE_Msk;
   //Enable DMA transmission and reception
   ETH0->OPERATION_MODE |= ETH_OPERATION_MODE_ST_Msk | ETH_OPERATION_MODE_SR_Msk;

   //Accept any packets from the upper layer
   osSetEvent(&interface->nicTxEvent);

   //Successful initialization
   return NO_ERROR;
}


//XMC4700 Relax Kit?
#if defined(USE_XMC4700_RELAX_KIT)

/**
 * @brief GPIO configuration
 * @param[in] interface Underlying network interface
 **/

void xmc4700EthInitGpio(NetInterface *interface)
{
   uint32_t temp;

   //Configure ETH0.MDIO (P2.0), ETH0.RXD0A (P2.2) and ETH0.RXD1A (P2.3)
   temp = PORT2->IOCR0;
   temp &= ~(PORT2_IOCR0_PC0_Msk | PORT2_IOCR0_PC2_Msk | PORT2_IOCR0_PC3_Msk);
   temp |= (0UL << PORT2_IOCR0_PC0_Pos) | (0UL << PORT2_IOCR0_PC2_Pos) | (0UL << PORT2_IOCR0_PC3_Pos);
   PORT2->IOCR0 = temp;

   //Configure ETH0.RXERA (P2.4), ETH0.TX_EN (P2.5) and ETH0.MDC (P2.7)
   temp = PORT2->IOCR4;
   temp &= ~(PORT2_IOCR4_PC4_Msk | PORT2_IOCR4_PC5_Msk | PORT2_IOCR4_PC7_Msk);
   temp |= (0UL << PORT2_IOCR4_PC4_Pos) | (17UL << PORT2_IOCR4_PC5_Pos) | (17UL << PORT2_IOCR4_PC7_Pos);
   PORT2->IOCR4 = temp;

   //Configure ETH0.TXD0 (P2.8) and ETH0.TXD1 (P2.9)
   temp = PORT2->IOCR8;
   temp &= ~(PORT2_IOCR8_PC8_Msk | PORT2_IOCR8_PC9_Msk);
   temp |= (17UL << PORT2_IOCR8_PC8_Pos) | (17UL << PORT2_IOCR8_PC9_Pos);
   PORT2->IOCR8 = temp;

   //Configure ETH0.CLK_RMIIC (P15.8) and ETH0.CRS_DVC (P15.9)
   temp = PORT15->IOCR8;
   temp &= ~(PORT15_IOCR8_PC8_Msk | PORT15_IOCR8_PC9_Msk);
   temp |= (0UL << PORT15_IOCR8_PC8_Pos) | (0UL << PORT15_IOCR8_PC9_Pos);
   PORT15->IOCR8 = temp;

   //Assign ETH_MDIO (P2.0) to HW0
   temp = PORT2->HWSEL & ~PORT2_HWSEL_HW0_Msk;
   PORT2->HWSEL = temp | (1UL << PORT2_HWSEL_HW0_Pos);

   //Select output driver strength for ETH0.TX_EN (P2.5)
   temp = PORT2->PDR0;
   temp &= ~PORT2_PDR0_PD5_Msk;
   temp |= (0UL << PORT2_PDR0_PD5_Pos);
   PORT2->PDR0 = temp;

   //Select output driver strength for ETH0.TXD0 (P2.8) and ETH0.TXD1 (P2.9)
   temp = PORT2->PDR1;
   temp &= ~(PORT2_PDR1_PD8_Msk | PORT2_PDR1_PD9_Msk);
   temp |= (0UL << PORT2_PDR1_PD8_Pos) | (0UL << PORT2_PDR1_PD9_Pos);
   PORT2->PDR1 = temp;

   //Use ETH0.CLK_RMIIC (P15.8) and ETH0.CRS_DVC (P15.9) as digital inputs
   PORT15->PDISC &= ~(PORT15_PDISC_PDIS8_Msk | PORT15_PDISC_PDIS9_Msk);

   //Select RMII operation mode
   ETH0_CON->CON = ETH_CON_INFSEL_Msk | ETH_CON_MDIO_B | ETH_CON_RXER_A |
      ETH_CON_CRS_DV_C | ETH_CON_CLK_RMII_C | ETH_CON_RXD1_A | ETH_CON_RXD0_A;
}

#endif


/**
 * @brief Initialize DMA descriptor lists
 * @param[in] interface Underlying network interface
 **/

void xmc4700EthInitDmaDesc(NetInterface *interface)
{
   uint_t i;

   //Initialize TX DMA descriptor list
   for(i = 0; i < XMC4700_ETH_TX_BUFFER_COUNT; i++)
   {
      //Use chain structure rather than ring structure
      txDmaDesc[i].tdes0 = ETH_TDES0_IC | ETH_TDES0_TCH;
      //Initialize transmit buffer size
      txDmaDesc[i].tdes1 = 0;
      //Transmit buffer address
      txDmaDesc[i].tdes2 = (uint32_t) txBuffer[i];
      //Next descriptor address
      txDmaDesc[i].tdes3 = (uint32_t) &txDmaDesc[i + 1];
   }

   //The last descriptor is chained to the first entry
   txDmaDesc[i - 1].tdes3 = (uint32_t) &txDmaDesc[0];
   //Point to the very first descriptor
   txCurDmaDesc = &txDmaDesc[0];

   //Initialize RX DMA descriptor list
   for(i = 0; i < XMC4700_ETH_RX_BUFFER_COUNT; i++)
   {
      //The descriptor is initially owned by the DMA
      rxDmaDesc[i].rdes0 = ETH_RDES0_OWN;
      //Use chain structure rather than ring structure
      rxDmaDesc[i].rdes1 = ETH_RDES1_RCH | (XMC4700_ETH_RX_BUFFER_SIZE & ETH_RDES1_RBS1);
      //Receive buffer address
      rxDmaDesc[i].rdes2 = (uint32_t) rxBuffer[i];
      //Next descriptor address
      rxDmaDesc[i].rdes3 = (uint32_t) &rxDmaDesc[i + 1];
   }

   //The last descriptor is chained to the first entry
   rxDmaDesc[i - 1].rdes3 = (uint32_t) &rxDmaDesc[0];
   //Point to the very first descriptor
   rxCurDmaDesc = &rxDmaDesc[0];

   //Start location of the TX descriptor list
   ETH0->TRANSMIT_DESCRIPTOR_LIST_ADDRESS = (uint32_t) txDmaDesc;
   //Start location of the RX descriptor list
   ETH0->RECEIVE_DESCRIPTOR_LIST_ADDRESS = (uint32_t) rxDmaDesc;
}


/**
 * @brief XMC4700 Ethernet MAC timer handler
 *
 * This routine is periodically called by the TCP/IP stack to
 * handle periodic operations such as polling the link state
 *
 * @param[in] interface Underlying network interface
 **/

void xmc4700EthTick(NetInterface *interface)
{
   //Handle periodic operations
   interface->phyDriver->tick(interface);
}


/**
 * @brief Enable interrupts
 * @param[in] interface Underlying network interface
 **/

void xmc4700EthEnableIrq(NetInterface *interface)
{
   //Enable Ethernet MAC interrupts
   NVIC_EnableIRQ(ETH0_0_IRQn);
   //Enable Ethernet PHY interrupts
   interface->phyDriver->enableIrq(interface);
}


/**
 * @brief Disable interrupts
 * @param[in] interface Underlying network interface
 **/

void xmc4700EthDisableIrq(NetInterface *interface)
{
   //Disable Ethernet MAC interrupts
   NVIC_DisableIRQ(ETH0_0_IRQn);
   //Disable Ethernet PHY interrupts
   interface->phyDriver->disableIrq(interface);
}


/**
 * @brief XMC4700 Ethernet MAC interrupt service routine
 **/

void ETH0_0_IRQHandler(void)
{
   bool_t flag;
   uint32_t status;

   //Enter interrupt service routine
   osEnterIsr();

   //This flag will be set if a higher priority task must be woken
   flag = FALSE;

   //Read DMA status register
   status = ETH0->STATUS;

   //A packet has been transmitted?
   if(status & ETH_STATUS_TI_Msk)
   {
      //Clear TI interrupt flag
      ETH0->STATUS = ETH_STATUS_TI_Msk;

      //Check whether the TX buffer is available for writing
      if(!(txCurDmaDesc->tdes0 & ETH_TDES0_OWN))
      {
         //Notify the TCP/IP stack that the transmitter is ready to send
         flag |= osSetEventFromIsr(&nicDriverInterface->nicTxEvent);
      }
   }

   //A packet has been received?
   if(status & ETH_STATUS_RI_Msk)
   {
      //Disable RIE interrupt
      ETH0->INTERRUPT_ENABLE &= ~ETH_INTERRUPT_ENABLE_RIE_Msk;

      //Set event flag
      nicDriverInterface->nicEvent = TRUE;
      //Notify the TCP/IP stack of the event
      flag |= osSetEventFromIsr(&netEvent);
   }

   //Clear NIS interrupt flag
   ETH0->STATUS = ETH_STATUS_NIS_Msk;

   //Leave interrupt service routine
   osExitIsr(flag);
}


/**
 * @brief XMC4700 Ethernet MAC event handler
 * @param[in] interface Underlying network interface
 **/

void xmc4700EthEventHandler(NetInterface *interface)
{
   error_t error;

   //Packet received?
   if(ETH0->STATUS & ETH_STATUS_RI_Msk)
   {
      //Clear interrupt flag
      ETH0->STATUS = ETH_STATUS_RI_Msk;

      //Process all pending packets
      do
      {
         //Read incoming packet
         error = xmc4700EthReceivePacket(interface);

         //No more data in the receive buffer?
      } while(error != ERROR_BUFFER_EMPTY);
   }

   //Re-enable DMA interrupts
   ETH0->INTERRUPT_ENABLE |= ETH_INTERRUPT_ENABLE_NIE_Msk |
      ETH_INTERRUPT_ENABLE_RIE_Msk | ETH_INTERRUPT_ENABLE_TIE_Msk;
}


/**
 * @brief Send a packet
 * @param[in] interface Underlying network interface
 * @param[in] buffer Multi-part buffer containing the data to send
 * @param[in] offset Offset to the first data byte
 * @return Error code
 **/

error_t xmc4700EthSendPacket(NetInterface *interface,
   const NetBuffer *buffer, size_t offset)
{
   size_t length;

   //Retrieve the length of the packet
   length = netBufferGetLength(buffer) - offset;

   //Check the frame length
   if(length > XMC4700_ETH_TX_BUFFER_SIZE)
   {
      //The transmitter can accept another packet
      osSetEvent(&interface->nicTxEvent);
      //Report an error
      return ERROR_INVALID_LENGTH;
   }

   //Make sure the current buffer is available for writing
   if(txCurDmaDesc->tdes0 & ETH_TDES0_OWN)
      return ERROR_FAILURE;

   //Copy user data to the transmit buffer
   netBufferRead((uint8_t *) txCurDmaDesc->tdes2, buffer, offset, length);

   //Write the number of bytes to send
   txCurDmaDesc->tdes1 = length & ETH_TDES1_TBS1;
   //Set LS and FS flags as the data fits in a single buffer
   txCurDmaDesc->tdes0 |= ETH_TDES0_LS | ETH_TDES0_FS;
   //Give the ownership of the descriptor to the DMA
   txCurDmaDesc->tdes0 |= ETH_TDES0_OWN;

   //Clear TU flag to resume processing
   ETH0->STATUS = ETH_STATUS_TU_Msk;
   //Instruct the DMA to poll the transmit descriptor list
   ETH0->TRANSMIT_POLL_DEMAND = 0;

   //Point to the next descriptor in the list
   txCurDmaDesc = (Xmc4700TxDmaDesc *) txCurDmaDesc->tdes3;

   //Check whether the next buffer is available for writing
   if(!(txCurDmaDesc->tdes0 & ETH_TDES0_OWN))
   {
      //The transmitter can accept another packet
      osSetEvent(&interface->nicTxEvent);
   }

   //Data successfully written
   return NO_ERROR;
}


/**
 * @brief Receive a packet
 * @param[in] interface Underlying network interface
 * @return Error code
 **/

error_t xmc4700EthReceivePacket(NetInterface *interface)
{
   error_t error;
   size_t n;

   //The current buffer is available for reading?
   if(!(rxCurDmaDesc->rdes0 & ETH_RDES0_OWN))
   {
      //FS and LS flags should be set
      if((rxCurDmaDesc->rdes0 & ETH_RDES0_FS) && (rxCurDmaDesc->rdes0 & ETH_RDES0_LS))
      {
         //Make sure no error occurred
         if(!(rxCurDmaDesc->rdes0 & ETH_RDES0_ES))
         {
            //Retrieve the length of the frame
            n = (rxCurDmaDesc->rdes0 & ETH_RDES0_FL) >> 16;
            //Limit the number of data to read
            n = MIN(n, XMC4700_ETH_RX_BUFFER_SIZE);

            //Pass the packet to the upper layer
            nicProcessPacket(interface, (uint8_t *) rxCurDmaDesc->rdes2, n);

            //Valid packet received
            error = NO_ERROR;
         }
         else
         {
            //The received packet contains an error
            error = ERROR_INVALID_PACKET;
         }
      }
      else
      {
         //The packet is not valid
         error = ERROR_INVALID_PACKET;
      }

      //Give the ownership of the descriptor back to the DMA
      rxCurDmaDesc->rdes0 = ETH_RDES0_OWN;
      //Point to the next descriptor in the list
      rxCurDmaDesc = (Xmc4700RxDmaDesc *) rxCurDmaDesc->rdes3;
   }
   else
   {
      //No more data in the receive buffer
      error = ERROR_BUFFER_EMPTY;
   }

   //Clear RU flag to resume processing
   ETH0->STATUS = ETH_STATUS_RU_Msk;
   //Instruct the DMA to poll the receive descriptor list
   ETH0->RECEIVE_POLL_DEMAND = 0;

   //Return status code
   return error;
}


/**
 * @brief Configure multicast MAC address filtering
 * @param[in] interface Underlying network interface
 * @return Error code
 **/

error_t xmc4700EthSetMulticastFilter(NetInterface *interface)
{
   uint_t i;
   uint_t k;
   uint32_t crc;
   uint32_t hashTable[2];
   MacFilterEntry *entry;

   //Debug message
   TRACE_DEBUG("Updating XMC4700 hash table...\r\n");

   //Clear hash table
   hashTable[0] = 0;
   hashTable[1] = 0;

   //The MAC filter table contains the multicast MAC addresses
   //to accept when receiving an Ethernet frame
   for(i = 0; i < MAC_MULTICAST_FILTER_SIZE; i++)
   {
      //Point to the current entry
      entry = &interface->macMulticastFilter[i];

      //Valid entry?
      if(entry->refCount > 0)
      {
         //Compute CRC over the current MAC address
         crc = xmc4700EthCalcCrc(&entry->addr, sizeof(MacAddr));

         //The upper 6 bits in the CRC register are used to index the
         //contents of the hash table
         k = (crc >> 26) & 0x3F;

         //Update hash table contents
         hashTable[k / 32] |= (1 << (k % 32));
      }
   }

   //Write the hash table
   ETH0->HASH_TABLE_LOW = hashTable[0];
   ETH0->HASH_TABLE_HIGH = hashTable[1];

   //Debug message
   TRACE_DEBUG("  HTL = %08" PRIX32 "\r\n", ETH0->HASH_TABLE_LOW);
   TRACE_DEBUG("  HTH = %08" PRIX32 "\r\n", ETH0->HASH_TABLE_HIGH);

   //Successful processing
   return NO_ERROR;
}


/**
 * @brief Adjust MAC configuration parameters for proper operation
 * @param[in] interface Underlying network interface
 * @return Error code
 **/

error_t xmc4700EthUpdateMacConfig(NetInterface *interface)
{
   uint32_t config;

   //Read current MAC configuration
   config = ETH0->MAC_CONFIGURATION;

   //10BASE-T or 100BASE-TX operation mode?
   if(interface->linkSpeed == NIC_LINK_SPEED_100MBPS)
      config |= ETH_MAC_CONFIGURATION_FES_Msk;
   else
      config &= ~ETH_MAC_CONFIGURATION_FES_Msk;

   //Half-duplex or full-duplex mode?
   if(interface->duplexMode == NIC_FULL_DUPLEX_MODE)
      config |= ETH_MAC_CONFIGURATION_DM_Msk;
   else
      config &= ~ETH_MAC_CONFIGURATION_DM_Msk;

   //Update MAC configuration register
   ETH0->MAC_CONFIGURATION = config;

   //Successful processing
   return NO_ERROR;
}


/**
 * @brief Write PHY register
 * @param[in] phyAddr PHY address
 * @param[in] regAddr Register address
 * @param[in] data Register value
 **/

void xmc4700EthWritePhyReg(uint8_t phyAddr, uint8_t regAddr, uint16_t data)
{
   uint32_t value;

   //Take care not to alter MDC clock configuration
   value = ETH0->GMII_ADDRESS & ETH_GMII_ADDRESS_CR_Msk;
   //Set up a write operation
   value |= ETH_GMII_ADDRESS_MW_Msk | ETH_GMII_ADDRESS_MB_Msk;
   //PHY address
   value |= (phyAddr << ETH_GMII_ADDRESS_PA_Pos) & ETH_GMII_ADDRESS_PA_Msk;
   //Register address
   value |= (regAddr << ETH_GMII_ADDRESS_MR_Pos) & ETH_GMII_ADDRESS_MR_Msk;

   //Data to be written in the PHY register
   ETH0->GMII_DATA = data & ETH_GMII_DATA_MD_Msk;

   //Start a write operation
   ETH0->GMII_ADDRESS = value;
   //Wait for the write to complete
   while(ETH0->GMII_ADDRESS & ETH_GMII_ADDRESS_MB_Msk);
}


/**
 * @brief Read PHY register
 * @param[in] phyAddr PHY address
 * @param[in] regAddr Register address
 * @return Register value
 **/

uint16_t xmc4700EthReadPhyReg(uint8_t phyAddr, uint8_t regAddr)
{
   uint32_t value;

   //Take care not to alter MDC clock configuration
   value = ETH0->GMII_ADDRESS & ETH_GMII_ADDRESS_CR_Msk;
   //Set up a read operation
   value |= ETH_GMII_ADDRESS_MB_Msk;
   //PHY address
   value |= (phyAddr << ETH_GMII_ADDRESS_PA_Pos) & ETH_GMII_ADDRESS_PA_Msk;
   //Register address
   value |= (regAddr << ETH_GMII_ADDRESS_MR_Pos) & ETH_GMII_ADDRESS_MR_Msk;

   //Start a read operation
   ETH0->GMII_ADDRESS = value;
   //Wait for the read to complete
   while(ETH0->GMII_ADDRESS & ETH_GMII_ADDRESS_MB_Msk);

   //Return PHY register contents
   return ETH0->GMII_DATA & ETH_GMII_DATA_MD_Msk;
}


/**
 * @brief CRC calculation
 * @param[in] data Pointer to the data over which to calculate the CRC
 * @param[in] length Number of bytes to process
 * @return Resulting CRC value
 **/

uint32_t xmc4700EthCalcCrc(const void *data, size_t length)
{
   uint_t i;
   uint_t j;

   //Point to the data over which to calculate the CRC
   const uint8_t *p = (uint8_t *) data;
   //CRC preset value
   uint32_t crc = 0xFFFFFFFF;

   //Loop through data
   for(i = 0; i < length; i++)
   {
      //The message is processed bit by bit
      for(j = 0; j < 8; j++)
      {
         //Update CRC value
         if(((crc >> 31) ^ (p[i] >> j)) & 0x01)
            crc = (crc << 1) ^ 0x04C11DB7;
         else
            crc = crc << 1;
      }
   }

   //Return CRC value
   return ~crc;
}

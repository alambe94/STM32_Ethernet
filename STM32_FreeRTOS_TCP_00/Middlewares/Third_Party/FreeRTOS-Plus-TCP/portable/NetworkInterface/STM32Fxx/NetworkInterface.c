/*
 * Some constants, hardware definitions and comments taken from ST's HAL driver
 * library, COPYRIGHT(c) 2015 STMicroelectronics.
 */

/*
FreeRTOS+TCP V2.0.11
Copyright (C) 2017 Amazon.com, Inc. or its affiliates.  All Rights Reserved.

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
the Software, and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

 http://aws.amazon.com/freertos
 http://www.FreeRTOS.org
*/

/* Standard includes. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

/* FreeRTOS includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

/* FreeRTOS+TCP includes. */
#include "FreeRTOS_IP.h"
#include "FreeRTOS_Sockets.h"
#include "FreeRTOS_IP_Private.h"
#include "FreeRTOS_DNS.h"
#include "NetworkBufferManagement.h"
#include "NetworkInterface.h"

/* ST includes. */
#include "stm32f4xx_hal.h"

/* Default the size of the stack used by the EMAC deferred handler task to twice
the size of the stack used by the idle task - but allow this to be overridden in
FreeRTOSConfig.h as configMINIMAL_STACK_SIZE is a user definable constant. */
#ifndef configEMAC_TASK_STACK_SIZE
	#define configEMAC_TASK_STACK_SIZE ( 2 * configMINIMAL_STACK_SIZE )
#endif

#ifndef	PHY_LS_HIGH_CHECK_TIME_MS
	/* Check if the LinkSStatus in the PHY is still high after 15 seconds of not
	receiving packets. */
	#define PHY_LS_HIGH_CHECK_TIME_MS	15000
#endif

#ifndef	PHY_LS_LOW_CHECK_TIME_MS
	/* Check if the LinkSStatus in the PHY is still low every second. */
	#define PHY_LS_LOW_CHECK_TIME_MS	1000
#endif

/*
 * A deferred interrupt handler task that processes
 */
static void prvEMACHandlerTask( void *pvParameters );

/*
 * See if there is a new packet and forward it to the IP-task.
 */
static BaseType_t prvNetworkInterfaceInput( void );

/*
 * Check if a given packet should be accepted.
 */
static BaseType_t xMayAcceptPacket( uint8_t *pcBuffer );

#if defined ( __ICCARM__ ) /*!< IAR Compiler */
  #pragma data_alignment=4
#endif
__ALIGN_BEGIN ETH_DMADescTypeDef  DMARxDscrTab[ETH_RXBUFNB] __ALIGN_END;/* Ethernet Rx MA Descriptor */

#if defined ( __ICCARM__ ) /*!< IAR Compiler */
  #pragma data_alignment=4
#endif
__ALIGN_BEGIN ETH_DMADescTypeDef  DMATxDscrTab[ETH_TXBUFNB] __ALIGN_END;/* Ethernet Tx DMA Descriptor */

#if defined ( __ICCARM__ ) /*!< IAR Compiler */
  #pragma data_alignment=4
#endif
__ALIGN_BEGIN uint8_t Rx_Buff[ETH_RXBUFNB][ETH_RX_BUF_SIZE] __ALIGN_END; /* Ethernet Receive Buffer */

#if defined ( __ICCARM__ ) /*!< IAR Compiler */
  #pragma data_alignment=4
#endif
__ALIGN_BEGIN uint8_t Tx_Buff[ETH_TXBUFNB][ETH_TX_BUF_SIZE] __ALIGN_END; /* Ethernet Transmit Buffer */


/* Holds the handle of the task used as a deferred interrupt processor.  The
handle is used so direct notifications can be sent to the task for all EMAC/DMA
related interrupts. */
static TaskHandle_t xEMACTaskHandle = NULL;

void HAL_ETH_RxCpltCallback( ETH_HandleTypeDef *heth )
    {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    /* Wakeup the prvEMACHandlerTask. */
    if (xEMACTaskHandle != NULL)
	{
	vTaskNotifyGiveFromISR(xEMACTaskHandle, &xHigherPriorityTaskWoken);
	portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
	}
    }

extern ETH_HandleTypeDef heth;
BaseType_t xNetworkInterfaceInitialise(void)
    {
    BaseType_t xResult;
    uint32_t regvalue = 0;
    HAL_StatusTypeDef hal_eth_init_status;

  /* Init ETH */

    uint8_t MACAddr[6] ;
    heth.Instance = ETH;
    heth.Init.AutoNegotiation = ETH_AUTONEGOTIATION_ENABLE;
    heth.Init.PhyAddress = 1;
    MACAddr[0] = 0x00;
    MACAddr[1] = 0x80;
    MACAddr[2] = 0xE1;
    MACAddr[3] = 0x00;
    MACAddr[4] = 0x00;
    MACAddr[5] = 0x00;
    heth.Init.MACAddr = &MACAddr[0];
    heth.Init.RxMode = ETH_RXPOLLING_MODE;
    heth.Init.ChecksumMode = ETH_CHECKSUM_BY_HARDWARE;
    heth.Init.MediaInterface = ETH_MEDIA_INTERFACE_RMII;

    hal_eth_init_status = HAL_ETH_Init(&heth);

    if (hal_eth_init_status == HAL_OK)
	{
	xResult = pdPASS;
	FreeRTOS_printf( ( "Link Status is high\n" ) );
	}
    else
	{
	/* For now pdFAIL will be returned. But prvEMACHandlerTask() is running
	 and it will keep on checking the PHY and set ulPHYLinkStatus when necessary. */
	xResult = pdFAIL;
	FreeRTOS_printf( ( "Link Status still low\n" ) );
	}

    /* Initialize Tx Descriptors list: Chain Mode */
    HAL_ETH_DMATxDescListInit(&heth, DMATxDscrTab, &Tx_Buff[0][0], ETH_TXBUFNB);

    /* Initialize Rx Descriptors list: Chain Mode  */
    HAL_ETH_DMARxDescListInit(&heth, DMARxDscrTab, &Rx_Buff[0][0], ETH_RXBUFNB);

    /* Enable MAC and DMA transmission and reception */
    HAL_ETH_Start(&heth);

    /* Read Register Configuration */
    HAL_ETH_ReadPHYRegister(&heth, PHY_ISFR, &regvalue);
    regvalue |= (PHY_ISFR_INT4);

    /* Enable Interrupt on change of link status */
    HAL_ETH_WritePHYRegister(&heth, PHY_ISFR , regvalue );

    /* Read Register Configuration */
    HAL_ETH_ReadPHYRegister(&heth, PHY_ISFR , &regvalue);

    if (xEMACTaskHandle == NULL)
	{

	/* The deferred interrupt handler task is created at the highest
	 possible priority to ensure the interrupt handler can return directly
	 to it.  The task's handle is stored in xEMACTaskHandle so interrupts can
	 notify the task when there is something to process. */
	xTaskCreate(prvEMACHandlerTask,
		    "EMAC",
		    configEMAC_TASK_STACK_SIZE,
		    NULL,
		    configMAX_PRIORITIES - 1,
		    &xEMACTaskHandle);
	} /* if( xEMACTaskHandle == NULL ) */

    return pdPASS;
    }

static void prvEMACHandlerTask(void *pvParameters)
    {
    TimeOut_t xPhyTime;
    TickType_t xPhyRemTime;
    UBaseType_t uxLastMinBufferCount = 0;
    UBaseType_t uxCurrentCount;
    BaseType_t xResult = 0;
    uint32_t xStatus;
    const TickType_t ulMaxBlockTime = pdMS_TO_TICKS(100UL);

    /* Remove compiler warnings about unused parameters. */
    (void) pvParameters;

    vTaskSetTimeOutState(&xPhyTime);
    xPhyRemTime = pdMS_TO_TICKS(PHY_LS_LOW_CHECK_TIME_MS);

    for (;;)
	{
	uxCurrentCount = uxGetMinimumFreeNetworkBuffers();
	if (uxLastMinBufferCount != uxCurrentCount)
	    {
	    /* The logging produced below may be helpful
	     while tuning +TCP: see how many buffers are in use. */
	    uxLastMinBufferCount = uxCurrentCount;
	    FreeRTOS_printf(
		    ( "Network buffers: %lu lowest %lu\n", uxGetNumberOfFreeNetworkBuffers(), uxCurrentCount ));
	    }

	/* No events to process now, wait for the next. */
	ulTaskNotifyTake( pdFALSE, ulMaxBlockTime);

	xResult = prvNetworkInterfaceInput();
	if (xResult > 0)
	    {
	    while (prvNetworkInterfaceInput() > 0)
		{
		}
	    }

	if (xResult > 0)
	    {
	    /* A packet was received. No need to check for the PHY status now,
	     but set a timer to check it later on. */
	    vTaskSetTimeOutState(&xPhyTime);
	    xPhyRemTime = pdMS_TO_TICKS(PHY_LS_HIGH_CHECK_TIME_MS);
	    xResult = 0;
	    }
	}
    }

static BaseType_t prvNetworkInterfaceInput(void)
    {
    NetworkBufferDescriptor_t *pxCurDescriptor;
    NetworkBufferDescriptor_t *pxNewDescriptor = NULL;
    BaseType_t xReceivedLength, xAccepted;
    __IO ETH_DMADescTypeDef *pxDMARxDescriptor;
    xIPStackEvent_t xRxEvent =
	{
	eNetworkRxEvent, NULL
	};
    const TickType_t xDescriptorWaitTime = pdMS_TO_TICKS(250);
    uint8_t *pucBuffer;

    pxDMARxDescriptor = heth.RxDesc;

    if ((pxDMARxDescriptor->Status & ETH_DMARXDESC_OWN) == 0)
	{
	/* Get the Frame Length of the received packet: substruct 4 bytes of the CRC */
	xReceivedLength = ((pxDMARxDescriptor->Status & ETH_DMARXDESC_FL)
		>> ETH_DMARXDESC_FRAMELENGTHSHIFT) - 4;

	pucBuffer = (uint8_t*) pxDMARxDescriptor->Buffer1Addr;

	/* Update the ETHERNET DMA global Rx descriptor with next Rx descriptor */
	/* Chained Mode */
	/* Selects the next DMA Rx descriptor list for next buffer to read */
	heth.RxDesc =
		(ETH_DMADescTypeDef*) pxDMARxDescriptor->Buffer2NextDescAddr;
	}
    else
	{
	xReceivedLength = 0;
	}

    /* Obtain the size of the packet and put it into the "usReceivedLength" variable. */
    /* In order to make the code easier and faster, only packets in a single buffer
     will be accepted.  This can be done by making the buffers large enough to
     hold a complete Ethernet packet (1536 bytes). */
    if (xReceivedLength > 0ul && xReceivedLength < ETH_RX_BUF_SIZE)
	{
	if ((pxDMARxDescriptor->Status
		& ( ETH_DMARXDESC_CE | ETH_DMARXDESC_IPV4HCE | ETH_DMARXDESC_FT))
		!= ETH_DMARXDESC_FT)
	    {
	    /* Not an Ethernet frame-type or a checksum error. */
	    xAccepted = pdFALSE;
	    }
	else
	    {
	    /* See if this packet must be handled. */
	    xAccepted = xMayAcceptPacket(pucBuffer);
	    }

	if (xAccepted != pdFALSE)
	    {
	    /* The packet wil be accepted, but check first if a new Network Buffer can
	     be obtained. If not, the packet will still be dropped. */
	    pxNewDescriptor = pxGetNetworkBufferWithDescriptor( ETH_RX_BUF_SIZE,
		    xDescriptorWaitTime);

	    if (pxNewDescriptor == NULL)
		{
		/* A new descriptor can not be allocated now. This packet will be dropped. */
		xAccepted = pdFALSE;
		}

	    /* In this mode, the two descriptors are the same. */
	    pxCurDescriptor = pxNewDescriptor;
	    if (pxNewDescriptor != NULL)
		{
		/* The packet is acepted and a new Network Buffer was created,
		 copy data to the Network Bufffer. */
		memcpy(pxNewDescriptor->pucEthernetBuffer, pucBuffer,
			xReceivedLength);
		}
	    }

	if (xAccepted != pdFALSE)
	    {
	    pxCurDescriptor->xDataLength = xReceivedLength;
	    xRxEvent.pvData = (void*) pxCurDescriptor;

	    /* Pass the data to the TCP/IP task for processing. */
	    if (xSendEventStructToIPTask(&xRxEvent,
		    xDescriptorWaitTime) == pdFALSE)
		{
		/* Could not send the descriptor into the TCP/IP stack, it
		 must be released. */
		vReleaseNetworkBufferAndDescriptor(pxCurDescriptor);
		iptraceETHERNET_RX_EVENT_LOST();
		}
	    else
		{
		iptraceNETWORK_INTERFACE_RECEIVE();
		}
	    }

	/* Set Buffer1 size and Second Address Chained bit */
	pxDMARxDescriptor->ControlBufferSize = ETH_DMARXDESC_RCH
		| (uint32_t) ETH_RX_BUF_SIZE;
	pxDMARxDescriptor->Status = ETH_DMARXDESC_OWN;

	/* When Rx Buffer unavailable flag is set clear it and resume
	 reception. */
	if ((heth.Instance->DMASR & ETH_DMASR_RBUS) != 0)
	    {
	    /* Clear RBUS ETHERNET DMA flag. */
	    heth.Instance->DMASR = ETH_DMASR_RBUS;

	    /* Resume DMA reception. */
	    heth.Instance->DMARPDR = 0;
	    }
	}

    return (xReceivedLength > 0);
    }











































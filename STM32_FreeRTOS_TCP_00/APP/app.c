

/*
 * FreeRTOS Kernel V10.3.1
 * Copyright (C) 2017 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * http://www.FreeRTOS.org
 * http://aws.amazon.com/freertos
 *
 * 1 tab == 4 spaces!
 */

/*
 * This project is a cut down version of the project described on the following
 * link.  Only the simple UDP client and server and the TCP echo clients are
 * included in the build:
 * http://www.freertos.org/FreeRTOS-Plus/FreeRTOS_Plus_TCP/examples_FreeRTOS_simulator.html
 */

/* Standard includes. */
#include <stdio.h>
#include <time.h>

/* FreeRTOS includes. */
#include "FreeRTOS.h"
#include "task.h"

/* Demo application includes. */
#include "FreeRTOS_IP.h"
#include "FreeRTOS_Sockets.h"


/* Default MAC address configuration.  The demo creates a virtual network
 connection that uses this MAC address by accessing the raw Ethernet/WiFi data
 to and from a real network connection on the host PC.  See the
 configNETWORK_INTERFACE_TO_USE definition above for information on how to
 configure the real network connection to use. */
#define configMAC_ADDR0		0x00
#define configMAC_ADDR1		0x11
#define configMAC_ADDR2		0x22
#define configMAC_ADDR3		0x33
#define configMAC_ADDR4		0x44
#define configMAC_ADDR5		0x41

/* Default IP address configuration.  Used in ipconfigUSE_DNS is set to 0, or
 ipconfigUSE_DNS is set to 1 but a DNS server cannot be contacted. */
#define configIP_ADDR0		192
#define configIP_ADDR1		168
#define configIP_ADDR2		31
#define configIP_ADDR3		90

/* Default gateway IP address configuration.  Used in ipconfigUSE_DNS is set to
 0, or ipconfigUSE_DNS is set to 1 but a DNS server cannot be contacted. */
#define configGATEWAY_ADDR0	192
#define configGATEWAY_ADDR1	168
#define configGATEWAY_ADDR2	31
#define configGATEWAY_ADDR3	1

/* Default DNS server configuration.  OpenDNS addresses are 208.67.222.222 and
 208.67.220.220.  Used in ipconfigUSE_DNS is set to 0, or ipconfigUSE_DNS is set
 to 1 but a DNS server cannot be contacted.*/
#define configDNS_SERVER_ADDR0 	208
#define configDNS_SERVER_ADDR1 	67
#define configDNS_SERVER_ADDR2 	222
#define configDNS_SERVER_ADDR3 	222

/* Default netmask configuration.  Used in ipconfigUSE_DNS is set to 0, or
 ipconfigUSE_DNS is set to 1 but a DNS server cannot be contacted. */
#define configNET_MASK0		255
#define configNET_MASK1		255
#define configNET_MASK2		255
#define configNET_MASK3		0

/* Define a name that will be used for LLMNR and NBNS searches. */
#define mainHOST_NAME "FreeRTOS"
#define mainDEVICE_NICK_NAME "STM32_FreeRTOS_TCP_Stack"

/*
 * Just seeds the simple pseudo random number generator.
 */
static void prvSRand(UBaseType_t ulSeed);

/*
 * Miscellaneous initialisation including preparing the logging and seeding the
 * random number generator.
 */
static void prvMiscInitialisation(void);

/* The default IP and MAC address used by the demo.  The address configuration
defined here will be used if ipconfigUSE_DHCP is 0, or if ipconfigUSE_DHCP is
1 but a DHCP server could not be contacted.  See the online documentation for
more information. */
static const uint8_t ucIPAddress[4] = {configIP_ADDR0, configIP_ADDR1, configIP_ADDR2, configIP_ADDR3};
static const uint8_t ucNetMask[4] = {configNET_MASK0, configNET_MASK1, configNET_MASK2, configNET_MASK3};
static const uint8_t ucGatewayAddress[4] = {configGATEWAY_ADDR0, configGATEWAY_ADDR1, configGATEWAY_ADDR2, configGATEWAY_ADDR3};
static const uint8_t ucDNSServerAddress[4] = {configDNS_SERVER_ADDR0, configDNS_SERVER_ADDR1, configDNS_SERVER_ADDR2, configDNS_SERVER_ADDR3};

/* Set the following constant to pdTRUE to log using the method indicated by the
name of the constant, or pdFALSE to not log using the method indicated by the
name of the constant.  Options include to standard out (xLogToStdout), to a disk
file (xLogToFile), and to a UDP port (xLogToUDP).  If xLogToUDP is set to pdTRUE
then UDP messages are sent to the IP address configured as the echo server
address (see the configECHO_SERVER_ADDR0 definitions in FreeRTOSConfig.h) and
the port number set by configPRINT_PORT in FreeRTOSConfig.h. */
const BaseType_t xLogToStdout = pdTRUE, xLogToFile = pdFALSE, xLogToUDP = pdFALSE;

/* Default MAC address configuration.  The demo creates a virtual network
connection that uses this MAC address by accessing the raw Ethernet data
to and from a real network connection on the host PC.  See the
configNETWORK_INTERFACE_TO_USE definition for information on how to configure
the real network connection to use. */
const uint8_t ucMACAddress[6] = {configMAC_ADDR0, configMAC_ADDR1, configMAC_ADDR2, configMAC_ADDR3, configMAC_ADDR4, configMAC_ADDR5};

/* Use by the pseudo random number generator. */
static UBaseType_t ulNextRand;

void Add_TCP_IP(void)
{
    /*
     * Instructions for using this project are provided on:
     * http://www.freertos.org/FreeRTOS-Plus/FreeRTOS_Plus_TCP/examples_FreeRTOS_simulator.html
     */

    /* Miscellaneous initialisation including preparing the logging and seeding
    the random number generator. */
    prvMiscInitialisation();

    /* Initialise the network interface.

    ***NOTE*** Tasks that use the network are created in the network event hook
    when the network is connected and ready for use (see the definition of
    vApplicationIPNetworkEventHook() below).  The address values passed in here
    are used if ipconfigUSE_DHCP is set to 0, or if ipconfigUSE_DHCP is set to 1
    but a DHCP server cannot be	contacted. */
    FreeRTOS_debug_printf(("FreeRTOS_IPInit\n"));
    FreeRTOS_IPInit(ucIPAddress,
                    ucNetMask,
                    ucGatewayAddress,
                    ucDNSServerAddress,
                    ucMACAddress);
}

void vApplicationIdleHook(void)
{
    /* This is just a trivial example of an idle hook.  It is called on each
    cycle of the idle task if configUSE_IDLE_HOOK is set to 1 in
    FreeRTOSConfig.h.  It must *NOT* attempt to block.  In this case the
    idle task just sleeps to lower the CPU usage. */
}
/*-----------------------------------------------------------*/

void vAssertCalled(const char *pcFile, uint32_t ulLine)
{
    volatile uint32_t ulBlockVariable = 0UL;
    volatile char *pcFileName = (volatile char *)pcFile;
    volatile uint32_t ulLineNumber = ulLine;

    (void)pcFileName;
    (void)ulLineNumber;

    FreeRTOS_debug_printf(("vAssertCalled( %s, %ld\n", pcFile, ulLine));

    /* Setting ulBlockVariable to a non-zero value in the debugger will allow
    this function to be exited. */
    taskDISABLE_INTERRUPTS();
    {
        while (ulBlockVariable == 0UL)
        {
        }
    }
    taskENABLE_INTERRUPTS();
}
/*-----------------------------------------------------------*/

/* Called by FreeRTOS+TCP when the network connects or disconnects.
   Disconnect events are only received if implemented in the MAC driver. */
void vApplicationIPNetworkEventHook(eIPCallbackEvent_t eNetworkEvent)
{
    uint32_t ulIPAddress, ulNetMask, ulGatewayAddress, ulDNSServerAddress;
    char cBuffer[16];
    static BaseType_t xTasksAlreadyCreated = pdFALSE;

    /* If the network has just come up...*/
    if (eNetworkEvent == eNetworkUp)
    {
        /* Create the tasks that use the IP stack if they have not already been
        created. */

        xTasksAlreadyCreated = pdTRUE;

        /* Print out the network configuration, which may have come from a DHCP
        server. */
        FreeRTOS_GetAddressConfiguration(&ulIPAddress, &ulNetMask, &ulGatewayAddress, &ulDNSServerAddress);
        FreeRTOS_inet_ntoa(ulIPAddress, cBuffer);
        FreeRTOS_printf(("\r\n\r\nIP Address: %s\r\n", cBuffer));

        FreeRTOS_inet_ntoa(ulNetMask, cBuffer);
        FreeRTOS_printf(("Subnet Mask: %s\r\n", cBuffer));

        FreeRTOS_inet_ntoa(ulGatewayAddress, cBuffer);
        FreeRTOS_printf(("Gateway Address: %s\r\n", cBuffer));

        FreeRTOS_inet_ntoa(ulDNSServerAddress, cBuffer);
        FreeRTOS_printf(("DNS Server Address: %s\r\n\r\n\r\n", cBuffer));
    }
}
/*-----------------------------------------------------------*/

void vApplicationMallocFailedHook(void)
{
    /* Called if a call to pvPortMalloc() fails because there is insufficient
    free memory available in the FreeRTOS heap.  pvPortMalloc() is called
    internally by FreeRTOS API functions that create tasks, queues, software
    timers, and semaphores.  The size of the FreeRTOS heap is set by the
    configTOTAL_HEAP_SIZE configuration constant in FreeRTOSConfig.h. */
    vAssertCalled(__FILE__, __LINE__);
}
/*-----------------------------------------------------------*/

UBaseType_t uxRand(void)
{
    const uint32_t ulMultiplier = 0x015a4e35UL, ulIncrement = 1UL;

    /* Utility function to generate a pseudo random number. */

    ulNextRand = (ulMultiplier * ulNextRand) + ulIncrement;
    return ((int)(ulNextRand >> 16UL) & 0x7fffUL);
}
/*-----------------------------------------------------------*/

static void prvSRand(UBaseType_t ulSeed)
{
    /* Utility function to seed the pseudo random number generator. */
    ulNextRand = ulSeed;
}
/*-----------------------------------------------------------*/

static void prvMiscInitialisation(void)
{
    time_t xTimeNow;

    /* Seed the random number generator. */
    time(&xTimeNow);

    FreeRTOS_debug_printf(("Seed for randomizer: %lu\n", xTimeNow));
    prvSRand((uint32_t)xTimeNow);
    FreeRTOS_debug_printf(("Random numbers: %08X %08X %08X %08X\n",
                           ipconfigRAND32(),
                           ipconfigRAND32(),
                           ipconfigRAND32(),
                           ipconfigRAND32()));
}
/*-----------------------------------------------------------*/

#if (ipconfigUSE_LLMNR != 0) || (ipconfigUSE_NBNS != 0) || (ipconfigDHCP_REGISTER_HOSTNAME == 1)

const char *pcApplicationHostnameHook(void)
{
    /* Assign the name "FreeRTOS" to this network node.  This function will
        be called during the DHCP: the machine will be registered with an IP
        address plus this name. */
    return mainHOST_NAME;
}

#endif
/*-----------------------------------------------------------*/

#if (ipconfigUSE_LLMNR != 0) || (ipconfigUSE_NBNS != 0)

BaseType_t xApplicationDNSQueryHook(const char *pcName)
{
    BaseType_t xReturn;

    /* Determine if a name lookup is for this node.  Two names are given
       to this node: that returned by pcApplicationHostnameHook() and that set
       by mainDEVICE_NICK_NAME. */
    if (strcmp(pcName, pcApplicationHostnameHook()) == 0)
    {
        xReturn = pdPASS;
    }
    else if (strcmp(pcName, mainDEVICE_NICK_NAME) == 0)
    {
        xReturn = pdPASS;
    }
    else
    {
        xReturn = pdFAIL;
    }

    return xReturn;
}

#endif

/*
 * Callback that provides the inputs necessary to generate a randomized TCP
 * Initial Sequence Number per RFC 6528.  THIS IS ONLY A DUMMY IMPLEMENTATION
 * THAT RETURNS A PSEUDO RANDOM NUMBER SO IS NOT INTENDED FOR USE IN PRODUCTION
 * SYSTEMS.
 */
extern uint32_t ulApplicationGetNextSequenceNumber(uint32_t ulSourceAddress,
                                                   uint16_t usSourcePort,
                                                   uint32_t ulDestinationAddress,
                                                   uint16_t usDestinationPort)
{
    (void)ulSourceAddress;
    (void)usSourcePort;
    (void)ulDestinationAddress;
    (void)usDestinationPort;

    return uxRand();
}

/*
 * Supply a random number to FreeRTOS+TCP stack.
 * THIS IS ONLY A DUMMY IMPLEMENTATION THAT RETURNS A PSEUDO RANDOM NUMBER
 * SO IS NOT INTENDED FOR USE IN PRODUCTION SYSTEMS.
 */
BaseType_t xApplicationGetRandomNumber(uint32_t *pulNumber)
{
    *(pulNumber) = uxRand();
    return pdTRUE;
}

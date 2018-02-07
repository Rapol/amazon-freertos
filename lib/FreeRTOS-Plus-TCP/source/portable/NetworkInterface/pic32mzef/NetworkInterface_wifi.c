/*******************************************************************************
  Network Interface file

  Summary:
    Network Interface file for FreeRTOS-Plus-TCP stack
    
  Description:
    - Interfaces PIC32 to the FreeRTOS TCP/IP stack
*******************************************************************************/

/*******************************************************************************
File Name:  pic32_NetworkInterface.c
Copyright 2017 Microchip Technology Incorporated and its subsidiaries.

Permission is hereby granted, free of charge, to any person obtaining a copy of 
this software and associated documentation files (the "Software"), to deal in 
the Software without restriction, including without limitation the rights to 
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies 
of the Software, and to permit persons to whom the Software is furnished to do 
so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in all 
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE 
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, 
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE 
SOFTWARE
*******************************************************************************/
#include <sys/kmem.h>

#include "FreeRTOS.h"
#include "semphr.h"
#include "event_groups.h"
#include "FreeRTOS_IP.h"
#include "FreeRTOS_IP_Private.h"

#include "NetworkInterface.h"
#include "NetworkBufferManagement.h"


#include "pic32_NetworkInterface.h"
#include "pic32_NetworkConfig.h"

#include "peripheral/eth/plib_eth.h"

#include "system_config.h"
#include "system/console/sys_console.h"
#include "system/debug/sys_debug.h"
#include "system/command/sys_command.h"

#include "driver/ethmac/drv_ethmac.h"
#include "driver/miim/drv_miim.h"

#include "tcpip/tcpip.h"
#include "tcpip/src/tcpip_private.h"
#include "tcpip/src/link_list.h"
#include "Port_WiFi.h"
#include "Wilc1000_task.h"

// local definitions and data


// FreeRTOS implementation functions
BaseType_t xNetworkInterfaceInitialise( void )
{
	WIFINetworkParams_t TheBlackHole= { clientcredentialWIFI_SSID,
										sizeof(clientcredentialWIFI_SSID),
										clientcredentialWIFI_PASSWORD,
										sizeof(clientcredentialWIFI_PASSWORD),
										clientcredentialWIFI_SECURITY,
										255};
	
	//Turn  WiFi ON
  	if(WIFI_On()!= eWiFiSuccess)
   	{
        	return pdFAIL;
    	}

	// Connect to the AP
  	if(WIFI_ConnectAP(&TheBlackHole)!= eWiFiSuccess)
    	{
        	return pdFAIL;
    	}

    	return pdPASS;
}


BaseType_t xNetworkInterfaceOutput( NetworkBufferDescriptor_t * const pxDescriptor, BaseType_t xReleaseAfterSend )
{

    BaseType_t retRes = pdFALSE;

    if(pxDescriptor != 0 && pxDescriptor->pucEthernetBuffer != 0 && pxDescriptor->xDataLength != 0)
    {

	 // There you go
	 if(WDRV_EXT_DataSend(pxDescriptor->xDataLength,pxDescriptor->pucEthernetBuffer) == 0)
	 	retRes = pdTRUE;
	 	
        /* The buffer has been sent so can be released. */
        if( xReleaseAfterSend != pdFALSE )
        {
            vReleaseNetworkBufferAndDescriptor( pxDescriptor );
        }
    }

    return retRes;
}


//************************************ Section: helper functions **************************************************
//



//************************************ Section: worker code **************************************************
//

void xNetworkFrameReceived(uint32_t len, uint8_t const *const frame)
{
    bool pktSuccess, pktLost;
    NetworkBufferDescriptor_t *pxNetworkBuffer;
    IPStackEvent_t xRxEvent = { eNetworkRxEvent, NULL };

    pktSuccess = pktLost = false;

    while(true)
    {
        if( eConsiderFrameForProcessing( frame ) != eProcessBuffer )
        {
            break;
        }

        // get the network descriptor (no data buffer) to hold this packet
        pxNetworkBuffer = pxGetNetworkBufferWithDescriptor( len, 0 );
        if(pxNetworkBuffer == 0)
        {
            pktLost = true;
            break;
        }

        memcpy( pxNetworkBuffer->pucEthernetBuffer, frame, len);

        xRxEvent.pvData = ( void * ) pxNetworkBuffer;

        // Send the data to the TCP/IP stack
        if( xSendEventStructToIPTask( &xRxEvent, 0 ) == pdFALSE )
        {   // failed
            pktLost = true;
        }
        else
        {   // success
            pktSuccess = true;
            iptraceNETWORK_INTERFACE_RECEIVE();
        }

        break;
    }

    if(!pktSuccess)
    {   // smth went wrong; nothing sent to the 
        if(pxNetworkBuffer != 0)
        {
            pxNetworkBuffer->pucEthernetBuffer = 0;
            vReleaseNetworkBufferAndDescriptor( pxNetworkBuffer );
        }
        
        if(pktLost)
        {
            iptraceETHERNET_RX_EVENT_LOST();
        }
    }      
}

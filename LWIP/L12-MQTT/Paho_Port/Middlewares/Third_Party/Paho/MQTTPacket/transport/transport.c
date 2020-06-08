/*******************************************************************************
 * Copyright (c) 2014 IBM Corp.
 *
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v1.0
 * and Eclipse Distribution License v1.0 which accompany this distribution.
 *
 * The Eclipse Public License is available at
 *    http://www.eclipse.org/legal/epl-v10.html
 * and the Eclipse Distribution License is available at
 *   http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * Contributors:
 *    Ian Craggs - initial API and implementation and/or initial documentation
 *    Sergio R. Caprile - "commonalization" from prior samples and/or documentation extension
 *******************************************************************************/

 #include "transport.h"
 #include "lwip/opt.h"
 #include "lwip/arch.h"
 #include "lwip/api.h"
 #include "lwip/inet.h"
 #include "lwip/sockets.h"
 #include "string.h"

/**
This simple low-level implementation assumes a single connection for a single thread. Thus, a static
variable is used for that connection.
On other scenarios, the user must solve this by taking int32_to account that the current implementation of
MQTTPacket_read() has a function point32_ter for a function call to get the data to a buffer, but no provisions
to know the caller or other indicator (the socket id): int32_t (*getfn)(uint8_t*, int32_t)
*/
static int32_t mysock;


int32_t transport_sendPacketBuffer(int32_t sock, uint8_t* buf, int32_t buflen)
{
	int32_t rc = 0;
	rc = write(sock, buf, buflen);
	return rc;
}


int32_t transport_getdata(uint8_t* buf, int32_t count)
{
	int32_t rc = recv(mysock, buf, count, 0);
	return rc;
}

int32_t transport_getdatanb(void *sck, uint8_t* buf, int32_t count)
{
	int32_t sock = *((int32_t *)sck); 	/* sck: point32_ter to whatever the system may use to identify the transport */
	/* this call will return after the timeout set on initialization if no bytes;
	   in your system you will use whatever you use to get whichever outstanding
	   bytes your socket equivalent has ready to be extracted right now, if any,
	   or return immediately */
	int32_t rc = recv(sock, buf, count, 0);
	if (rc == -1) {
		/* check error conditions from your system here, and return -1 */
		return 0;
	}
	return rc;
}

/**
return >=0 for a socket descriptor, <0 for an error code
@todo Basically moved from the sample without changes, should accomodate same usage for 'sock' for clarity,
removing indirections
*/
int32_t transport_open(char* servip, int32_t port)
{
    int32_t *sock = &mysock;
    int32_t ret;
//  int32_t opt;
    struct sockaddr_in addr;

    memset(&addr,0,sizeof(addr));
    addr.sin_len = sizeof(addr);
    addr.sin_family = AF_INET;

    addr.sin_port = PP_HTONS(port);
    addr.sin_addr.s_addr = inet_addr((const char*)servip);

    *sock = socket(AF_INET,SOCK_STREAM,0);

    ret = connect(*sock,(struct sockaddr*)&addr,sizeof(addr));
    if (ret != 0)
    {
        close(*sock);
        return -1;
    }
//  opt = 1000;
//  setsockopt(*sock,SOL_SOCKET,SO_RCVTIMEO,&opt,sizeof(int));

    return *sock;
}

int32_t transport_close(int32_t sock)
{
int32_t rc;

	rc = shutdown(sock, SHUT_WR);
	rc = recv(sock, NULL, (size_t)0, 0);
	rc = close(sock);

	return rc;
}

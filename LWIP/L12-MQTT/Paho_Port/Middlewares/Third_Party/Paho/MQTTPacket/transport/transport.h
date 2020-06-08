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

#include "stdint.h"

int32_t transport_sendPacketBuffer(int32_t sock, uint8_t* buf, int32_t buflen);
int32_t transport_getdata(uint8_t* buf, int32_t count);
int32_t transport_getdatanb(void *sck, uint8_t* buf, int32_t count);
int32_t transport_open(char* host, int32_t port);
int32_t transport_close(int32_t sock);

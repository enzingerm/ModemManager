/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details:
 *
 * Copyright (C) 2020 Marinus Enzinger
 */

#include <config.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>

#include "ModemManager.h"
#include "mm-log.h"
#include "mm-iface-modem.h"
#include "mm-broadband-modem-xmm.h"
#include "mm-shared-xmm.h"
#include "mm-broadband-modem-xmm7360.h"


static void iface_modem_init (MMIfaceModem *iface);
// static void shared_xmm_init  (MMSharedXmm  *iface);
// static void iface_modem_signal_init (MMIfaceModemSignal *iface);

/* XMM7360 specific bearer creation */
void xmm7360_create_bearer (MMIfaceModem *self,
                        MMBearerProperties *properties,
                        GAsyncReadyCallback callback,
                        gpointer user_data);
MMBaseBearer * xmm7360_create_bearer_finish (MMIfaceModem *self,
                                        GAsyncResult *res,
                                        GError **error);

G_DEFINE_TYPE_EXTENDED (MMBroadbandModemXmm7360, mm_broadband_modem_xmm7360, MM_TYPE_BROADBAND_MODEM_XMM, 0, 
                         G_IMPLEMENT_INTERFACE (MM_TYPE_IFACE_MODEM, iface_modem_init))

/*****************************************************************************/

MMBroadbandModemXmm7360 *
mm_broadband_modem_xmm7360_new (const gchar  *device,
                            const gchar **drivers,
                            const gchar  *plugin,
                            guint16       vendor_id,
                            guint16       product_id)
{
    return g_object_new (MM_TYPE_BROADBAND_MODEM_XMM7360,
                         MM_BASE_MODEM_DEVICE,     device,
                         MM_BASE_MODEM_DRIVERS,    drivers,
                         MM_BASE_MODEM_PLUGIN,     plugin,
                         MM_BASE_MODEM_VENDOR_ID,  vendor_id,
                         MM_BASE_MODEM_PRODUCT_ID, product_id,
                         NULL);
}

static void
mm_broadband_modem_xmm7360_init (MMBroadbandModemXmm7360 *self)
{
    //TODO: RPC calls to initialize modem
}

static void
iface_modem_init (MMIfaceModem *iface)
{
    iface->create_bearer = xmm7360_create_bearer;
    iface->create_bearer_finish = xmm7360_create_bearer_finish;
}

static void
mm_broadband_modem_xmm7360_class_init (MMBroadbandModemXmm7360Class *klass)
{

}


void xmm7360_create_bearer (MMIfaceModem *self,
                        MMBearerProperties *properties,
                        GAsyncReadyCallback callback,
                        gpointer user_data)
{
    //TODO: create bearer
}

MMBaseBearer * xmm7360_create_bearer_finish (MMIfaceModem *self,
                                        GAsyncResult *res,
                                        GError **error)
{
    //TODO: return bearer
    return NULL;
}
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
 * GNU General Public License for more details.
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
#include "mm-xmm7360-rpc.h"
#include "mm-bearer-xmm7360.h"

struct _MMBroadbandModemXmm7360Private {
    xmm7360_rpc rpc;
};

static void iface_modem_init (MMIfaceModem *iface);

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
    xmm7360_rpc* rpc;

    /* Initialize private data */
    self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
                                              MM_TYPE_BROADBAND_MODEM_XMM7360,
                                              MMBroadbandModemXmm7360Private);

    /* initialize modem RPC */
    rpc = &self->priv->rpc;
    if(xmm7360_rpc_init(rpc) != 0) {
        mm_obj_err (self, "Failed to initialize XMM7360 RPC!");
        /* TODO: handle rpc initialization error */
        return;
    }

    mm_obj_dbg (self, "Initializing XMM7360 modem!");
    /* lots of synchronous calls, this has to be improved for sure */
    xmm7360_rpc_execute(rpc, UtaMsSmsInit, FALSE, NULL, NULL);
    xmm7360_rpc_execute(rpc, UtaMsCbsInit, FALSE, NULL, NULL);
    xmm7360_rpc_execute(rpc, UtaMsNetOpen, FALSE, NULL, NULL);
    xmm7360_rpc_execute(rpc, UtaMsCallCsInit, FALSE, NULL, NULL);
    xmm7360_rpc_execute(rpc, UtaMsCallPsInitialize, FALSE, NULL, NULL);
    /* TODO: Signal reporting does not work yet, maybe use different parameters to this call? */
    xmm7360_rpc_execute(rpc, UtaMsNetSetRadioSignalReporting, FALSE, NULL, NULL);
    xmm7360_rpc_execute(rpc, UtaMsSsInit, FALSE, NULL, NULL);
    xmm7360_rpc_execute(rpc, UtaMsSimOpenReq, FALSE, NULL, NULL);

    if(xmm7360_do_fcc_unlock(rpc) != 0) {
        /* TODO: handle error */
        return;
    }
    if(xmm7360_uta_mode_set(rpc, 1) != 0) {
        /* TODO: handle error */
        return;
    }

    /* Had to do this otherwise ModemManager gets an error when issuing AT+CPIN? while loading modem capabilities */
    while(!rpc->sim_initialized) {
        xmm7360_rpc_pump(rpc, NULL);
    }

    mm_obj_dbg (self, "Successfully initialized XMM7360 modem!");
}

static void
iface_modem_init (MMIfaceModem *iface)
{
    iface->create_bearer = xmm7360_create_bearer;
    iface->create_bearer_finish = xmm7360_create_bearer_finish;
}

static void
dispose (GObject *object)
{
    MMBroadbandModemXmm7360 *self = MM_BROADBAND_MODEM_XMM7360 (object);

    /* disconnect from RPC */
    xmm7360_rpc_dispose(&self->priv->rpc);

    G_OBJECT_CLASS (mm_broadband_modem_xmm7360_parent_class)->dispose (object);
}

static void
mm_broadband_modem_xmm7360_class_init (MMBroadbandModemXmm7360Class *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    g_type_class_add_private (object_class, sizeof (MMBroadbandModemXmm7360Private));

    /* virtual methods */
    object_class->dispose = dispose;
}


void xmm7360_create_bearer (MMIfaceModem *_self,
                        MMBearerProperties *properties,
                        GAsyncReadyCallback callback,
                        gpointer user_data)
{
    MMBaseBearer *bearer;
    GTask *task;
    MMBroadbandModemXmm7360 *self = MM_BROADBAND_MODEM_XMM7360(_self);

    bearer = mm_bearer_xmm7360_new (self, properties, &self->priv->rpc);

    task = g_task_new (self, NULL, callback, user_data);
    g_task_return_pointer (task, bearer, g_object_unref);
    g_object_unref (task);
}

MMBaseBearer * xmm7360_create_bearer_finish (MMIfaceModem *self,
                                        GAsyncResult *res,
                                        GError **error)
{
    return g_task_propagate_pointer (G_TASK (res), error);
}

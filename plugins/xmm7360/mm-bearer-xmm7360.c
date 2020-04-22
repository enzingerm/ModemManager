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
 * Copyright (C) 2020 Marinus Enzinger <marinus@enzingerm.de>
 */

#include <arpa/inet.h>

#include <ModemManager.h>
#define _LIBMM_INSIDE_MM
#include <libmm-glib.h>

#include "mm-bearer-xmm7360.h"
#include "mm-log.h"
#include "mm-xmm7360-rpc.h"

G_DEFINE_TYPE (MMBearerXmm7360, mm_bearer_xmm7360, MM_TYPE_BASE_BEARER)

struct _MMBearerXmm7360Private {
    xmm7360_ip_config ip_config;
    xmm7360_rpc *rpc;
    MMPort *data;
    gchar ip[16];
    gchar dns1[16];
    gchar dns2[16];
    gchar* dnsaddrs[3];
    MMBearerIpConfig *bearer_ip_config;
};

/*****************************************************************************/
/* Connect */

static MMBearerConnectResult *
connect_finish (MMBaseBearer *_self,
                GAsyncResult *res,
                GError **error)
{

    return g_task_propagate_pointer (G_TASK (res), error);
}

static void
convert_xmm_ip_config(MMBearerXmm7360Private *priv) {
    MMBearerIpConfig *dst = priv->bearer_ip_config;
    xmm7360_ip_config *src = &priv->ip_config;
    /* we get static IPs to set */
    mm_bearer_ip_config_set_method(dst, MM_BEARER_IP_METHOD_STATIC);
    inet_ntop(AF_INET, &src->ip4_1, priv->ip, 16);
    inet_ntop(AF_INET, &src->dns4_1, priv->dns1, 16);
    inet_ntop(AF_INET, &src->dns4_2, priv->dns2, 16);
    mm_bearer_ip_config_set_address(dst, priv->ip);
    mm_bearer_ip_config_set_dns(dst, (const gchar**)priv->dnsaddrs);
}

static void
_connect (MMBaseBearer *_self,
          GCancellable *cancellable,
          GAsyncReadyCallback callback,
          gpointer user_data)
{
    MMBaseModem *modem  = NULL;
    MMPort *data = NULL;
    const gchar *apn;
    gint32 status = 0;
    GTask *task;
    MMBearerXmm7360 *self = MM_BEARER_XMM7360 (_self);
    xmm7360_rpc *rpc = self->priv->rpc;
    MMBearerIpConfig *ip_config;
    MMBearerConnectResult *connect_result;
    

    g_object_get (self,
                  MM_BASE_BEARER_MODEM, &modem,
                  NULL);
    g_assert (modem);

    /* Grab a data port */
    data = mm_base_modem_get_best_data_port (modem, MM_PORT_TYPE_NET);
    if (!data) {
        g_task_report_new_error (
            self,
            callback,
            user_data,
            _connect,
            MM_CORE_ERROR,
            MM_CORE_ERROR_NOT_FOUND,
            "No valid data port found to launch connection");
    }

    mm_dbg ("XMM7360 Bearer: data port grabbed, now trying to connect!");

    /* Check whether we have an APN */
    apn = mm_bearer_properties_get_apn (mm_base_bearer_peek_config (_self));

    /**
     * All the following RPC calls are made synchronously one after another. This should
     * be subject to improvement (make it more robust and also asynchronous)
     */

    if(xmm7360_rpc_execute(
        rpc,
        UtaMsCallPsAttachApnConfigReq,
        TRUE,
        pack_uta_ms_call_ps_attach_apn_config_req(apn),
        NULL
    ) != 0) {
        g_task_report_new_error (
            self,
            callback,
            user_data,
            _connect,
            MM_CORE_ERROR,
            MM_CORE_ERROR_NOT_FOUND,
            "could not attach APN config");
        return;
    }

    mm_dbg ("XMM7360 Bearer: after attach APN config!");

    if(xmm7360_net_attach(rpc, &status) != 0) {
        g_task_report_new_error (
            self,
            callback,
            user_data,
            _connect,
            MM_CORE_ERROR,
            MM_CORE_ERROR_NOT_FOUND,
            "could not attach to net!");
        return;
    }
    mm_dbg ("XMM7360 Bearer: after net attach!");
    if(status == (gint32)0xffffffff) {
        while(!rpc->attach_allowed) {
            xmm7360_rpc_pump(rpc, NULL);
        }
        // now attach is allowed
        if(xmm7360_net_attach(rpc, &status) != 0) {
            g_task_report_new_error (
                self,
                callback,
                user_data,
                _connect,
                MM_CORE_ERROR,
                MM_CORE_ERROR_NOT_FOUND,
                "could not attach to net");
            return;
        }
        if(status == (gint32)0xffffffff) {
            g_task_report_new_error (
                self,
                callback,
                user_data,
                _connect,
                MM_CORE_ERROR,
                MM_CORE_ERROR_NOT_FOUND,
                "giving up attaching to net");
            return;
        }
    }

    mm_dbg ("XMM7360 Bearer: attached to net, waiting shortly before getting ip config!");

    // ugly: wait 1 seconds before fetching IP config
    sleep(1);

    if(xmm7360_get_ip_and_dns(rpc, &self->priv->ip_config) != 0) {
        g_task_report_new_error (
            self,
            callback,
            user_data,
            _connect,
            MM_CORE_ERROR,
            MM_CORE_ERROR_NOT_FOUND,
            "could not get IP config!");
        return;
    }

    mm_dbg ("XMM7360 Bearer: IP config fetched!");

    if(xmm7360_establish_connection(rpc) != 0) {
        g_task_report_new_error (
            self,
            callback,
            user_data,
            _connect,
            MM_CORE_ERROR,
            MM_CORE_ERROR_NOT_FOUND,
            "error establishing connection finally!");
        return;
    }
    
    /* unref data from previous connections */
    g_object_unref(self->priv->data);
    g_clear_object (&self->priv->bearer_ip_config);
    ip_config = self->priv->bearer_ip_config = mm_bearer_ip_config_new ();

    convert_xmm_ip_config(self->priv);
    self->priv->data = g_object_ref(data);

    connect_result = mm_bearer_connect_result_new(self->priv->data, ip_config, NULL);

    task = g_task_new (self, NULL, callback, user_data);
    g_task_return_pointer (task, connect_result, g_object_unref);
    g_object_unref (task);
}

/*****************************************************************************/
/* Disconnect */

static gboolean
disconnect_finish (MMBaseBearer *self,
                   GAsyncResult *res,
                   GError **error)
{
    return g_task_propagate_boolean (G_TASK (res), error);
}

static void
disconnect (MMBaseBearer *_self,
            GAsyncReadyCallback callback,
            gpointer user_data)
{
    GTask* task;
    /* TODO: implement */
    task = g_task_new (_self, NULL, callback, user_data);
    g_task_return_boolean(task, FALSE);
    g_object_unref (task);
}

/*****************************************************************************/

static void
report_connection_status (MMBaseBearer             *_self,
                          MMBearerConnectionStatus  status)
{
    /* TODO: implement */

    /* Chain up parent's report_connection_status() */
    MM_BASE_BEARER_CLASS (mm_bearer_xmm7360_parent_class)->report_connection_status (_self, status);
}

/*****************************************************************************/

MMBaseBearer *
mm_bearer_xmm7360_new (MMBroadbandModemXmm7360 *modem,
                   MMBearerProperties  *config, xmm7360_rpc* rpc)
{
    MMBaseBearer *base_bearer;
    MMBearerXmm7360 *bearer;

    /* The Xmm7360 bearer inherits from MMBaseBearer (so it's not a MMBroadbandBearer)
     * and that means that the object is not async-initable, so we just use
     * g_object_new() here */
    bearer = g_object_new (MM_TYPE_BEARER_XMM7360,
                           MM_BASE_BEARER_MODEM, modem,
                           MM_BASE_BEARER_CONFIG, config,
                           NULL);

    bearer->priv->rpc = rpc;
    
    base_bearer = MM_BASE_BEARER (bearer);
    /* Only export valid bearers */
    mm_base_bearer_export (base_bearer);

    return base_bearer;
}

static void
mm_bearer_xmm7360_init (MMBearerXmm7360 *self)
{
    /* Initialize private data */
    self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
                                              MM_TYPE_BEARER_XMM7360,
                                              MMBearerXmm7360Private);
    self->priv->dnsaddrs[0] = self->priv->dns1;
    self->priv->dnsaddrs[1] = self->priv->dns2;
    self->priv->dnsaddrs[2] = NULL;
}

static void
dispose (GObject *object)
{
    MMBearerXmm7360 *self = MM_BEARER_XMM7360 (object);

    g_object_unref(self->priv->data);
    g_clear_object (&self->priv->bearer_ip_config);

    G_OBJECT_CLASS (mm_bearer_xmm7360_parent_class)->dispose (object);
}

static void
mm_bearer_xmm7360_class_init (MMBearerXmm7360Class *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    MMBaseBearerClass *base_bearer_class = MM_BASE_BEARER_CLASS (klass);

    g_type_class_add_private (object_class, sizeof (MMBearerXmm7360Private));

    /* Virtual methods */
    object_class->dispose = dispose;

    base_bearer_class->connect = _connect;
    base_bearer_class->connect_finish = connect_finish;
    base_bearer_class->disconnect = disconnect;
    base_bearer_class->disconnect_finish = disconnect_finish;
    base_bearer_class->report_connection_status = report_connection_status;
}

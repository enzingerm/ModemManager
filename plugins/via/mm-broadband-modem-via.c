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
 * Copyright (C) 2012 Red Hat, Inc.
 */

#include <config.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>

#define _LIBMM_INSIDE_MM
#include <libmm-glib.h>

#include "mm-log-object.h"
#include "mm-modem-helpers.h"
#include "mm-errors-types.h"
#include "mm-base-modem-at.h"
#include "mm-broadband-modem-via.h"
#include "mm-iface-modem-cdma.h"
#include "mm-iface-modem.h"

static void iface_modem_cdma_init (MMIfaceModemCdma *iface);

static MMIfaceModemCdma *iface_modem_cdma_parent;

G_DEFINE_TYPE_EXTENDED (MMBroadbandModemVia, mm_broadband_modem_via, MM_TYPE_BROADBAND_MODEM, 0,
                        G_IMPLEMENT_INTERFACE (MM_TYPE_IFACE_MODEM_CDMA, iface_modem_cdma_init))

struct _MMBroadbandModemViaPrivate {
    /* Regex for signal quality related notifications */
    GRegex *hrssilvl_regex; /* EVDO signal strength */

    /* Regex for other notifications to ignore */
    GRegex *mode_regex; /* Access technology change */
    GRegex *dosession_regex; /* EVDO data dormancy */
    GRegex *simst_regex;
    GRegex *vpon_regex;
    GRegex *creg_regex;
    GRegex *vrom_regex; /* Roaming indicator (reportedly unreliable) */
    GRegex *vser_regex;
    GRegex *ciev_regex;
    GRegex *vpup_regex;
};

/*****************************************************************************/
/* Setup registration checks (CDMA interface) */

typedef struct {
    gboolean skip_qcdm_call_manager_step;
    gboolean skip_qcdm_hdr_step;
    gboolean skip_at_cdma_service_status_step;
    gboolean skip_at_cdma1x_serving_system_step;
    gboolean skip_detailed_registration_state;
} SetupRegistrationChecksResults;

static gboolean
setup_registration_checks_finish (MMIfaceModemCdma  *self,
                                  GAsyncResult      *res,
                                  gboolean          *skip_qcdm_call_manager_step,
                                  gboolean          *skip_qcdm_hdr_step,
                                  gboolean          *skip_at_cdma_service_status_step,
                                  gboolean          *skip_at_cdma1x_serving_system_step,
                                  gboolean          *skip_detailed_registration_state,
                                  GError           **error)
{
    SetupRegistrationChecksResults *results;

    results = g_task_propagate_pointer (G_TASK (res), error);
    if (!results)
        return FALSE;

    *skip_qcdm_call_manager_step        = results->skip_qcdm_call_manager_step;
    *skip_qcdm_hdr_step                 = results->skip_qcdm_hdr_step;
    *skip_at_cdma_service_status_step   = results->skip_at_cdma_service_status_step;
    *skip_at_cdma1x_serving_system_step = results->skip_at_cdma1x_serving_system_step;
    *skip_detailed_registration_state   = results->skip_detailed_registration_state;

    g_free (results);

    return TRUE;
}

static void
parent_setup_registration_checks_ready (MMIfaceModemCdma *self,
                                        GAsyncResult     *res,
                                        GTask            *task)
{
    GError                         *error = NULL;
    SetupRegistrationChecksResults *results;

    results = g_new0 (SetupRegistrationChecksResults, 1);

    if (!iface_modem_cdma_parent->setup_registration_checks_finish (self,
                                                                    res,
                                                                    &results->skip_qcdm_call_manager_step,
                                                                    &results->skip_qcdm_hdr_step,
                                                                    &results->skip_at_cdma_service_status_step,
                                                                    &results->skip_at_cdma1x_serving_system_step,
                                                                    &results->skip_detailed_registration_state,
                                                                    &error)) {
        g_free (results);
        g_task_return_error (task, error);
    } else {
        /* Skip +CSS */
        results->skip_at_cdma1x_serving_system_step = TRUE;
        /* Skip +CAD */
        results->skip_at_cdma_service_status_step = TRUE;
        /* Force to always use the detailed registration checks, as we have
         * ^SYSINFO for that */
        results->skip_detailed_registration_state = FALSE;
        g_task_return_pointer (task, results, g_free);
    }
    g_object_unref (task);
}

static void
setup_registration_checks (MMIfaceModemCdma    *self,
                           GAsyncReadyCallback  callback,
                           gpointer             user_data)
{
    /* Run parent's checks first */
    iface_modem_cdma_parent->setup_registration_checks (
        self,
        (GAsyncReadyCallback)parent_setup_registration_checks_ready,
        g_task_new (self, NULL, callback, user_data));
}

/*****************************************************************************/
/* Detailed registration state (CDMA interface) */

typedef struct {
    MMModemCdmaRegistrationState detailed_cdma1x_state;
    MMModemCdmaRegistrationState detailed_evdo_state;
} DetailedRegistrationStateResults;

static gboolean
get_detailed_registration_state_finish (MMIfaceModemCdma              *self,
                                        GAsyncResult                  *res,
                                        MMModemCdmaRegistrationState  *detailed_cdma1x_state,
                                        MMModemCdmaRegistrationState  *detailed_evdo_state,
                                        GError                       **error)
{
    DetailedRegistrationStateResults *results;

    results = g_task_propagate_pointer (G_TASK (res), error);
    if (!results)
        return FALSE;

    *detailed_cdma1x_state = results->detailed_cdma1x_state;
    *detailed_evdo_state   = results->detailed_evdo_state;
    g_free (results);
    return TRUE;
}

static void
sysinfo_ready (MMBaseModem  *self,
               GAsyncResult *res,
               GTask        *task)

{
    DetailedRegistrationStateResults *ctx;
    DetailedRegistrationStateResults *results;
    const gchar                      *response;
    GRegex                           *r;
    GMatchInfo                       *match_info;
    MMModemCdmaRegistrationState      reg_state;
    guint                             val = 0;

    ctx = g_task_get_task_data (task);

    /* Set input detailed states as fallback */
    results = g_memdup (ctx, sizeof (*ctx));

    /* If error, leave superclass' reg state alone if AT^SYSINFO isn't supported. */
    response = mm_base_modem_at_command_finish (self, res, NULL);
    if (!response)
        goto out;

    response = mm_strip_tag (response, "^SYSINFO:");

    /* Format is "<srv_status>,<srv_domain>,<roam_status>,<sys_mode>,<sim_state>" */
    r = g_regex_new ("\\s*(\\d+)\\s*,\\s*(\\d+)\\s*,\\s*(\\d+)\\s*,\\s*(\\d+)\\s*,\\s*(\\d+)",
                     G_REGEX_RAW | G_REGEX_OPTIMIZE, 0, NULL);
    g_assert (r != NULL);

    /* Try to parse the results */
    g_regex_match (r, response, 0, &match_info);
    if (g_match_info_get_match_count (match_info) < 6) {
        mm_obj_warn (self, "failed to parse ^SYSINFO response: '%s'", response);
        goto out;
    }

    /* At this point the generic code already knows we've been registered */
    reg_state = MM_MODEM_CDMA_REGISTRATION_STATE_REGISTERED;

    if (mm_get_uint_from_match_info (match_info, 1, &val)) {
        if (val == 2) {
            /* Service available, check roaming state */
            val = 0;
            if (mm_get_uint_from_match_info (match_info, 3, &val)) {
                if (val == 0)
                    reg_state = MM_MODEM_CDMA_REGISTRATION_STATE_HOME;
                else if (val == 1)
                    reg_state = MM_MODEM_CDMA_REGISTRATION_STATE_ROAMING;
            }
        }
    }

    /* Check service type */
    val = 0;
    if (mm_get_uint_from_match_info (match_info, 4, &val)) {
        if (val == 2) /* CDMA */
            results->detailed_cdma1x_state = reg_state;
        else if (val == 4) /* HDR */
            results->detailed_evdo_state = reg_state;
        else if (val == 8) { /* Hybrid */
            results->detailed_cdma1x_state = reg_state;
            results->detailed_evdo_state = reg_state;
        }
    } else {
        /* Say we're registered to something even though sysmode parsing failed */
        mm_obj_dbg (self, "SYSMODE parsing failed: assuming registered at least in CDMA1x");
        results->detailed_cdma1x_state = reg_state;
    }

    g_match_info_free (match_info);
    g_regex_unref (r);

out:
    g_task_return_pointer (task, results, NULL);
    g_object_unref (task);
}

static void
get_detailed_registration_state (MMIfaceModemCdma             *self,
                                 MMModemCdmaRegistrationState  cdma1x_state,
                                 MMModemCdmaRegistrationState  evdo_state,
                                 GAsyncReadyCallback           callback,
                                 gpointer                      user_data)
{
    GTask                            *task;
    DetailedRegistrationStateResults *ctx;

    /* Setup context */
    ctx = g_new0 (DetailedRegistrationStateResults, 1);
    ctx->detailed_cdma1x_state = cdma1x_state;
    ctx->detailed_evdo_state   = evdo_state;

    task = g_task_new (self, NULL, callback, user_data);
    g_task_set_task_data (task, ctx, g_free);

    mm_base_modem_at_command (MM_BASE_MODEM (self),
                              "^SYSINFO",
                              3,
                              FALSE,
                              (GAsyncReadyCallback)sysinfo_ready,
                              task);
}

/*****************************************************************************/
/* Setup/Cleanup unsolicited events (CDMA interface) */

static void
handle_evdo_quality_change (MMPortSerialAt *port,
                            GMatchInfo *match_info,
                            MMBroadbandModemVia *self)
{
    guint quality = 0;

    if (mm_get_uint_from_match_info (match_info, 1, &quality)) {
        quality = MM_CLAMP_HIGH (quality, 100);
        mm_obj_dbg (self, "EVDO signal quality: %u", quality);
        mm_iface_modem_update_signal_quality (MM_IFACE_MODEM (self), quality);
    }
}

static void
set_unsolicited_events_handlers (MMBroadbandModemVia *self,
                                 gboolean enable)
{
    MMPortSerialAt *ports[2];
    guint i;

    ports[0] = mm_base_modem_peek_port_primary (MM_BASE_MODEM (self));
    ports[1] = mm_base_modem_peek_port_secondary (MM_BASE_MODEM (self));

    /* Enable unsolicited events in given port */
    for (i = 0; i < G_N_ELEMENTS (ports); i++) {
        if (!ports[i])
            continue;

        /* Signal quality related */
        mm_port_serial_at_add_unsolicited_msg_handler (
            ports[i],
            self->priv->hrssilvl_regex,
            enable ? (MMPortSerialAtUnsolicitedMsgFn)handle_evdo_quality_change : NULL,
            enable ? self : NULL,
            NULL);
    }
}

static gboolean
modem_cdma_setup_cleanup_unsolicited_events_finish (MMIfaceModemCdma *self,
                                                    GAsyncResult *res,
                                                    GError **error)
{
    return g_task_propagate_boolean (G_TASK (res), error);
}

static void
parent_cdma_setup_unsolicited_events_ready (MMIfaceModemCdma *self,
                                            GAsyncResult *res,
                                            GTask *task)
{
    GError *error = NULL;

    if (!iface_modem_cdma_parent->setup_unsolicited_events_finish (self, res, &error))
        g_task_return_error (task, error);
    else {
        /* Our own setup now */
        set_unsolicited_events_handlers (MM_BROADBAND_MODEM_VIA (self), TRUE);
        g_task_return_boolean (task, TRUE);
    }
    g_object_unref (task);
}

static void
modem_cdma_setup_unsolicited_events (MMIfaceModemCdma *self,
                                     GAsyncReadyCallback callback,
                                     gpointer user_data)
{
    /* Chain up parent's setup */
    iface_modem_cdma_parent->setup_unsolicited_events (
        self,
        (GAsyncReadyCallback)parent_cdma_setup_unsolicited_events_ready,
        g_task_new (self, NULL, callback, user_data));
}

static void
parent_cdma_cleanup_unsolicited_events_ready (MMIfaceModemCdma *self,
                                              GAsyncResult *res,
                                              GTask *task)
{
    GError *error = NULL;

    if (!iface_modem_cdma_parent->cleanup_unsolicited_events_finish (self, res, &error))
        g_task_return_error (task, error);
    else
        g_task_return_boolean (task, TRUE);

    g_object_unref (task);
}

static void
modem_cdma_cleanup_unsolicited_events (MMIfaceModemCdma *self,
                                       GAsyncReadyCallback callback,
                                       gpointer user_data)
{
    /* Our own cleanup first */
    set_unsolicited_events_handlers (MM_BROADBAND_MODEM_VIA (self), FALSE);

    /* And now chain up parent's cleanup */
    iface_modem_cdma_parent->cleanup_unsolicited_events (
        self,
        (GAsyncReadyCallback)parent_cdma_cleanup_unsolicited_events_ready,
        g_task_new (self, NULL, callback, user_data));
}

/*****************************************************************************/
/* Setup ports (Broadband modem class) */

static void
set_ignored_unsolicited_events_handlers (MMBroadbandModemVia *self)
{
    MMPortSerialAt *ports[2];
    guint i;

    ports[0] = mm_base_modem_peek_port_primary (MM_BASE_MODEM (self));
    ports[1] = mm_base_modem_peek_port_secondary (MM_BASE_MODEM (self));

    /* Enable unsolicited events in given port */
    for (i = 0; i < G_N_ELEMENTS (ports); i++) {
        if (!ports[i])
            continue;

        mm_port_serial_at_add_unsolicited_msg_handler (
            ports[i],
            self->priv->mode_regex,
            NULL, NULL, NULL);
        mm_port_serial_at_add_unsolicited_msg_handler (
            ports[i],
            self->priv->dosession_regex,
            NULL, NULL, NULL);
        mm_port_serial_at_add_unsolicited_msg_handler (
            ports[i],
            self->priv->simst_regex,
            NULL, NULL, NULL);
        mm_port_serial_at_add_unsolicited_msg_handler (
            ports[i],
            self->priv->vpon_regex,
            NULL, NULL, NULL);
        mm_port_serial_at_add_unsolicited_msg_handler (
            ports[i],
            self->priv->creg_regex,
            NULL, NULL, NULL);
        mm_port_serial_at_add_unsolicited_msg_handler (
            ports[i],
            self->priv->vrom_regex,
            NULL, NULL, NULL);
        mm_port_serial_at_add_unsolicited_msg_handler (
            ports[i],
            self->priv->vser_regex,
            NULL, NULL, NULL);
        mm_port_serial_at_add_unsolicited_msg_handler (
            ports[i],
            self->priv->ciev_regex,
            NULL, NULL, NULL);
        mm_port_serial_at_add_unsolicited_msg_handler (
            ports[i],
            self->priv->vpup_regex,
            NULL, NULL, NULL);
    }
}

static void
setup_ports (MMBroadbandModem *self)
{
    /* Call parent's setup ports first always */
    MM_BROADBAND_MODEM_CLASS (mm_broadband_modem_via_parent_class)->setup_ports (self);

    /* Unsolicited messages to always ignore */
    set_ignored_unsolicited_events_handlers (MM_BROADBAND_MODEM_VIA (self));

    /* Now reset the unsolicited messages we'll handle when enabled */
    set_unsolicited_events_handlers (MM_BROADBAND_MODEM_VIA (self), FALSE);
}

/*****************************************************************************/

MMBroadbandModemVia *
mm_broadband_modem_via_new (const gchar *device,
                            const gchar **drivers,
                            const gchar *plugin,
                            guint16 vendor_id,
                            guint16 product_id)
{
    return g_object_new (MM_TYPE_BROADBAND_MODEM_VIA,
                         MM_BASE_MODEM_DEVICE, device,
                         MM_BASE_MODEM_DRIVERS, drivers,
                         MM_BASE_MODEM_PLUGIN, plugin,
                         MM_BASE_MODEM_VENDOR_ID, vendor_id,
                         MM_BASE_MODEM_PRODUCT_ID, product_id,
                         NULL);
}

static void
mm_broadband_modem_via_init (MMBroadbandModemVia *self)
{
    /* Initialize private data */
    self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
                                              MM_TYPE_BROADBAND_MODEM_VIA,
                                              MMBroadbandModemViaPrivate);

    /* Prepare regular expressions to setup */
    self->priv->hrssilvl_regex = g_regex_new ("\\r\\n\\^HRSSILVL:(.*)\\r\\n",
                                              G_REGEX_RAW | G_REGEX_OPTIMIZE, 0, NULL);
    self->priv->mode_regex = g_regex_new ("\\r\\n\\^MODE:(.*)\\r\\n",
                                          G_REGEX_RAW | G_REGEX_OPTIMIZE, 0, NULL);
    self->priv->dosession_regex = g_regex_new ("\\r\\n\\+DOSESSION:(.*)\\r\\n",
                                               G_REGEX_RAW | G_REGEX_OPTIMIZE, 0, NULL);
    self->priv->simst_regex = g_regex_new ("\\r\\n\\^SIMST:(.*)\\r\\n",
                                           G_REGEX_RAW | G_REGEX_OPTIMIZE, 0, NULL);
    self->priv->simst_regex = g_regex_new ("\\r\\n\\+VPON:(.*)\\r\\n",
                                           G_REGEX_RAW | G_REGEX_OPTIMIZE, 0, NULL);
    self->priv->creg_regex = g_regex_new ("\\r\\n\\+CREG:(.*)\\r\\n",
                                          G_REGEX_RAW | G_REGEX_OPTIMIZE, 0, NULL);
    self->priv->vrom_regex = g_regex_new ("\\r\\n\\+VROM:(.*)\\r\\n",
                                          G_REGEX_RAW | G_REGEX_OPTIMIZE, 0, NULL);
    self->priv->vser_regex = g_regex_new ("\\r\\n\\+VSER:(.*)\\r\\n",
                                          G_REGEX_RAW | G_REGEX_OPTIMIZE, 0, NULL);
    self->priv->ciev_regex = g_regex_new ("\\r\\n\\+CIEV:(.*)\\r\\n",
                                          G_REGEX_RAW | G_REGEX_OPTIMIZE, 0, NULL);
    self->priv->vpup_regex = g_regex_new ("\\r\\n\\+VPUP:(.*)\\r\\n",
                                          G_REGEX_RAW | G_REGEX_OPTIMIZE, 0, NULL);
}

static void
finalize (GObject *object)
{
    MMBroadbandModemVia *self = MM_BROADBAND_MODEM_VIA (object);

    g_regex_unref (self->priv->hrssilvl_regex);
    g_regex_unref (self->priv->mode_regex);
    g_regex_unref (self->priv->dosession_regex);
    g_regex_unref (self->priv->simst_regex);
    g_regex_unref (self->priv->simst_regex);
    g_regex_unref (self->priv->creg_regex);
    g_regex_unref (self->priv->vrom_regex);
    g_regex_unref (self->priv->vser_regex);
    g_regex_unref (self->priv->ciev_regex);
    g_regex_unref (self->priv->vpup_regex);

    G_OBJECT_CLASS (mm_broadband_modem_via_parent_class)->finalize (object);
}

static void
iface_modem_cdma_init (MMIfaceModemCdma *iface)
{
    iface_modem_cdma_parent = g_type_interface_peek_parent (iface);

    iface->setup_unsolicited_events = modem_cdma_setup_unsolicited_events;
    iface->setup_unsolicited_events_finish = modem_cdma_setup_cleanup_unsolicited_events_finish;
    iface->cleanup_unsolicited_events = modem_cdma_cleanup_unsolicited_events;
    iface->cleanup_unsolicited_events_finish = modem_cdma_setup_cleanup_unsolicited_events_finish;
    iface->setup_registration_checks = setup_registration_checks;
    iface->setup_registration_checks_finish = setup_registration_checks_finish;
    iface->get_detailed_registration_state = get_detailed_registration_state;
    iface->get_detailed_registration_state_finish = get_detailed_registration_state_finish;
}

static void
mm_broadband_modem_via_class_init (MMBroadbandModemViaClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    MMBroadbandModemClass *broadband_modem_class = MM_BROADBAND_MODEM_CLASS (klass);

    g_type_class_add_private (object_class, sizeof (MMBroadbandModemViaPrivate));

    object_class->finalize = finalize;
    broadband_modem_class->setup_ports = setup_ports;
}

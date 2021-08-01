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
 * Copyright (C) 2008 - 2009 Novell, Inc.
 * Copyright (C) 2009 - 2012 Red Hat, Inc.
 * Copyright (C) 2012 Aleksander Morgado <aleksander@gnu.org>
 */

#include <config.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>

#include "ModemManager.h"
#include "mm-errors-types.h"
#include "mm-modem-helpers.h"
#include "mm-base-modem-at.h"
#include "mm-iface-modem.h"
#include "mm-iface-modem-3gpp.h"
#include "mm-common-zte.h"
#include "mm-broadband-modem-zte.h"

static void iface_modem_init (MMIfaceModem *iface);
static void iface_modem_3gpp_init (MMIfaceModem3gpp *iface);

static MMIfaceModem *iface_modem_parent;
static MMIfaceModem3gpp *iface_modem_3gpp_parent;

G_DEFINE_TYPE_EXTENDED (MMBroadbandModemZte, mm_broadband_modem_zte, MM_TYPE_BROADBAND_MODEM, 0,
                        G_IMPLEMENT_INTERFACE (MM_TYPE_IFACE_MODEM, iface_modem_init)
                        G_IMPLEMENT_INTERFACE (MM_TYPE_IFACE_MODEM_3GPP, iface_modem_3gpp_init));

struct _MMBroadbandModemZtePrivate {
    /* Unsolicited messaging setup */
    MMCommonZteUnsolicitedSetup *unsolicited_setup;
};

/*****************************************************************************/
/* Unlock retries (Modem interface) */

static MMUnlockRetries *
load_unlock_retries_finish (MMIfaceModem *self,
                            GAsyncResult *res,
                            GError **error)
{
    return g_task_propagate_pointer (G_TASK (res), error);
}

static void
load_unlock_retries_ready (MMBaseModem *self,
                           GAsyncResult *res,
                           GTask *task)
{
    const gchar *response;
    GError *error = NULL;
    gint pin1, puk1;

    response = mm_base_modem_at_command_finish (MM_BASE_MODEM (self), res, &error);
    if (!response) {
        g_task_return_error (task, error);
        g_object_unref (task);
        return;
    }

    response = mm_strip_tag (response, "+ZPINPUK:");
    if (sscanf (response, "%d,%d", &pin1, &puk1) == 2) {
        MMUnlockRetries *retries;

        retries = mm_unlock_retries_new ();
        mm_unlock_retries_set (retries, MM_MODEM_LOCK_SIM_PIN, pin1);
        mm_unlock_retries_set (retries, MM_MODEM_LOCK_SIM_PUK, puk1);
        g_task_return_pointer (task, retries, g_object_unref);
    } else {
        g_task_return_new_error (task,
                                 MM_CORE_ERROR,
                                 MM_CORE_ERROR_FAILED,
                                 "Invalid unlock retries response: '%s'",
                                 response);
    }
    g_object_unref (task);
}

static void
load_unlock_retries (MMIfaceModem *self,
                     GAsyncReadyCallback callback,
                     gpointer user_data)
{
    mm_base_modem_at_command (
        MM_BASE_MODEM (self),
        "+ZPINPUK=?",
        3,
        FALSE,
        (GAsyncReadyCallback)load_unlock_retries_ready,
        g_task_new (self, NULL, callback, user_data));
}

/*****************************************************************************/
/* After SIM unlock (Modem interface) */

typedef struct {
    guint retries;
} ModemAfterSimUnlockContext;

static gboolean
modem_after_sim_unlock_finish (MMIfaceModem *self,
                               GAsyncResult *res,
                               GError **error)
{
    return g_task_propagate_boolean (G_TASK (res), error);
}

static void modem_after_sim_unlock_context_step (GTask *task);

static gboolean
cpms_timeout_cb (GTask *task)
{
    ModemAfterSimUnlockContext *ctx;

    ctx = g_task_get_task_data (task);
    ctx->retries--;
    modem_after_sim_unlock_context_step (task);
    return G_SOURCE_REMOVE;
}

static void
cpms_try_ready (MMBaseModem *self,
                GAsyncResult *res,
                GTask *task)
{
    GError *error = NULL;

    if (!mm_base_modem_at_command_finish (self, res, &error) &&
        g_error_matches (error,
                         MM_MOBILE_EQUIPMENT_ERROR,
                         MM_MOBILE_EQUIPMENT_ERROR_SIM_BUSY)) {
            /* Retry in 2 seconds */
        g_timeout_add_seconds (2, (GSourceFunc)cpms_timeout_cb, task);
        g_error_free (error);
        return;
    }

    if (error)
        g_error_free (error);

    /* Well, we're done */
    g_task_return_boolean (task, TRUE);
    g_object_unref (task);
}

static void
modem_after_sim_unlock_context_step (GTask *task)
{
    MMBroadbandModemZte *self;
    ModemAfterSimUnlockContext *ctx;

    self = g_task_get_source_object (task);
    ctx = g_task_get_task_data (task);

    if (ctx->retries == 0) {
        /* Well... just return without error */
        g_task_return_new_error (
            task,
            MM_CORE_ERROR,
            MM_CORE_ERROR_FAILED,
            "Consumed all attempts to wait for SIM not being busy");
        g_object_unref (task);
        return;
    }

    mm_base_modem_at_command (MM_BASE_MODEM (self),
                              "+CPMS?",
                              3,
                              FALSE,
                              (GAsyncReadyCallback)cpms_try_ready,
                              task);
}

static gboolean
after_sim_unlock_wait_cb (GTask *task)
{
    /* Attempt to disable floods of "+ZUSIMR:2" unsolicited responses that
     * eventually fill up the device's buffers and make it crash.  Normally
     * done during probing, but if the device has a PIN enabled it won't
     * accept the +CPMS? during the probe and we have to do it here.
     */
    modem_after_sim_unlock_context_step (task);

    return G_SOURCE_REMOVE;
}

static void
modem_after_sim_unlock (MMIfaceModem *self,
                        GAsyncReadyCallback callback,
                        gpointer user_data)
{
    ModemAfterSimUnlockContext *ctx;
    GTask *task;

    ctx = g_new (ModemAfterSimUnlockContext, 1);
    ctx->retries = 3;

    task = g_task_new (self, NULL, callback, user_data);
    g_task_set_task_data (task, ctx, g_free);

    g_timeout_add_seconds (1, (GSourceFunc)after_sim_unlock_wait_cb, task);
}

/*****************************************************************************/
/* Modem power down (Modem interface) */

static gboolean
modem_power_down_finish (MMIfaceModem *self,
                         GAsyncResult *res,
                         GError **error)
{
    return !!mm_base_modem_at_command_finish (MM_BASE_MODEM (self), res, error);
}

static void
modem_power_down (MMIfaceModem *self,
                  GAsyncReadyCallback callback,
                  gpointer user_data)
{
    /* Use AT+CFUN=4 for power down. It will stop the RF (IMSI detach), and
     * keeps access to the SIM */
    mm_base_modem_at_command (MM_BASE_MODEM (self),
                              "+CFUN=4",
                              3,
                              FALSE,
                              callback,
                              user_data);
}

/*****************************************************************************/
/* Load supported modes (Modem interface) */

static GArray *
load_supported_modes_finish (MMIfaceModem *self,
                             GAsyncResult *res,
                             GError **error)
{
    return g_task_propagate_pointer (G_TASK (res), error);
}

static void
parent_load_supported_modes_ready (MMIfaceModem *self,
                                   GAsyncResult *res,
                                   GTask *task)
{
    GError *error = NULL;
    GArray *all;
    GArray *combinations;
    GArray *filtered;
    MMModemModeCombination mode;

    all = iface_modem_parent->load_supported_modes_finish (self, res, &error);
    if (!all) {
        g_task_return_error (task, error);
        g_object_unref (task);
        return;
    }

    /* Build list of combinations */
    combinations = g_array_sized_new (FALSE, FALSE, sizeof (MMModemModeCombination), 5);

    /* 2G only */
    mode.allowed = MM_MODEM_MODE_2G;
    mode.preferred = MM_MODEM_MODE_NONE;
    g_array_append_val (combinations, mode);
    /* 3G only */
    mode.allowed = MM_MODEM_MODE_3G;
    mode.preferred = MM_MODEM_MODE_NONE;
    g_array_append_val (combinations, mode);

    if (!mm_iface_modem_is_3gpp_lte (self)) {
        /* 2G and 3G */
        mode.allowed = (MM_MODEM_MODE_2G | MM_MODEM_MODE_3G);
        mode.preferred = MM_MODEM_MODE_NONE;
        g_array_append_val (combinations, mode);
        /* 2G and 3G, 2G preferred */
        mode.allowed = (MM_MODEM_MODE_2G | MM_MODEM_MODE_3G);
        mode.preferred = MM_MODEM_MODE_2G;
        g_array_append_val (combinations, mode);
        /* 2G and 3G, 3G preferred */
        mode.allowed = (MM_MODEM_MODE_2G | MM_MODEM_MODE_3G);
        mode.preferred = MM_MODEM_MODE_3G;
        g_array_append_val (combinations, mode);
    } else {
        /* 4G only */
        mode.allowed = MM_MODEM_MODE_4G;
        mode.preferred = MM_MODEM_MODE_NONE;
        g_array_append_val (combinations, mode);
        /* 2G, 3G and 4G */
        mode.allowed = (MM_MODEM_MODE_2G | MM_MODEM_MODE_3G | MM_MODEM_MODE_4G);
        mode.preferred = MM_MODEM_MODE_NONE;
        g_array_append_val (combinations, mode);
    }

    /* Filter out those unsupported modes */
    filtered = mm_filter_supported_modes (all, combinations, self);
    g_array_unref (all);
    g_array_unref (combinations);

    g_task_return_pointer (task, filtered, (GDestroyNotify) g_array_unref);
    g_object_unref (task);
}

static void
load_supported_modes (MMIfaceModem *self,
                      GAsyncReadyCallback callback,
                      gpointer user_data)
{
    /* Run parent's loading */
    iface_modem_parent->load_supported_modes (
        MM_IFACE_MODEM (self),
        (GAsyncReadyCallback)parent_load_supported_modes_ready,
        g_task_new (self, NULL, callback, user_data));
}

/*****************************************************************************/
/* Load initial allowed/preferred modes (Modem interface) */

static gboolean
load_current_modes_finish (MMIfaceModem *self,
                           GAsyncResult *res,
                           MMModemMode *allowed,
                           MMModemMode *preferred,
                           GError **error)
{
    const gchar *response;
    GMatchInfo *match_info = NULL;
    GRegex *r;
    gint cm_mode = -1;
    gint pref_acq = -1;
    gboolean result;
    GError *match_error = NULL;

    response = mm_base_modem_at_command_finish (MM_BASE_MODEM (self), res, error);
    if (!response)
        return FALSE;

    r = g_regex_new ("\\+ZSNT:\\s*(\\d),(\\d),(\\d)", G_REGEX_UNGREEDY, 0, error);
    g_assert (r != NULL);

    result = FALSE;
    if (!g_regex_match_full (r, response, strlen (response), 0, 0, &match_info, &match_error)) {
        if (match_error)
            g_propagate_error (error, match_error);
        else
            g_set_error (error,
                         MM_CORE_ERROR,
                         MM_CORE_ERROR_FAILED,
                         "Couldn't parse +ZSNT response: '%s'",
                         response);
        goto done;
    }

    if (!mm_get_int_from_match_info (match_info, 1, &cm_mode) ||
        cm_mode < 0 || (cm_mode > 2 && cm_mode != 6) ||
        !mm_get_int_from_match_info (match_info, 3, &pref_acq) ||
        pref_acq < 0 || pref_acq > 2) {
        g_set_error (error,
                     MM_CORE_ERROR,
                     MM_CORE_ERROR_FAILED,
                     "Failed to parse the allowed mode response: '%s'",
                     response);
        goto done;
    }

    /* Correctly parsed! */
    result = TRUE;
    if (cm_mode == 0) {
        /* Both 2G, 3G and LTE allowed. For LTE modems, no 2G/3G preference supported. */
        if (pref_acq == 0 || mm_iface_modem_is_3gpp_lte (self)) {
            /* Any allowed */
            *allowed = MM_MODEM_MODE_ANY;
            *preferred = MM_MODEM_MODE_NONE;
        } else if (pref_acq == 1) {
            *allowed = (MM_MODEM_MODE_2G | MM_MODEM_MODE_3G);
            *preferred = MM_MODEM_MODE_2G;
        } else if (pref_acq == 2) {
            *allowed = (MM_MODEM_MODE_2G | MM_MODEM_MODE_3G);
            *preferred = MM_MODEM_MODE_3G;
        } else
            g_assert_not_reached ();
    } else if (cm_mode == 1) {
        /* GSM only */
        *allowed = MM_MODEM_MODE_2G;
        *preferred = MM_MODEM_MODE_NONE;
    } else if (cm_mode == 2) {
        /* WCDMA only */
        *allowed = MM_MODEM_MODE_3G;
        *preferred = MM_MODEM_MODE_NONE;
    } else if (cm_mode == 6) {
        /* LTE only */
        *allowed = MM_MODEM_MODE_4G;
        *preferred = MM_MODEM_MODE_NONE;
    } else
        g_assert_not_reached ();

done:
    g_match_info_free (match_info);
    if (r)
        g_regex_unref (r);

    return result;
}

static void
load_current_modes (MMIfaceModem *self,
                    GAsyncReadyCallback callback,
                    gpointer user_data)
{
    mm_base_modem_at_command (MM_BASE_MODEM (self),
                              "+ZSNT?",
                              3,
                              FALSE,
                              callback,
                              user_data);
}

/*****************************************************************************/
/* Set allowed modes (Modem interface) */

static gboolean
set_current_modes_finish (MMIfaceModem *self,
                          GAsyncResult *res,
                          GError **error)
{
    return g_task_propagate_boolean (G_TASK (res), error);
}

static void
allowed_mode_update_ready (MMBroadbandModemZte *self,
                           GAsyncResult *res,
                           GTask *task)
{
    GError *error = NULL;

    mm_base_modem_at_command_finish (MM_BASE_MODEM (self), res, &error);

    if (error)
        /* Let the error be critical. */
        g_task_return_error (task, error);
    else
        g_task_return_boolean (task, TRUE);
    g_object_unref (task);
}

static void
set_current_modes (MMIfaceModem *self,
                   MMModemMode allowed,
                   MMModemMode preferred,
                   GAsyncReadyCallback callback,
                   gpointer user_data)
{
    GTask *task;
    gchar *command;
    gint cm_mode = -1;
    gint pref_acq = -1;

    task = g_task_new (self, NULL, callback, user_data);

    if (allowed == MM_MODEM_MODE_2G) {
        cm_mode = 1;
        pref_acq = 0;
    } else if (allowed == MM_MODEM_MODE_3G) {
        cm_mode = 2;
        pref_acq = 0;
    } else if (allowed == (MM_MODEM_MODE_2G | MM_MODEM_MODE_3G)
               && !mm_iface_modem_is_3gpp_lte (self)) { /* LTE models do not support 2G|3G mode */
        cm_mode = 0;
        if (preferred == MM_MODEM_MODE_2G)
            pref_acq = 1;
        else if (preferred == MM_MODEM_MODE_3G)
            pref_acq = 2;
        else /* none preferred, so AUTO */
            pref_acq = 0;
    } else if (allowed == (MM_MODEM_MODE_2G | MM_MODEM_MODE_3G | MM_MODEM_MODE_4G) &&
               preferred == MM_MODEM_MODE_NONE) {
        cm_mode = 0;
        pref_acq = 0;
    } else if (allowed == MM_MODEM_MODE_ANY &&
               preferred == MM_MODEM_MODE_NONE) {
        cm_mode = 0;
        pref_acq = 0;
    } else if (allowed == MM_MODEM_MODE_4G) {
        cm_mode = 6;
        pref_acq = 0;
    }

    if (cm_mode < 0 || pref_acq < 0) {
        gchar *allowed_str;
        gchar *preferred_str;

        allowed_str = mm_modem_mode_build_string_from_mask (allowed);
        preferred_str = mm_modem_mode_build_string_from_mask (preferred);
        g_task_return_new_error (task,
                                 MM_CORE_ERROR,
                                 MM_CORE_ERROR_FAILED,
                                 "Requested mode (allowed: '%s', preferred: '%s') not "
                                 "supported by the modem.",
                                 allowed_str,
                                 preferred_str);
        g_object_unref (task);

        g_free (allowed_str);
        g_free (preferred_str);
        return;
    }

    command = g_strdup_printf ("AT+ZSNT=%d,0,%d", cm_mode, pref_acq);
    mm_base_modem_at_command (
        MM_BASE_MODEM (self),
        command,
        3,
        FALSE,
        (GAsyncReadyCallback)allowed_mode_update_ready,
        task);
    g_free (command);
}

/*****************************************************************************/
/* Load access technology (Modem interface) */

static gboolean
load_access_technologies_finish (MMIfaceModem *self,
                                 GAsyncResult *res,
                                 MMModemAccessTechnology *access_technologies,
                                 guint *mask,
                                 GError **error)
{
    const gchar *response;

    /* CDMA-only devices run parent access technology checks */
    if (mm_iface_modem_is_cdma_only (self)) {
        return iface_modem_parent->load_access_technologies_finish (self,
                                                                    res,
                                                                    access_technologies,
                                                                    mask,
                                                                    error);
    }

    /* Otherwise process and handle +ZPAS response from 3GPP devices */
    response = mm_base_modem_at_command_finish (MM_BASE_MODEM (self), res, error);
    if (!response)
        return FALSE;

    /* Sample response from an MF626:
     *   +ZPAS: "GPRS/EDGE","CS_ONLY"
     */
    response = mm_strip_tag (response, "+ZPAS:");
    *access_technologies = mm_string_to_access_tech (response);
    *mask = MM_IFACE_MODEM_3GPP_ALL_ACCESS_TECHNOLOGIES_MASK;
    return TRUE;
}

static void
load_access_technologies (MMIfaceModem *self,
                          GAsyncReadyCallback callback,
                          gpointer user_data)
{

    /* CDMA modems don't support ZPAS and thus run parent's access technology
     * loading. */
    if (mm_iface_modem_is_cdma_only (self)) {
        iface_modem_parent->load_access_technologies (self, callback, user_data);
        return;
    }

    mm_base_modem_at_command (MM_BASE_MODEM (self),
                              "+ZPAS?",
                              3,
                              FALSE,
                              callback,
                              user_data);
}

/*****************************************************************************/
/* Setup/Cleanup unsolicited events (3GPP interface) */

static gboolean
modem_3gpp_setup_cleanup_unsolicited_events_finish (MMIfaceModem3gpp *self,
                                                    GAsyncResult *res,
                                                    GError **error)
{
    return g_task_propagate_boolean (G_TASK (res), error);
}

static void
parent_setup_unsolicited_events_ready (MMIfaceModem3gpp *self,
                                       GAsyncResult *res,
                                       GTask *task)
{
    GError *error = NULL;

    if (!iface_modem_3gpp_parent->setup_unsolicited_events_finish (self, res, &error))
        g_task_return_error (task, error);
    else {
        /* Our own setup now */
        mm_common_zte_set_unsolicited_events_handlers (MM_BROADBAND_MODEM (self),
                                                       MM_BROADBAND_MODEM_ZTE (self)->priv->unsolicited_setup,
                                                       TRUE);
        g_task_return_boolean (task, TRUE);
    }
    g_object_unref (task);
}

static void
modem_3gpp_setup_unsolicited_events (MMIfaceModem3gpp *self,
                                     GAsyncReadyCallback callback,
                                     gpointer user_data)
{
    GTask *task;

    task = g_task_new (self, NULL, callback, user_data);

    /* Chain up parent's setup */
    iface_modem_3gpp_parent->setup_unsolicited_events (
        self,
        (GAsyncReadyCallback)parent_setup_unsolicited_events_ready,
        task);
}

static void
parent_cleanup_unsolicited_events_ready (MMIfaceModem3gpp *self,
                                         GAsyncResult *res,
                                         GTask *task)
{
    GError *error = NULL;

    if (!iface_modem_3gpp_parent->cleanup_unsolicited_events_finish (self, res, &error))
        g_task_return_error (task, error);
    else
        g_task_return_boolean (task, TRUE);
    g_object_unref (task);
}

static void
modem_3gpp_cleanup_unsolicited_events (MMIfaceModem3gpp *self,
                                       GAsyncReadyCallback callback,
                                       gpointer user_data)
{
    GTask *task;

    task = g_task_new (self, NULL, callback, user_data);

    /* Our own cleanup first */
    mm_common_zte_set_unsolicited_events_handlers (MM_BROADBAND_MODEM (self),
                                                   MM_BROADBAND_MODEM_ZTE (self)->priv->unsolicited_setup,
                                                   FALSE);

    /* And now chain up parent's cleanup */
    iface_modem_3gpp_parent->cleanup_unsolicited_events (
        self,
        (GAsyncReadyCallback)parent_cleanup_unsolicited_events_ready,
        task);
}

/*****************************************************************************/
/* Setup ports (Broadband modem class) */

static void
setup_ports (MMBroadbandModem *self)
{
    /* Call parent's setup ports first always */
    MM_BROADBAND_MODEM_CLASS (mm_broadband_modem_zte_parent_class)->setup_ports (self);

    /* Now reset the unsolicited messages we'll handle when enabled */
    mm_common_zte_set_unsolicited_events_handlers (MM_BROADBAND_MODEM (self),
                                                   MM_BROADBAND_MODEM_ZTE (self)->priv->unsolicited_setup,
                                                   FALSE);
}

/*****************************************************************************/

MMBroadbandModemZte *
mm_broadband_modem_zte_new (const gchar *device,
                              const gchar **drivers,
                              const gchar *plugin,
                              guint16 vendor_id,
                              guint16 product_id)
{
    return g_object_new (MM_TYPE_BROADBAND_MODEM_ZTE,
                         MM_BASE_MODEM_DEVICE, device,
                         MM_BASE_MODEM_DRIVERS, drivers,
                         MM_BASE_MODEM_PLUGIN, plugin,
                         MM_BASE_MODEM_VENDOR_ID, vendor_id,
                         MM_BASE_MODEM_PRODUCT_ID, product_id,
                         NULL);
}

static void
mm_broadband_modem_zte_init (MMBroadbandModemZte *self)
{
    /* Initialize private data */
    self->priv = G_TYPE_INSTANCE_GET_PRIVATE ((self),
                                              MM_TYPE_BROADBAND_MODEM_ZTE,
                                              MMBroadbandModemZtePrivate);
    self->priv->unsolicited_setup = mm_common_zte_unsolicited_setup_new ();
}

static void
finalize (GObject *object)
{
    MMBroadbandModemZte *self = MM_BROADBAND_MODEM_ZTE (object);

    mm_common_zte_unsolicited_setup_free (self->priv->unsolicited_setup);

    G_OBJECT_CLASS (mm_broadband_modem_zte_parent_class)->finalize (object);
}

static void
iface_modem_init (MMIfaceModem *iface)
{
    iface_modem_parent = g_type_interface_peek_parent (iface);

    iface->modem_after_sim_unlock = modem_after_sim_unlock;
    iface->modem_after_sim_unlock_finish = modem_after_sim_unlock_finish;
    iface->modem_power_down = modem_power_down;
    iface->modem_power_down_finish = modem_power_down_finish;
    iface->load_access_technologies = load_access_technologies;
    iface->load_access_technologies_finish = load_access_technologies_finish;
    iface->load_supported_modes = load_supported_modes;
    iface->load_supported_modes_finish = load_supported_modes_finish;
    iface->load_current_modes = load_current_modes;
    iface->load_current_modes_finish = load_current_modes_finish;
    iface->set_current_modes = set_current_modes;
    iface->set_current_modes_finish = set_current_modes_finish;
    iface->load_unlock_retries = load_unlock_retries;
    iface->load_unlock_retries_finish = load_unlock_retries_finish;
}

static void
iface_modem_3gpp_init (MMIfaceModem3gpp *iface)
{
    iface_modem_3gpp_parent = g_type_interface_peek_parent (iface);

    iface->setup_unsolicited_events = modem_3gpp_setup_unsolicited_events;
    iface->setup_unsolicited_events_finish = modem_3gpp_setup_cleanup_unsolicited_events_finish;
    iface->cleanup_unsolicited_events = modem_3gpp_cleanup_unsolicited_events;
    iface->cleanup_unsolicited_events_finish = modem_3gpp_setup_cleanup_unsolicited_events_finish;
}

static void
mm_broadband_modem_zte_class_init (MMBroadbandModemZteClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    MMBroadbandModemClass *broadband_modem_class = MM_BROADBAND_MODEM_CLASS (klass);

    g_type_class_add_private (object_class, sizeof (MMBroadbandModemZtePrivate));

    object_class->finalize = finalize;
    broadband_modem_class->setup_ports = setup_ports;
}

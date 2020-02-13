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

#include <stdlib.h>
#include <gmodule.h>

#define _LIBMM_INSIDE_MM
#include <libmm-glib.h>

#include "mm-log.h"
#include "mm-plugin-xmm7360.h"
#include "mm-broadband-modem.h"

G_DEFINE_TYPE (MMPluginXmm7360, mm_plugin_xmm7360, MM_TYPE_PLUGIN)

MM_PLUGIN_DEFINE_MAJOR_VERSION
MM_PLUGIN_DEFINE_MINOR_VERSION

/*****************************************************************************/

static MMBaseModem *
create_modem (MMPlugin *self,
              const gchar *uid,
              const gchar **drivers,
              guint16 vendor,
              guint16 product,
              GList *probes,
              GError **error)
{
    return MM_BASE_MODEM (mm_broadband_modem_new (uid,
                                                  drivers,
                                                  mm_plugin_get_name (self),
                                                  vendor,
                                                  product));
}

/*****************************************************************************/

G_MODULE_EXPORT MMPlugin *
mm_plugin_create (void)
{
    //TODO: check if we need also "tty", "net" here
    static const gchar *subsystems[] = { "pci", NULL };
    static const mm_uint16_pair products[] = {
        { 0x8086, 0x7360 }, /* Intel XMM7360 */
        { 0, 0 }
    };
    static const gchar *drivers[] = { "xmm7360", NULL };

    return MM_PLUGIN (
        g_object_new (MM_TYPE_PLUGIN_XMM7360,
                      MM_PLUGIN_NAME,               "XMM7360",
                      MM_PLUGIN_ALLOWED_SUBSYSTEMS, subsystems,
                      MM_PLUGIN_ALLOWED_PRODUCT_IDS, products,
                      MM_PLUGIN_ALLOWED_DRIVERS,    drivers,
                    //   MM_PLUGIN_ALLOWED_AT,         TRUE,
                    //   MM_PLUGIN_ALLOWED_QCDM,       TRUE,
                    //   MM_PLUGIN_ALLOWED_QMI,        TRUE,
                    //   MM_PLUGIN_ALLOWED_MBIM,       TRUE,
                      NULL));
}

static void
mm_plugin_xmm7360_init (MMPluginXmm7360 *self)
{
}

static void
mm_plugin_xmm7360_class_init (MMPluginXmm7360Class *klass)
{
    MMPluginClass *plugin_class = MM_PLUGIN_CLASS (klass);

    plugin_class->create_modem = create_modem;
}

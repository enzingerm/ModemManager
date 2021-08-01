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

#ifndef MM_PLUGIN_XMM7360_H
#define MM_PLUGIN_XMM7360_H

#include "mm-plugin.h"

#define MM_TYPE_PLUGIN_XMM7360            (mm_plugin_xmm7360_get_type ())
#define MM_PLUGIN_XMM7360(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), MM_TYPE_PLUGIN_XMM7360, MMPluginXmm7360))
#define MM_PLUGIN_XMM7360_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  MM_TYPE_PLUGIN_XMM7360, MMPluginXmm7360Class))
#define MM_IS_PLUGIN_XMM7360(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MM_TYPE_PLUGIN_XMM7360))
#define MM_IS_PLUGIN_XMM7360_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  MM_TYPE_PLUGIN_XMM7360))
#define MM_PLUGIN_XMM7360_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  MM_TYPE_PLUGIN_XMM7360, MMPluginXmm7360Class))

typedef struct {
    MMPlugin parent;
} MMPluginXmm7360;

typedef struct {
    MMPluginClass parent;
} MMPluginXmm7360Class;

GType mm_plugin_xmm7360_get_type (void);

G_MODULE_EXPORT MMPlugin *mm_plugin_create (void);

#endif /* MM_PLUGIN_XMM7360_H */

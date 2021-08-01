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
 * Copyright (C) 2013 Aleksander Morgado <aleksander@gnu.org>
 */

#ifndef MM_SIM_MBIM_H
#define MM_SIM_MBIM_H

#include <glib.h>
#include <glib-object.h>

#include "mm-base-sim.h"

#define MM_TYPE_SIM_MBIM            (mm_sim_mbim_get_type ())
#define MM_SIM_MBIM(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), MM_TYPE_SIM_MBIM, MMSimMbim))
#define MM_SIM_MBIM_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  MM_TYPE_SIM_MBIM, MMSimMbimClass))
#define MM_IS_SIM_MBIM(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MM_TYPE_SIM_MBIM))
#define MM_IS_SIM_MBIM_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  MM_TYPE_SIM_MBIM))
#define MM_SIM_MBIM_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  MM_TYPE_SIM_MBIM, MMSimMbimClass))

typedef struct _MMSimMbim MMSimMbim;
typedef struct _MMSimMbimClass MMSimMbimClass;

struct _MMSimMbim {
    MMBaseSim parent;
};

struct _MMSimMbimClass {
    MMBaseSimClass parent;
};

GType mm_sim_mbim_get_type (void);
G_DEFINE_AUTOPTR_CLEANUP_FUNC (MMSimMbim, g_object_unref)

void       mm_sim_mbim_new        (MMBaseModem *modem,
                                   GCancellable *cancellable,
                                   GAsyncReadyCallback callback,
                                   gpointer user_data);
MMBaseSim *mm_sim_mbim_new_finish (GAsyncResult  *res,
                                   GError       **error);

#endif /* MM_SIM_MBIM_H */

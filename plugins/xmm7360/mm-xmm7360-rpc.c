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



#include <assert.h>
#include "mm-xmm7360-rpc.h"

GByteArray* asn_int4(gint32 val) {
    GByteArray* arr = g_byte_array_sized_new(6);
    gint32 be = GINT32_TO_BE(val);
    g_byte_array_append(arr, (guint8*)"\x02\x04", 2);
    g_byte_array_append(arr, (guint8*)&be, 4);
    return arr;
}

gint get_asn_int(gchar* data, gint len) {
    gint size;
    gint val;
    assert(*data++ == 2);
    assert(len > 2);
    size = *data++;
    len -=2;
    assert(len > size);
    val = 0;
    do {
        val <<= 8;
        val |= *data++;
    } while(--size > 0);

    return val;
}
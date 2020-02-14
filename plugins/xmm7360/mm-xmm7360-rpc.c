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

gint get_asn_int(GBytes* bytes, gsize* current_offset) {
    gint size;
    gint val;
    gsize len;
    const guchar* data = g_bytes_get_data(bytes, &len);
    *current_offset += 2;
    assert(len > *current_offset);
    assert(*data++ == 2);
    size = *data++;
    *current_offset += size;
    assert(len >= *current_offset);
    val = 0;
    do {
        val <<= 8;
        val |= *data++;
    } while(--size > 0);

    g_free(bytes);

    return val;
}

GBytes* get_string(GBytes* bytes, gsize *current_offset) {
    GBytes *ret;
    gsize len;
    guchar valid;
    gint value = 0;
    guchar type;
    gint count;
    gint padding;
    const guchar* data = g_bytes_get_data(bytes, &len);
    *current_offset += 2;
    assert(len > *current_offset);
    type = *data++;
    valid = *data++;

    assert(type == 0x55 || type == 0x56 || type == 0x57);
    if(valid & 0x80) {
        guchar bytelen = valid & 0xf;
        assert(bytelen <= 4);
        *current_offset += bytelen;
        assert(len > *current_offset);
        for (valid = 0; valid < bytelen; valid++) {
            value |= *data++ << (valid * 8);
        }
    }

    //0x56 contains 2-byte chars
    if(type == 0x56) {
        value <<= 1;
    //0x57 contains 4-byte chars
    } else if(type == 0x57) {
        value <<= 2;
    }

    count = get_asn_int(bytes, current_offset);
    padding = get_asn_int(bytes, current_offset);
    
    if(count > 0) {
        assert(count == value + padding);
    }

    *current_offset += (value + padding);
    assert(len >= *current_offset);
    ret = g_bytes_new_from_bytes(bytes, *current_offset, value);
    g_free(bytes);

    return ret;

}
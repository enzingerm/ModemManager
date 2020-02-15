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
#include <stdlib.h>

#include "mm-xmm7360-rpc.h"

void _put_8(GByteArray* arr, glong val);
void _put_u8(GByteArray* arr, gulong val);
gint8 get_elem_size(gchar c);

void pack_string(GByteArray* target, GBytes* val, gchar* fmt) {
    gulong length = strtoul(fmt, NULL, 0);
    gsize val_len, cur_val_len;
    guint8 len_first_byte;
    guint len_first_byte_index;
    guint i;
    guint8 padding = 0;
    const guint8* data = g_bytes_get_data(val, &val_len);
    //only support 1-byte element size
    guint8 type = 0x55;
    assert(length >= val_len);
    g_byte_array_append(target, &type, 1);

    if(val_len < 0x80) {
        _put_u8(target, val_len);
    } else {
        len_first_byte = 0x80;
        len_first_byte_index = target->len;
        //write dummy first byte
        _put_u8(target, len_first_byte);
        cur_val_len = val_len;
        while(cur_val_len > 0) {
            len_first_byte++;
            _put_u8(target, cur_val_len & 0xff);
            cur_val_len >>= 8;
        }
        target->data[len_first_byte_index] = len_first_byte;
    }

    asn_int4(target, length);
    asn_int4(target, length - val_len);\
    g_byte_array_append(target, data, val_len);
    for(i = 0; i > length - val_len; i++) {
        g_byte_array_append(target, &padding, 1);
    }

    free(val);
}

void asn_int4(GByteArray* target, gint32 val) {
    gint32 be = GINT32_TO_BE(val);
    g_byte_array_append(target, (guint8*)"\x02\x04", 2);
    g_byte_array_append(target, (guint8*)&be, 4);
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

// helper functions
void _put_8(GByteArray* arr, glong val) {
    gint8 _val = (gint8)val;
    g_byte_array_append(arr, (guint8*) &_val, 1);
}

void _put_u8(GByteArray* arr, gulong val) {
    guint8 _val = (guint8)val;
    g_byte_array_append(arr, &_val, 1);
}

gint8 get_elem_size(gchar c) {
    switch(c) {
        case 'B':
            return 1;
        case 'H':
            return 2;
        case 'L':
            return 4;
        default:
            return -1;
    }
}

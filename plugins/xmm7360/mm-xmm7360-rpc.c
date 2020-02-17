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
#include <fcntl.h>

#include "mm-xmm7360-rpc.h"
#include "mm-xmm7360-rpc-enums-types.h"

void _put_8(GByteArray* arr, glong val);
void _put_u8(GByteArray* arr, gulong val);
gint8 get_elem_size(gchar c);

int xmm7360_rpc_init(xmm7360_rpc* rpc) {
    int fd = open("/dev/xmm0/rpc", O_RDWR | O_SYNC);
    if(fd < 0) {
        return -1;
    }
    rpc->fd = fd;
    rpc->attach_allowed = FALSE;
    return 0;
}

int xmm7360_rpc_pump(xmm7360_rpc* rpc, gboolean is_async, gboolean have_ack, guint32 tid_word) {
    //TODO: implement
    return 0;
}

int xmm7360_rpc_execute(xmm7360_rpc* rpc, gboolean is_async, GByteArray* body, rpc_message* res_ptr) {
    //TODO: implement
    return 0;
}

int xmm7360_rpc_handle_message(xmm7360_rpc* rpc, GBytes* message, rpc_message* res_ptr) {
    //TODO: implement
    return 0;
}

int xmm7360_rpc_handle_unsolicited(xmm7360_rpc* rpc, rpc_message* message) {
    rpc_arg* attach_argument;

    if(g_strcmp0(
        xmm_7360_rpc_unsol_ids_get_string((Xmm7360RpcUnsolIds)message->code),
        "UtaMsNetIsAttachAllowedIndCb"
    ) == 0) {
        if(message->content->len <= 2) {
            //TODO: print error
            return -1;
        }
        attach_argument = &g_array_index(message->content, rpc_arg, 2);
        rpc->attach_allowed = GET_RPC_INT(attach_argument);
    }
    return 0;
}

GByteArray* pack(guint count, rpc_arg* args) {
    GByteArray* ret = g_byte_array_new();
    guint i;
    gint16 sh;
    gint32 lng;
    for(i = 0; i < count; i++, args++) {
        switch(args->type) {
            case BYTE:
                g_byte_array_append(ret, (guint8*)&args->value.b, 1);
                break;
            case SHORT:
                sh = GINT16_TO_BE(args->value.s);
                g_byte_array_append(ret, (guint8*)&sh, 2);
                break;
            case LONG:
                lng = GINT32_TO_BE(args->value.l);
                g_byte_array_append(ret, (guint8*)&lng, 4);
                break;
            case STRING:
                pack_string(ret, (guint8*)args->value.string, strlen(args->value.string), args->size);
                break;
            default:
                //should be unreachable
                return NULL;
        }
    }
    return ret;
}

void pack_string(GByteArray* target, guint8* data, gsize val_len, guint length) {
    gsize cur_val_len;
    guint8 len_first_byte;
    guint len_first_byte_index;
    guint i;
    guint8 padding = 0;
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

GByteArray* pack_uta_ms_call_ps_connect_req(void) {
    rpc_arg args[] = {
        { .type = BYTE, .value = { .b = 0 } },
        { .type = LONG, .value = { .l = 6 } },
        { .type = LONG, .value = { .l = 0 } },
        { .type = LONG, .value = { .l = 0 } },
    };
    return pack(G_N_ELEMENTS(args), args);
}

GByteArray* pack_uta_ms_call_ps_get_negotiated_dns_req(void) {
    rpc_arg args[] = {
        { .type = BYTE, .value = { .b = 0 } },
        { .type = LONG, .value = { .l = 0 } },
        { .type = LONG, .value = { .l = 0 } },
    };
    return pack(G_N_ELEMENTS(args), args);
}

GByteArray* pack_uta_ms_call_ps_get_get_ip_addr_req(void) {
    rpc_arg args[] = {
        { .type = BYTE, .value = { .b = 0 } },
        { .type = LONG, .value = { .l = 0 } },
        { .type = LONG, .value = { .l = 0 } },
    };
    return pack(G_N_ELEMENTS(args), args);
}

GByteArray* pack_uta_ms_net_attach_req(void) {
    rpc_arg args[] = {
        { .type = BYTE, .value = { .b = 0 } },
        { .type = LONG, .value = { .l = 0 } },
        { .type = LONG, .value = { .l = 0 } },
        { .type = LONG, .value = { .l = 0 } },
        { .type = LONG, .value = { .l = 0 } },
        { .type = SHORT, .value = { .s = 0xffff } },
        { .type = SHORT, .value = { .s = 0xffff } },
        { .type = LONG, .value = { .l = 0 } },
        { .type = LONG, .value = { .l = 0 } },
    };
    return pack(G_N_ELEMENTS(args), args);
}

GByteArray* pack_uta_rpc_ps_connect_to_datachannel_req(void) {
    return pack(1, (rpc_arg[]) {
        {
            .type = STRING,
            .size=24,
            .value = { .string = "/sioscc/PCIE/IOSM/IPS/0" }
        },
    });
}

GByteArray* pack_uta_sys_get_info(gint index) {
    rpc_arg args[] = {
        { .type = LONG, .value = { .l = 0 } },
        { .type = STRING, .size = 0, .value = { .string = "" } },
        { .type = LONG, .value = { .l = index } },
    };
    return pack(G_N_ELEMENTS(args), args);
}

GByteArray* pack_uta_ms_call_ps_attach_apn_config_req(gchar* apn) {
    gchar zeroes[270] = { 0 };
    rpc_arg args[] = {
        {  .type = BYTE, .value = { .b = 0 } },
        {  .type = STRING, .size = 260, .value = { .string = zeroes } },
        {  .type = LONG, .value = { .l = 0 } },
        {  .type = STRING, .size = 66, .value = { .string = zeroes } },
        {  .type = STRING, .size = 65, .value = { .string = zeroes } },
        {  .type = STRING, .size = 250, .value = { .string = zeroes } },
        {  .type = BYTE, .value = { .b = 0 } },
        {  .type = STRING, .size = 252, .value = { .string = zeroes } },
        {  .type = SHORT, .value = { .s = 0 } },
        {  .type = LONG, .value = { .l = 0 } },
        {  .type = LONG, .value = { .l = 0 } },
        {  .type = LONG, .value = { .l = 0 } },
        {  .type = LONG, .value = { .l = 0 } },
        {  .type = LONG, .value = { .l = 0 } },
        {  .type = LONG, .value = { .l = 0 } },
        {  .type = LONG, .value = { .l = 0 } },
        {  .type = LONG, .value = { .l = 0 } },
        {  .type = LONG, .value = { .l = 0 } },
        {  .type = LONG, .value = { .l = 0 } },
        {  .type = LONG, .value = { .l = 0 } },
        {  .type = LONG, .value = { .l = 0 } },
        {  .type = LONG, .value = { .l = 0 } },
        {  .type = LONG, .value = { .l = 0 } },
        {  .type = LONG, .value = { .l = 0 } },
        {  .type = LONG, .value = { .l = 0 } },
        {  .type = LONG, .value = { .l = 0 } },
        {  .type = LONG, .value = { .l = 0 } },
        {  .type = LONG, .value = { .l = 0 } },
        {  .type = LONG, .value = { .l = 0 } },
        {  .type = LONG, .value = { .l = 0 } },
        {  .type = STRING, .size = 20, .value = { .string = zeroes } },
        {  .type = LONG, .value = { .l = 0 } },
        {  .type = STRING, .size = 104, .value = { .string = zeroes } },
        {  .type = STRING, .size = 260, .value = { .string = zeroes } },
        {  .type = LONG, .value = { .l = 0 } },
        {  .type = STRING, .size = 66, .value = { .string = zeroes } },
        {  .type = STRING, .size = 65, .value = { .string = zeroes } },
        {  .type = STRING, .size = 250, .value = { .string = zeroes } },
        {  .type = BYTE, .value = { .b = 0 } },
        {  .type = STRING, .size = 252, .value = { .string = zeroes } },
        {  .type = SHORT, .value = { .s = 0 } },
        {  .type = LONG, .value = { .l = 0 } },
        {  .type = LONG, .value = { .l = 0 } },
        {  .type = LONG, .value = { .l = 0 } },
        {  .type = LONG, .value = { .l = 0 } },
        {  .type = LONG, .value = { .l = 0 } },
        {  .type = LONG, .value = { .l = 0 } },
        {  .type = LONG, .value = { .l = 0 } },
        {  .type = LONG, .value = { .l = 0 } },
        {  .type = LONG, .value = { .l = 0 } },
        {  .type = LONG, .value = { .l = 0 } },
        {  .type = LONG, .value = { .l = 0 } },
        {  .type = LONG, .value = { .l = 0 } },
        {  .type = LONG, .value = { .l = 0 } },
        {  .type = LONG, .value = { .l = 0 } },
        {  .type = LONG, .value = { .l = 0 } },
        {  .type = LONG, .value = { .l = 0 } },
        {  .type = LONG, .value = { .l = 0 } },
        {  .type = LONG, .value = { .l = 0 } },
        {  .type = LONG, .value = { .l = 0 } },
        {  .type = LONG, .value = { .l = 0 } },
        {  .type = LONG, .value = { .l = 0 } },
        {  .type = STRING, .size = 20, .value = { .string = zeroes } },
        {  .type = LONG, .value = { .l = 0 } },
        {  .type = STRING, .size = 104, .value = { .string = zeroes } },
        {  .type = STRING, .size = 260, .value = { .string = zeroes } },
        {  .type = LONG, .value = { .l = 0 } },
        {  .type = STRING, .size = 66, .value = { .string = zeroes } },
        {  .type = STRING, .size = 65, .value = { .string = zeroes } },
        {  .type = STRING, .size = 250, .value = { .string = zeroes } },
        {  .type = BYTE, .value = { .b = 0 } },
        {  .type = STRING, .size = 252, .value = { .string = zeroes } },
        {  .type = SHORT, .value = { .s = 0 } },
        {  .type = LONG, .value = { .l = 0 } },
        {  .type = LONG, .value = { .l = 0 } },
        {  .type = LONG, .value = { .l = 0 } },
        {  .type = LONG, .value = { .l = 0 } },
        {  .type = LONG, .value = { .l = 1 } },
        {  .type = LONG, .value = { .l = 0 } },
        {  .type = LONG, .value = { .l = 0 } },
        {  .type = LONG, .value = { .l = 0 } },
        {  .type = LONG, .value = { .l = 0 } },
        {  .type = LONG, .value = { .l = 0 } },
        {  .type = LONG, .value = { .l = 0 } },
        {  .type = LONG, .value = { .l = 0 } },
        {  .type = LONG, .value = { .l = 1 } },
        {  .type = LONG, .value = { .l = 0 } },
        {  .type = LONG, .value = { .l = 0 } },
        {  .type = LONG, .value = { .l = 0x404 } },
        {  .type = LONG, .value = { .l = 1 } },
        {  .type = LONG, .value = { .l = 0 } },
        {  .type = LONG, .value = { .l = 1 } },
        {  .type = LONG, .value = { .l = 0 } },
        {  .type = LONG, .value = { .l = 0 } },
        {  .type = STRING, .size = 20, .value = { .string = zeroes } },
        {  .type = LONG, .value = { .l = 3 } },
        {  .type = STRING, .size = 104, .value = { .string = apn } },
        {  .type = STRING, .size = 260, .value = { .string = zeroes } },
        {  .type = LONG, .value = { .l = 0 } },
        {  .type = STRING, .size = 66, .value = { .string = zeroes } },
        {  .type = STRING, .size = 65, .value = { .string = zeroes } },
        {  .type = STRING, .size = 250, .value = { .string = zeroes } },
        {  .type = BYTE, .value = { .b = 0 } },
        {  .type = STRING, .size = 252, .value = { .string = zeroes } },
        {  .type = SHORT, .value = { .s = 0 } },
        {  .type = LONG, .value = { .l = 0 } },
        {  .type = LONG, .value = { .l = 0 } },
        {  .type = LONG, .value = { .l = 0 } },
        {  .type = LONG, .value = { .l = 0 } },
        {  .type = LONG, .value = { .l = 1 } },
        {  .type = LONG, .value = { .l = 0 } },
        {  .type = LONG, .value = { .l = 0 } },
        {  .type = LONG, .value = { .l = 0 } },
        {  .type = LONG, .value = { .l = 0 } },
        {  .type = LONG, .value = { .l = 0 } },
        {  .type = LONG, .value = { .l = 0 } },
        {  .type = LONG, .value = { .l = 0 } },
        {  .type = LONG, .value = { .l = 1 } },
        {  .type = LONG, .value = { .l = 0 } },
        {  .type = LONG, .value = { .l = 0 } },
        {  .type = LONG, .value = { .l = 0x404 } },
        {  .type = LONG, .value = { .l = 1 } },
        {  .type = LONG, .value = { .l = 0 } },
        {  .type = LONG, .value = { .l = 1 } },
        {  .type = LONG, .value = { .l = 0 } },
        {  .type = LONG, .value = { .l = 0 } },
        {  .type = STRING, .size = 20, .value = { .string = zeroes } },
        {  .type = LONG, .value = { .l = 3 } },
        {  .type = STRING, .size = 103, .value = { .string = apn } },
        {  .type = BYTE, .value = { .b = 3 } },
        {  .type = LONG, .value = { .l = 0 } },
    };
    return pack(G_N_ELEMENTS(args), args);
}

gboolean unpack(GBytes* data, guint count, rpc_arg* args) {
    guint i;
    GBytes* string;
    const guint8 *strdata;
    gsize strlength;
    gsize current_offset = 0;

    for(i = 0; i < count; i++, args++) {
        switch(args->type) {
            case BYTE:
                args->value.b = (gint8)get_asn_int(data, &current_offset);
                break;
            case SHORT:
                args->value.s = (gint16)get_asn_int(data, &current_offset);
                break;
            case LONG:
                args->value.l = (gint32)get_asn_int(data, &current_offset);
                break;
            case STRING:
                string = get_string(data, &current_offset);
                strdata = g_bytes_get_data(string, &strlength);
                args->value.string = (const gchar*)strdata;
                args->size = strlength;
                break;
            default:
                //should be unreachable
                return FALSE;
        }
    }
    return TRUE;
}


gboolean unpack_uta_ms_call_ps_get_neg_ip_addr_req(GBytes* data, guint32* ip1, guint32* ip2, guint32* ip3) {
    rpc_arg* ip_arg;
    guint32* ip_ptr;
    rpc_arg args[] = {
        { .type = LONG },
        { .type = STRING },
        { .type = LONG },
        { .type = LONG },
        { .type = LONG },
        { .type = LONG },
    };
    if(!unpack(data, G_N_ELEMENTS(args), args)) {
        return FALSE;
    }
    ip_arg = args + 1;
    assert(ip_arg->size >= 12);
    ip_ptr = (guint32*)ip_arg->value.string;
    *ip1 = ip_ptr[0];
    *ip2 = ip_ptr[1];
    *ip3 = ip_ptr[2];
    return TRUE;
}

gboolean unpack_uta_ms_call_ps_get_neg_dns_req(GBytes* data, guint32* ipv4_1, guint32* ipv4_2) {
    guint i;
    rpc_arg* dns_args;
    rpc_arg args[] = {
        { .type = LONG },
        { .type = STRING }, { .type = LONG }, { .type = STRING }, { .type = LONG },
        { .type = STRING }, { .type = LONG }, { .type = STRING }, { .type = LONG },
        { .type = STRING }, { .type = LONG }, { .type = STRING }, { .type = LONG },
        { .type = STRING }, { .type = LONG }, { .type = STRING }, { .type = LONG },
        { .type = STRING }, { .type = LONG }, { .type = STRING }, { .type = LONG },
        { .type = STRING }, { .type = LONG }, { .type = STRING }, { .type = LONG },
        { .type = STRING }, { .type = LONG }, { .type = STRING }, { .type = LONG },
        { .type = LONG },
        { .type = STRING },
        { .type = LONG },
        { .type = LONG },
        { .type = LONG },
        { .type = LONG },
    };
    if(!unpack(data, G_N_ELEMENTS(args), args)) {
        return FALSE;
    }
    *ipv4_1 = 0;
    *ipv4_2 = 0;
    dns_args = args + 1;
    for(i = 0; i < 16; i += 2) {
        // check for IPv4
        if(dns_args[i + 1].value.l == 1) {
            assert(dns_args[i].size >= 4);
            if(*ipv4_1 == 0) {
                *ipv4_1 = *((guint32*)dns_args[i].value.string);
            } else {
                *ipv4_2 = *((guint32*)dns_args[i].value.string);
                return TRUE;
            }
        }
    }
    return TRUE;
}
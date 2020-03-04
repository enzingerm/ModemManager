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
#include <unistd.h>

#include "mm-xmm7360-rpc.h"
#include "mm-xmm7360-rpc-enums-types.h"

#define MAX_READ_SIZE (131072)

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

int xmm7360_rpc_dispose(xmm7360_rpc* rpc) {
    int res;
    if(rpc->fd < 0) {
        return -1;
    }
    res = close(rpc->fd);
    rpc->fd = -1;
    return res;
}

rpc_message* xmm7360_rpc_alloc_message(void) {
    GArray* response_arr = g_array_new(FALSE, TRUE, sizeof(rpc_arg));
    rpc_message* msg = g_new0(rpc_message, 1);
    msg->content = response_arr;
    return msg;
}

void xmm7360_rpc_free_message(rpc_message* msg) {
    if(msg != NULL) {
        g_array_free(msg->content, TRUE);
        g_bytes_unref(msg->body);
        g_free(msg);
    }
}

int xmm7360_rpc_pump(xmm7360_rpc* rpc, rpc_message** response_ptr) {
    GByteArray* response_msg = g_byte_array_sized_new(MAX_READ_SIZE);
    GBytes* response_msg_data;
    rpc_message *response;
    int read_bytes = read(rpc->fd, (void*)response_msg->data, MAX_READ_SIZE);
    assert(read_bytes > 0);
    response_msg->len = read_bytes;
    response_msg_data = g_byte_array_free_to_bytes(response_msg);
    response = xmm7360_rpc_alloc_message();
    if(response_ptr != NULL) {
        *response_ptr = response;
    }
    if(xmm7360_rpc_handle_message(rpc, response_msg_data, response) != 0) {
        xmm7360_rpc_free_message(response);
        return -1;
    }
    if(response->type == UNSOLICITED) {
        //TODO printf unsolicited
        xmm7360_rpc_handle_unsolicited(rpc, response);
    }
    
    if(response_ptr == NULL) {
        xmm7360_rpc_free_message(response);
    }
    return 0;
}

int xmm7360_rpc_execute(xmm7360_rpc* rpc, Xmm7360RpcCallIds cmd, gboolean is_async, GByteArray* body, rpc_message** res_ptr) {
    gint32 tid, tid_word;
    gint32 tid_word_be;
    guint32 total_len;
    rpc_message* pump_response;
    int written;
    GByteArray* header = g_byte_array_sized_new(22);
    //default argument
    if(body == NULL) {
        body = g_byte_array_new();
        asn_int4(body, 0);
    }
    if(is_async) {
        tid = 0x11000101;
    } else {
        tid = 0;
    }
    tid_word = 0x11000100 | tid;
    total_len = body->len + 16;
    if(tid) {
        total_len += 6;
    }
    g_byte_array_append(header, (guint8*)&total_len, 4);
    asn_int4(header, total_len);
    asn_int4(header, cmd);
    tid_word_be = GINT32_TO_BE(tid_word);
    g_byte_array_append(header, (guint8*)&tid_word_be, 4);
    if(tid) {
        asn_int4(header, tid);
    }

    assert(total_len == header->len + body->len - 4);
    g_byte_array_append(header, body->data, body->len);
    g_byte_array_free(body, TRUE);
    written = write(rpc->fd, header->data, header->len);
    if(written != (int)header->len) {
        //TODO: write error
        return -1;
    }

    while(TRUE) {
        if(xmm7360_rpc_pump(rpc, &pump_response) != 0) {
            return -1;
        }
        if(pump_response->type == RESPONSE) {
            if(res_ptr == NULL) {
                xmm7360_rpc_free_message(pump_response);
                return 0;
            }
            *res_ptr = pump_response;
            break;
        }
        xmm7360_rpc_free_message(pump_response);
    }

    return 0;
}

int xmm7360_rpc_handle_message(xmm7360_rpc* rpc, GBytes* message, rpc_message* res_ptr) {
    gsize message_size;
    gsize current_offset;
    const guint8* data = g_bytes_get_data(message, &message_size);
    gint32 txid;
    rpc_arg* txid_arg;

    assert(message_size > 20);
    assert(data[4] == 0x02 && data[5] == 0x04);
    assert(data[10] == 0x02 && data[11] == 0x04);

    current_offset = 4;
    if(*(gint32*)data != get_asn_int(message, &current_offset)) {
        //TODO: printf("length mismatch, framing error?\n");
        goto err;
    }
    res_ptr->code = get_asn_int(message, &current_offset);
    res_ptr->body = g_bytes_new_from_bytes(message, 20, message_size - 20);
    if(unpack_unknown(res_ptr->body, res_ptr->content) != 0) {
        //TODO print error
        goto err;
    }
    txid = GINT32_FROM_BE(*(gint32*)(data + 16));
    res_ptr->tx_id = txid;
    if(txid == 0x11000100) {
        res_ptr->type = RESPONSE;
        //TODO check if signedness leads to problems here
    } else if((txid & 0xffffff00) == 0x11000100) {
        if(res_ptr->code >= 2000) {
            res_ptr->type = ASYNC_ACK;
        } else {
            res_ptr->type = RESPONSE;
            //assert first element in content is txid
            txid_arg = &g_array_index(res_ptr->content, rpc_arg, 0);
            if(txid_arg->type != LONG || GET_RPC_INT(txid_arg) != txid) {
                goto err;
            }
            g_array_remove_index(res_ptr->content, 0);
            //jump over first six bytes (asn int of txid)
            g_bytes_unref(res_ptr->body);
            res_ptr->body = g_bytes_new_from_bytes(message, 26, message_size - 26);
        }
    } else {
        res_ptr->type = UNSOLICITED;
    }

    g_bytes_unref(message);
    return 0;

err:
    g_bytes_unref(message);
    return -1;
}

int xmm7360_rpc_handle_unsolicited(xmm7360_rpc* rpc, rpc_message* message) {
    rpc_arg* attach_argument;

    if((Xmm7360RpcUnsolIds)message->code == UtaMsNetIsAttachAllowedIndCb) {
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
                g_byte_array_append(ret, (guint8[]){ 0x02, 0x01 }, 2);
                g_byte_array_append(ret, (guint8*)&args->value.b, 1);
                break;
            case SHORT:
                sh = GINT16_TO_BE(args->value.s);
                g_byte_array_append(ret, (guint8[]){ 0x02, 0x02 }, 2);
                g_byte_array_append(ret, (guint8*)&sh, 2);
                break;
            case LONG:
                lng = GINT32_TO_BE(args->value.l);
                g_byte_array_append(ret, (guint8[]){ 0x02, 0x04 }, 2);
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
    asn_int4(target, length - val_len);
    g_byte_array_append(target, data, val_len);
    for(i = 0; i < length - val_len; i++) {
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
    data += *current_offset;
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
    gint value;
    guchar type;
    guchar bytelen;
    gint count;
    gint padding;
    const guchar* data = g_bytes_get_data(bytes, &len);
    data += *current_offset;
    *current_offset += 2;
    assert(len > *current_offset);
    type = *data++;
    valid = *data++;
    value = valid;

    assert(type == 0x55 || type == 0x56 || type == 0x57);
    if(valid & 0x80) {
        value = 0;
        bytelen = valid & 0xf;
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
    ret = g_bytes_new_from_bytes(bytes, *current_offset - value, value);

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

GByteArray* pack_uta_mode_set(gint32 mode) {
    rpc_arg args[] = {
        { .type = LONG, .value = { .l = 0 } },
        { .type = LONG, .value = { .l = 15 } },
        { .type = LONG, .value = { .l = mode } },
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

int unpack_unknown(GBytes* message, GArray* args_array) {
    gsize current_offset = 0;
    gsize data_len;
    const guint8* data;
    GBytes* taken_string;
    rpc_arg cur_arg = { 0 };
    assert(message != NULL && args_array != NULL);
    data = g_bytes_get_data(message, &data_len);
    while(current_offset < data_len) {
        cur_arg.size = 0;
        switch(data[current_offset]) {
            case 0x02:
                cur_arg.type = LONG;
                cur_arg.value.l = get_asn_int(message, &current_offset);
                break;
            case 0x55:
            case 0x56:
            case 0x57:
                cur_arg.type = STRING;
                taken_string = get_string(message, &current_offset);
                if(taken_string == NULL) {
                    return -1;
                }
                cur_arg.value.string = g_bytes_get_data(taken_string, &cur_arg.size);
                g_bytes_unref(taken_string);
                break;
            default:
                //TODO error handling
                return -1;
        }
        g_array_append_val(args_array, cur_arg);
    }
    return 0;
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

int xmm7360_do_fcc_unlock(xmm7360_rpc* rpc) {
    rpc_message *msg = NULL;
    rpc_arg* arg;
    GChecksum* checksum;
    guchar key[] = { 0x3d, 0xf8, 0xc7, 0x19 };
    gsize digest_len = 32;
    guint8 digest[32] = { 0 };
    gint32 fcc_chal;
    GByteArray* digest_response;

    if(xmm7360_rpc_execute(rpc, CsiFccLockQueryReq, TRUE, NULL, &msg) != 0) {
        goto err;
    }

    if(msg->content->len < 3) {
        goto err;
    }
    //second argument is fcc_state
    arg = &g_array_index(msg->content, rpc_arg, 1);
    assert(arg->type == LONG);
    // if fcc stat is not 0 -> return
    if(GET_RPC_INT(arg)) {
        goto success;
    }
    //third argument is fcc_mode
    arg++;
    assert(arg->type == LONG);
    // if fcc_mode is 0 -> return
    if(!GET_RPC_INT(arg)) {
        goto success;
    }
    xmm7360_rpc_free_message(msg);
    msg = NULL;
    if(xmm7360_rpc_execute(rpc, CsiFccLockGenChallengeReq, TRUE, NULL, &msg) != 0) {
        goto err;
    }
    if(msg->content->len < 2) {
        goto err;
    }
    arg = &g_array_index(msg->content, rpc_arg, 1);
    assert(arg->type == LONG);
    fcc_chal = GET_RPC_INT(arg);
    xmm7360_rpc_free_message(msg);
    checksum = g_checksum_new(G_CHECKSUM_SHA256);
    g_checksum_update(checksum, (guchar*)&fcc_chal, 4);
    g_checksum_update(checksum, key, 4);
    g_checksum_get_digest(checksum, digest, &digest_len);
    g_checksum_free(checksum);
    digest_response = g_byte_array_new();
    asn_int4(digest_response, GINT32_FROM_LE(*(gint32*)digest));

    //send back digest
    if(xmm7360_rpc_execute(rpc, CsiFccLockVerChallengeReq, TRUE, digest_response, &msg) != 0) {
        goto err;
    }
    if(msg->content->len < 1) {
        goto err;
    }
    arg = &g_array_index(msg->content, rpc_arg, 0);
    assert(arg->type == LONG);
    if(GET_RPC_INT(arg) != 1) {
        goto err;
    }

success:
    xmm7360_rpc_free_message(msg);
    return 0;

err:
    xmm7360_rpc_free_message(msg);
    return -1;
}

int xmm7360_uta_mode_set(xmm7360_rpc* rpc, gint32 mode) {
    rpc_message *msg = NULL;
    rpc_arg* arg;
    GByteArray* args = pack_uta_mode_set(mode);
    if(xmm7360_rpc_execute(rpc, UtaModeSetReq, FALSE, args, &msg) != 0) {
        goto err;
    }
    if(msg->content->len < 1) {
        goto err;
    }
    arg = &g_array_index(msg->content, rpc_arg, 0);
    assert(arg->type == LONG);
    if(GET_RPC_INT(arg) != 0) {
        goto err;
    }

    while(TRUE) {
        xmm7360_rpc_free_message(msg);
        msg = NULL;
        if(xmm7360_rpc_pump(rpc, &msg) != 0) {
            goto err;
        }
        if((Xmm7360RpcUnsolIds)msg->code == UtaModeSetRspCb) {
            if(msg->content->len < 1) {
                goto err;
            }
            arg = &g_array_index(msg->content, rpc_arg, 0);
            assert(arg->type == LONG);
            if(GET_RPC_INT(arg) != mode) {
                //TODO: mode set unable
                goto err;
            }
            break;
        }
    }

    xmm7360_rpc_free_message(msg);
    return 0;

err:
    xmm7360_rpc_free_message(msg);
    return -1;
}

int xmm7360_net_attach(xmm7360_rpc* rpc, gint32* status_ptr) {
    rpc_message* message = NULL;
    rpc_arg* status_arg = NULL;

    if(xmm7360_rpc_execute(
        rpc,
        UtaMsNetAttachReq,
        TRUE,
        pack_uta_ms_net_attach_req(),
        &message
    ) != 0) {
        return -1;
    }

    if(message->content->len < 2) {
        xmm7360_rpc_free_message(message);
        return -1;
    }
    status_arg = &g_array_index(message->content, rpc_arg, 1);
    assert(status_arg->type == LONG);
    *status_ptr = GET_RPC_INT(status_arg);
    xmm7360_rpc_free_message(message);
    return 0;
}

int xmm7360_get_ip_and_dns(xmm7360_rpc* rpc, xmm7360_ip_config* ip_config) {
    rpc_message* message = NULL;
    assert(ip_config != NULL);
    if(xmm7360_rpc_execute(
        rpc,
        UtaMsCallPsGetNegIpAddrReq,
        TRUE,
        pack_uta_ms_call_ps_get_get_ip_addr_req(),
        &message
    ) != 0) {
        goto err;
    }

    if(!unpack_uta_ms_call_ps_get_neg_ip_addr_req(
        message->body,
        &ip_config->ip4_1,
        &ip_config->ip4_2,
        &ip_config->ip4_3)
    ) {
        goto err;
    }
    xmm7360_rpc_free_message(message);
    message = NULL;
    if(xmm7360_rpc_execute(
        rpc,
        UtaMsCallPsGetNegotiatedDnsReq,
        TRUE,
        pack_uta_ms_call_ps_get_negotiated_dns_req(),
        &message
    ) != 0) {
        goto err;
    }
    if(!unpack_uta_ms_call_ps_get_neg_dns_req(
        message->body,
        &ip_config->dns4_1,
        &ip_config->dns4_2
    )) {
        goto err;
    }
    xmm7360_rpc_free_message(message);
    return 0;

err:
    xmm7360_rpc_free_message(message);
    return -1;
}
#include "common.h"
#include "messages.h"
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <spice/protocol.h>
#include <spice/macros.h>
#include "mem.h"

#ifdef _MSC_VER
#pragma warning(disable:4101)
#endif



#ifdef WORDS_BIGENDIAN
#define read_int8(ptr) (*((int8_t *)(ptr)))
#define write_int8(ptr, val) *(int8_t *)(ptr) = val
#define read_uint8(ptr) (*((uint8_t *)(ptr)))
#define write_uint8(ptr, val) *(uint8_t *)(ptr) = val
#define read_int16(ptr) ((int16_t)SPICE_BYTESWAP16(*((uint16_t *)(ptr)))
#define write_int16(ptr, val) *(uint16_t *)(ptr) = SPICE_BYTESWAP16((uint16_t)val)
#define read_uint16(ptr) ((uint16_t)SPICE_BYTESWAP16(*((uint16_t *)(ptr)))
#define write_uint16(ptr, val) *(uint16_t *)(ptr) = SPICE_BYTESWAP16((uint16_t)val)
#define read_int32(ptr) ((int32_t)SPICE_BYTESWAP32(*((uint32_t *)(ptr)))
#define write_int32(ptr, val) *(uint32_t *)(ptr) = SPICE_BYTESWAP32((uint32_t)val)
#define read_uint32(ptr) ((uint32_t)SPICE_BYTESWAP32(*((uint32_t *)(ptr)))
#define write_uint32(ptr, val) *(uint32_t *)(ptr) = SPICE_BYTESWAP32((uint32_t)val)
#define read_int64(ptr) ((int64_t)SPICE_BYTESWAP64(*((uint64_t *)(ptr)))
#define write_int64(ptr, val) *(uint64_t *)(ptr) = SPICE_BYTESWAP64((uint64_t)val)
#define read_uint64(ptr) ((uint64_t)SPICE_BYTESWAP64(*((uint64_t *)(ptr)))
#define write_uint64(ptr, val) *(uint64_t *)(ptr) = SPICE_BYTESWAP64((uint64_t)val)
#else
#define read_int8(ptr) (*((int8_t *)(ptr)))
#define write_int8(ptr, val) (*((int8_t *)(ptr))) = val
#define read_uint8(ptr) (*((uint8_t *)(ptr)))
#define write_uint8(ptr, val) (*((uint8_t *)(ptr))) = val
#define read_int16(ptr) (*((int16_t *)(ptr)))
#define write_int16(ptr, val) (*((int16_t *)(ptr))) = val
#define read_uint16(ptr) (*((uint16_t *)(ptr)))
#define write_uint16(ptr, val) (*((uint16_t *)(ptr))) = val
#define read_int32(ptr) (*((int32_t *)(ptr)))
#define write_int32(ptr, val) (*((int32_t *)(ptr))) = val
#define read_uint32(ptr) (*((uint32_t *)(ptr)))
#define write_uint32(ptr, val) (*((uint32_t *)(ptr))) = val
#define read_int64(ptr) (*((int64_t *)(ptr)))
#define write_int64(ptr, val) (*((int64_t *)(ptr))) = val
#define read_uint64(ptr) (*((uint64_t *)(ptr)))
#define write_uint64(ptr, val) (*((uint64_t *)(ptr))) = val
#endif

static int8_t SPICE_GNUC_UNUSED consume_int8(uint8_t **ptr)
{
    int8_t val;
    val = read_int8(*ptr);
    *ptr += 1;
    return val;
}

static uint8_t SPICE_GNUC_UNUSED consume_uint8(uint8_t **ptr)
{
    uint8_t val;
    val = read_uint8(*ptr);
    *ptr += 1;
    return val;
}

static int16_t SPICE_GNUC_UNUSED consume_int16(uint8_t **ptr)
{
    int16_t val;
    val = read_int16(*ptr);
    *ptr += 2;
    return val;
}

static uint16_t SPICE_GNUC_UNUSED consume_uint16(uint8_t **ptr)
{
    uint16_t val;
    val = read_uint16(*ptr);
    *ptr += 2;
    return val;
}

static int32_t SPICE_GNUC_UNUSED consume_int32(uint8_t **ptr)
{
    int32_t val;
    val = read_int32(*ptr);
    *ptr += 4;
    return val;
}

static uint32_t SPICE_GNUC_UNUSED consume_uint32(uint8_t **ptr)
{
    uint32_t val;
    val = read_uint32(*ptr);
    *ptr += 4;
    return val;
}

static int64_t SPICE_GNUC_UNUSED consume_int64(uint8_t **ptr)
{
    int64_t val;
    val = read_int64(*ptr);
    *ptr += 8;
    return val;
}

static uint64_t SPICE_GNUC_UNUSED consume_uint64(uint8_t **ptr)
{
    uint64_t val;
    val = read_uint64(*ptr);
    *ptr += 8;
    return val;
}

typedef struct PointerInfo PointerInfo;
typedef void (*message_destructor_t)(uint8_t *message);
typedef uint8_t * (*parse_func_t)(uint8_t *message_start, uint8_t *message_end, uint8_t *struct_data, PointerInfo *ptr_info, int minor);
typedef uint8_t * (*parse_msg_func_t)(uint8_t *message_start, uint8_t *message_end, int minor, size_t *size_out, message_destructor_t *free_message);
typedef uint8_t * (*spice_parse_channel_func_t)(uint8_t *message_start, uint8_t *message_end, uint16_t message_type, int minor, size_t *size_out, message_destructor_t *free_message);

struct PointerInfo {
    uint64_t offset;
    parse_func_t parse;
    void * *dest;
    uint32_t nelements;
};

static uint8_t * parse_msg_migrate(uint8_t *message_start, uint8_t *message_end, int minor, size_t *size, message_destructor_t *free_message)
{
    SPICE_GNUC_UNUSED uint8_t *pos;
    uint8_t *start = message_start;
    uint8_t *data = NULL;
    size_t mem_size, nw_size;
    uint8_t *in, *end;
    SpiceMsgMigrate *out;

    nw_size = 4;
    mem_size = sizeof(SpiceMsgMigrate);

    /* Check if message fits in reported side */
    if (start + nw_size > message_end) {
        return NULL;
    }

    /* Validated extents and calculated size */
    data = (uint8_t *)malloc(mem_size);
    if (SPICE_UNLIKELY(data == NULL)) {
        goto error;
    }
    end = data + sizeof(SpiceMsgMigrate);
    in = start;

    out = (SpiceMsgMigrate *)data;

    out->flags = consume_uint32(&in);

    assert(in <= message_end);
    assert(end <= data + mem_size);

    *size = end - data;
    *free_message = (message_destructor_t) free;
    return data;

   error:
    if (data != NULL) {
        free(data);
    }
    return NULL;
}

static void nofree(uint8_t *data)
{
}

static uint8_t * parse_SpiceMsgData(uint8_t *message_start, uint8_t *message_end, int minor, size_t *size, message_destructor_t *free_message)
{
    SPICE_GNUC_UNUSED uint8_t *pos;
    uint8_t *start = message_start;
    uint8_t *data = NULL;
    size_t mem_size, nw_size;
    size_t data__nw_size, data__mem_size;
    uint32_t data__nelements;

    { /* data */
        data__nelements = message_end - (start + 0);

        data__nw_size = data__nelements;
        data__mem_size = sizeof(uint8_t) * data__nelements;
    }

    nw_size = 0 + data__nw_size;
    mem_size = sizeof(SpiceMsgData) + data__mem_size;

    /* Check if message fits in reported side */
    if (start + nw_size > message_end) {
        return NULL;
    }

    /* Validated extents and calculated size */
    data = message_start;
    *size = message_end - message_start;
    *free_message = nofree;
    return data;

}

static uint8_t * parse_msg_set_ack(uint8_t *message_start, uint8_t *message_end, int minor, size_t *size, message_destructor_t *free_message)
{
    SPICE_GNUC_UNUSED uint8_t *pos;
    uint8_t *start = message_start;
    uint8_t *data = NULL;
    size_t mem_size, nw_size;
    uint8_t *in, *end;
    SpiceMsgSetAck *out;

    nw_size = 8;
    mem_size = sizeof(SpiceMsgSetAck);

    /* Check if message fits in reported side */
    if (start + nw_size > message_end) {
        return NULL;
    }

    /* Validated extents and calculated size */
    data = (uint8_t *)malloc(mem_size);
    if (SPICE_UNLIKELY(data == NULL)) {
        goto error;
    }
    end = data + sizeof(SpiceMsgSetAck);
    in = start;

    out = (SpiceMsgSetAck *)data;

    out->generation = consume_uint32(&in);
    out->window = consume_uint32(&in);

    assert(in <= message_end);
    assert(end <= data + mem_size);

    *size = end - data;
    *free_message = (message_destructor_t) free;
    return data;

   error:
    if (data != NULL) {
        free(data);
    }
    return NULL;
}

static uint8_t * parse_msg_ping(uint8_t *message_start, uint8_t *message_end, int minor, size_t *size, message_destructor_t *free_message)
{
    SPICE_GNUC_UNUSED uint8_t *pos;
    uint8_t *start = message_start;
    uint8_t *data = NULL;
    size_t mem_size, nw_size;
    uint8_t *in, *end;
    size_t data__nw_size;
    uint32_t data__nelements;
    SpiceMsgPing *out;

    { /* data */
        data__nelements = message_end - (start + 12);

        data__nw_size = data__nelements;
    }

    nw_size = 12 + data__nw_size;
    mem_size = sizeof(SpiceMsgPing);

    /* Check if message fits in reported side */
    if (start + nw_size > message_end) {
        return NULL;
    }

    /* Validated extents and calculated size */
    data = (uint8_t *)malloc(mem_size);
    if (SPICE_UNLIKELY(data == NULL)) {
        goto error;
    }
    end = data + sizeof(SpiceMsgPing);
    in = start;

    out = (SpiceMsgPing *)data;

    out->id = consume_uint32(&in);
    out->timestamp = consume_uint64(&in);
    /* use array as pointer */
    out->data = (uint8_t *)in;
    out->data_len = data__nelements;
    in += data__nelements;

    assert(in <= message_end);
    assert(end <= data + mem_size);

    *size = end - data;
    *free_message = (message_destructor_t) free;
    return data;

   error:
    if (data != NULL) {
        free(data);
    }
    return NULL;
}

static uint8_t * parse_msg_wait_for_channels(uint8_t *message_start, uint8_t *message_end, int minor, size_t *size, message_destructor_t *free_message)
{
    SPICE_GNUC_UNUSED uint8_t *pos;
    uint8_t *start = message_start;
    uint8_t *data = NULL;
    size_t mem_size, nw_size;
    uint8_t *in, *end;
    size_t wait_list__nw_size, wait_list__mem_size;
    uint32_t wait_list__nelements;
    SpiceMsgWaitForChannels *out;
    uint32_t i;

    { /* wait_list */
        uint8_t wait_count__value;
        pos = start + 0;
        if (SPICE_UNLIKELY(pos + 1 > message_end)) {
            goto error;
        }
        wait_count__value = read_uint8(pos);
        wait_list__nelements = wait_count__value;

        wait_list__nw_size = (10) * wait_list__nelements;
        wait_list__mem_size = sizeof(SpiceWaitForChannel) * wait_list__nelements;
    }

    nw_size = 1 + wait_list__nw_size;
    mem_size = sizeof(SpiceMsgWaitForChannels) + wait_list__mem_size;

    /* Check if message fits in reported side */
    if (start + nw_size > message_end) {
        return NULL;
    }

    /* Validated extents and calculated size */
    data = (uint8_t *)malloc(mem_size);
    if (SPICE_UNLIKELY(data == NULL)) {
        goto error;
    }
    end = data + sizeof(SpiceMsgWaitForChannels);
    in = start;

    out = (SpiceMsgWaitForChannels *)data;

    out->wait_count = consume_uint8(&in);
    for (i = 0; i < wait_list__nelements; i++) {
        SpiceWaitForChannel *out2;
        out2 = (SpiceWaitForChannel *)end;
        end += sizeof(SpiceWaitForChannel);

        out2->channel_type = consume_uint8(&in);
        out2->channel_id = consume_uint8(&in);
        out2->message_serial = consume_uint64(&in);
    }

    assert(in <= message_end);
    assert(end <= data + mem_size);

    *size = end - data;
    *free_message = (message_destructor_t) free;
    return data;

   error:
    if (data != NULL) {
        free(data);
    }
    return NULL;
}

static uint8_t * parse_msg_disconnecting(uint8_t *message_start, uint8_t *message_end, int minor, size_t *size, message_destructor_t *free_message)
{
    SPICE_GNUC_UNUSED uint8_t *pos;
    uint8_t *start = message_start;
    uint8_t *data = NULL;
    size_t mem_size, nw_size;
    uint8_t *in, *end;
    SpiceMsgDisconnect *out;

    nw_size = 12;
    mem_size = sizeof(SpiceMsgDisconnect);

    /* Check if message fits in reported side */
    if (start + nw_size > message_end) {
        return NULL;
    }

    /* Validated extents and calculated size */
    data = (uint8_t *)malloc(mem_size);
    if (SPICE_UNLIKELY(data == NULL)) {
        goto error;
    }
    end = data + sizeof(SpiceMsgDisconnect);
    in = start;

    out = (SpiceMsgDisconnect *)data;

    out->time_stamp = consume_uint64(&in);
    out->reason = consume_uint32(&in);

    assert(in <= message_end);
    assert(end <= data + mem_size);

    *size = end - data;
    *free_message = (message_destructor_t) free;
    return data;

   error:
    if (data != NULL) {
        free(data);
    }
    return NULL;
}

static uint8_t * parse_msg_notify(uint8_t *message_start, uint8_t *message_end, int minor, size_t *size, message_destructor_t *free_message)
{
    SPICE_GNUC_UNUSED uint8_t *pos;
    uint8_t *start = message_start;
    uint8_t *data = NULL;
    size_t mem_size, nw_size;
    uint8_t *in, *end;
    size_t message__nw_size, message__mem_size;
    uint32_t message__nelements;
    SpiceMsgNotify *out;

    { /* message */
        uint32_t message_len__value;
        pos = start + 20;
        if (SPICE_UNLIKELY(pos + 4 > message_end)) {
            goto error;
        }
        message_len__value = read_uint32(pos);
        message__nelements = message_len__value;

        message__nw_size = message__nelements;
        message__mem_size = sizeof(uint8_t) * message__nelements;
    }

    nw_size = 24 + message__nw_size;
    mem_size = sizeof(SpiceMsgNotify) + message__mem_size;

    /* Check if message fits in reported side */
    if (start + nw_size > message_end) {
        return NULL;
    }

    /* Validated extents and calculated size */
    data = (uint8_t *)malloc(mem_size);
    if (SPICE_UNLIKELY(data == NULL)) {
        goto error;
    }
    end = data + sizeof(SpiceMsgNotify);
    in = start;

    out = (SpiceMsgNotify *)data;

    out->time_stamp = consume_uint64(&in);
    out->severity = consume_uint32(&in);
    out->visibilty = consume_uint32(&in);
    out->what = consume_uint32(&in);
    out->message_len = consume_uint32(&in);
    memcpy(out->message, in, message__nelements);
    in += message__nelements;
    end += message__nelements;

    assert(in <= message_end);
    assert(end <= data + mem_size);

    *size = end - data;
    *free_message = (message_destructor_t) free;
    return data;

   error:
    if (data != NULL) {
        free(data);
    }
    return NULL;
}

static uint8_t * parse_array_uint8(uint8_t *message_start, uint8_t *message_end, uint8_t *struct_data, PointerInfo *this_ptr_info, int minor)
{
    uint8_t *in = message_start + this_ptr_info->offset;
    uint8_t *end;

    end = struct_data;
    memcpy(end, in, this_ptr_info->nelements);
    in += this_ptr_info->nelements;
    end += this_ptr_info->nelements;
    return end;
}

static uint8_t * parse_msg_main_migrate_begin(uint8_t *message_start, uint8_t *message_end, int minor, size_t *size, message_destructor_t *free_message)
{
    SPICE_GNUC_UNUSED uint8_t *pos;
    uint8_t *start = message_start;
    uint8_t *data = NULL;
    size_t mem_size, nw_size;
    uint8_t *in, *end;
    SPICE_GNUC_UNUSED intptr_t ptr_size;
    uint32_t n_ptr=0;
    PointerInfo ptr_info[2];
    size_t host_data__extra_size;
    uint32_t host_data__array__nelements;
    size_t pub_key_data__extra_size;
    uint32_t pub_key_data__array__nelements;
    SpiceMsgMainMigrationBegin *out;
    uint32_t i;

    { /* host_data */
        uint32_t host_data__value;
        uint32_t host_data__array__nw_size;
        uint32_t host_data__array__mem_size;
        uint32_t host_size__value;
        pos = (start + 8);
        if (SPICE_UNLIKELY(pos + 4 > message_end)) {
            goto error;
        }
        host_data__value = read_uint32(pos);
        if (SPICE_UNLIKELY(host_data__value == 0)) {
            goto error;
        }
        if (SPICE_UNLIKELY(message_start + host_data__value >= message_end)) {
            goto error;
        }
        pos = start + 4;
        if (SPICE_UNLIKELY(pos + 4 > message_end)) {
            goto error;
        }
        host_size__value = read_uint32(pos);
        host_data__array__nelements = host_size__value;

        host_data__array__nw_size = host_data__array__nelements;
        host_data__array__mem_size = sizeof(uint8_t) * host_data__array__nelements;
        if (SPICE_UNLIKELY(message_start + host_data__value + host_data__array__nw_size > message_end)) {
            goto error;
        }
        host_data__extra_size = host_data__array__mem_size + /* for alignment */ 3;
    }

    { /* pub_key_data */
        uint32_t pub_key_data__value;
        uint32_t pub_key_data__array__nw_size;
        uint32_t pub_key_data__array__mem_size;
        uint32_t pub_key_size__value;
        pos = (start + 18);
        if (SPICE_UNLIKELY(pos + 4 > message_end)) {
            goto error;
        }
        pub_key_data__value = read_uint32(pos);
        if (SPICE_UNLIKELY(pub_key_data__value == 0)) {
            goto error;
        }
        if (SPICE_UNLIKELY(message_start + pub_key_data__value >= message_end)) {
            goto error;
        }
        pos = start + 14;
        if (SPICE_UNLIKELY(pos + 4 > message_end)) {
            goto error;
        }
        pub_key_size__value = read_uint32(pos);
        pub_key_data__array__nelements = pub_key_size__value;

        pub_key_data__array__nw_size = pub_key_data__array__nelements;
        pub_key_data__array__mem_size = sizeof(uint8_t) * pub_key_data__array__nelements;
        if (SPICE_UNLIKELY(message_start + pub_key_data__value + pub_key_data__array__nw_size > message_end)) {
            goto error;
        }
        pub_key_data__extra_size = pub_key_data__array__mem_size + /* for alignment */ 3;
    }

    nw_size = 22;
    mem_size = sizeof(SpiceMsgMainMigrationBegin) + host_data__extra_size + pub_key_data__extra_size;

    /* Check if message fits in reported side */
    if (start + nw_size > message_end) {
        return NULL;
    }

    /* Validated extents and calculated size */
    data = (uint8_t *)malloc(mem_size);
    if (SPICE_UNLIKELY(data == NULL)) {
        goto error;
    }
    end = data + sizeof(SpiceMsgMainMigrationBegin);
    in = start;

    out = (SpiceMsgMainMigrationBegin *)data;

    out->port = consume_uint16(&in);
    out->sport = consume_uint16(&in);
    out->host_size = consume_uint32(&in);
    ptr_info[n_ptr].offset = consume_uint32(&in);
    ptr_info[n_ptr].parse = parse_array_uint8;
    ptr_info[n_ptr].dest = (void **)&out->host_data;
    ptr_info[n_ptr].nelements = host_data__array__nelements;
    n_ptr++;
    out->pub_key_type = consume_uint16(&in);
    out->pub_key_size = consume_uint32(&in);
    ptr_info[n_ptr].offset = consume_uint32(&in);
    ptr_info[n_ptr].parse = parse_array_uint8;
    ptr_info[n_ptr].dest = (void **)&out->pub_key_data;
    ptr_info[n_ptr].nelements = pub_key_data__array__nelements;
    n_ptr++;

    assert(in <= message_end);

    for (i = 0; i < n_ptr; i++) {
        if (ptr_info[i].offset == 0) {
            *ptr_info[i].dest = NULL;
        } else {
            /* Align to 32 bit */
            end = (uint8_t *)SPICE_ALIGN((size_t)end, 4);
            *ptr_info[i].dest = (void *)end;
            end = ptr_info[i].parse(message_start, message_end, end, &ptr_info[i], minor);
            if (SPICE_UNLIKELY(end == NULL)) {
                goto error;
            }
        }
    }

    assert(end <= data + mem_size);

    *size = end - data;
    *free_message = (message_destructor_t) free;
    return data;

   error:
    if (data != NULL) {
        free(data);
    }
    return NULL;
}

static uint8_t * parse_SpiceMsgEmpty(uint8_t *message_start, uint8_t *message_end, int minor, size_t *size, message_destructor_t *free_message)
{
    SPICE_GNUC_UNUSED uint8_t *pos;
    uint8_t *start = message_start;
    uint8_t *data = NULL;
    size_t mem_size, nw_size;
    uint8_t *in, *end;
    SpiceMsgEmpty *out;

    nw_size = 0;
    mem_size = sizeof(SpiceMsgEmpty);

    /* Check if message fits in reported side */
    if (start + nw_size > message_end) {
        return NULL;
    }

    /* Validated extents and calculated size */
    data = (uint8_t *)malloc(mem_size);
    if (SPICE_UNLIKELY(data == NULL)) {
        goto error;
    }
    end = data + sizeof(SpiceMsgEmpty);
    in = start;

    out = (SpiceMsgEmpty *)data;


    assert(in <= message_end);
    assert(end <= data + mem_size);

    *size = end - data;
    *free_message = (message_destructor_t) free;
    return data;

   error:
    if (data != NULL) {
        free(data);
    }
    return NULL;
}

static uint8_t * parse_msg_main_init(uint8_t *message_start, uint8_t *message_end, int minor, size_t *size, message_destructor_t *free_message)
{
    SPICE_GNUC_UNUSED uint8_t *pos;
    uint8_t *start = message_start;
    uint8_t *data = NULL;
    size_t mem_size, nw_size;
    uint8_t *in, *end;
    SpiceMsgMainInit *out;

    nw_size = 32;
    mem_size = sizeof(SpiceMsgMainInit);

    /* Check if message fits in reported side */
    if (start + nw_size > message_end) {
        return NULL;
    }

    /* Validated extents and calculated size */
    data = (uint8_t *)malloc(mem_size);
    if (SPICE_UNLIKELY(data == NULL)) {
        goto error;
    }
    end = data + sizeof(SpiceMsgMainInit);
    in = start;

    out = (SpiceMsgMainInit *)data;

    out->session_id = consume_uint32(&in);
    out->display_channels_hint = consume_uint32(&in);
    out->supported_mouse_modes = consume_uint32(&in);
    out->current_mouse_mode = consume_uint32(&in);
    out->agent_connected = consume_uint32(&in);
    out->agent_tokens = consume_uint32(&in);
    out->multi_media_time = consume_uint32(&in);
    out->ram_hint = consume_uint32(&in);

    assert(in <= message_end);
    assert(end <= data + mem_size);

    *size = end - data;
    *free_message = (message_destructor_t) free;
    return data;

   error:
    if (data != NULL) {
        free(data);
    }
    return NULL;
}

static uint8_t * parse_msg_main_channels_list(uint8_t *message_start, uint8_t *message_end, int minor, size_t *size, message_destructor_t *free_message)
{
    SPICE_GNUC_UNUSED uint8_t *pos;
    uint8_t *start = message_start;
    uint8_t *data = NULL;
    size_t mem_size, nw_size;
    uint8_t *in, *end;
    size_t channels__nw_size, channels__mem_size;
    uint32_t channels__nelements;
    SpiceMsgChannels *out;
    uint32_t i;

    { /* channels */
        uint32_t num_of_channels__value;
        pos = start + 0;
        if (SPICE_UNLIKELY(pos + 4 > message_end)) {
            goto error;
        }
        num_of_channels__value = read_uint32(pos);
        channels__nelements = num_of_channels__value;

        channels__nw_size = (2) * channels__nelements;
        channels__mem_size = sizeof(SpiceChannelId) * channels__nelements;
    }

    nw_size = 4 + channels__nw_size;
    mem_size = sizeof(SpiceMsgChannels) + channels__mem_size;

    /* Check if message fits in reported side */
    if (start + nw_size > message_end) {
        return NULL;
    }

    /* Validated extents and calculated size */
    data = (uint8_t *)malloc(mem_size);
    if (SPICE_UNLIKELY(data == NULL)) {
        goto error;
    }
    end = data + sizeof(SpiceMsgChannels);
    in = start;

    out = (SpiceMsgChannels *)data;

    out->num_of_channels = consume_uint32(&in);
    for (i = 0; i < channels__nelements; i++) {
        SpiceChannelId *out2;
        out2 = (SpiceChannelId *)end;
        end += sizeof(SpiceChannelId);

        out2->type = consume_uint8(&in);
        out2->id = consume_uint8(&in);
    }

    assert(in <= message_end);
    assert(end <= data + mem_size);

    *size = end - data;
    *free_message = (message_destructor_t) free;
    return data;

   error:
    if (data != NULL) {
        free(data);
    }
    return NULL;
}

static uint8_t * parse_msg_main_mouse_mode(uint8_t *message_start, uint8_t *message_end, int minor, size_t *size, message_destructor_t *free_message)
{
    SPICE_GNUC_UNUSED uint8_t *pos;
    uint8_t *start = message_start;
    uint8_t *data = NULL;
    size_t mem_size, nw_size;
    uint8_t *in, *end;
    SpiceMsgMainMouseMode *out;

    nw_size = 4;
    mem_size = sizeof(SpiceMsgMainMouseMode);

    /* Check if message fits in reported side */
    if (start + nw_size > message_end) {
        return NULL;
    }

    /* Validated extents and calculated size */
    data = (uint8_t *)malloc(mem_size);
    if (SPICE_UNLIKELY(data == NULL)) {
        goto error;
    }
    end = data + sizeof(SpiceMsgMainMouseMode);
    in = start;

    out = (SpiceMsgMainMouseMode *)data;

    out->supported_modes = consume_uint16(&in);
    out->current_mode = consume_uint16(&in);

    assert(in <= message_end);
    assert(end <= data + mem_size);

    *size = end - data;
    *free_message = (message_destructor_t) free;
    return data;

   error:
    if (data != NULL) {
        free(data);
    }
    return NULL;
}

static uint8_t * parse_msg_main_multi_media_time(uint8_t *message_start, uint8_t *message_end, int minor, size_t *size, message_destructor_t *free_message)
{
    SPICE_GNUC_UNUSED uint8_t *pos;
    uint8_t *start = message_start;
    uint8_t *data = NULL;
    size_t mem_size, nw_size;
    uint8_t *in, *end;
    SpiceMsgMainMultiMediaTime *out;

    nw_size = 4;
    mem_size = sizeof(SpiceMsgMainMultiMediaTime);

    /* Check if message fits in reported side */
    if (start + nw_size > message_end) {
        return NULL;
    }

    /* Validated extents and calculated size */
    data = (uint8_t *)malloc(mem_size);
    if (SPICE_UNLIKELY(data == NULL)) {
        goto error;
    }
    end = data + sizeof(SpiceMsgMainMultiMediaTime);
    in = start;

    out = (SpiceMsgMainMultiMediaTime *)data;

    out->time = consume_uint32(&in);

    assert(in <= message_end);
    assert(end <= data + mem_size);

    *size = end - data;
    *free_message = (message_destructor_t) free;
    return data;

   error:
    if (data != NULL) {
        free(data);
    }
    return NULL;
}

static uint8_t * parse_msg_main_agent_disconnected(uint8_t *message_start, uint8_t *message_end, int minor, size_t *size, message_destructor_t *free_message)
{
    SPICE_GNUC_UNUSED uint8_t *pos;
    uint8_t *start = message_start;
    uint8_t *data = NULL;
    size_t mem_size, nw_size;
    uint8_t *in, *end;
    SpiceMsgMainAgentDisconnect *out;

    nw_size = 4;
    mem_size = sizeof(SpiceMsgMainAgentDisconnect);

    /* Check if message fits in reported side */
    if (start + nw_size > message_end) {
        return NULL;
    }

    /* Validated extents and calculated size */
    data = (uint8_t *)malloc(mem_size);
    if (SPICE_UNLIKELY(data == NULL)) {
        goto error;
    }
    end = data + sizeof(SpiceMsgMainAgentDisconnect);
    in = start;

    out = (SpiceMsgMainAgentDisconnect *)data;

    out->error_code = consume_uint32(&in);

    assert(in <= message_end);
    assert(end <= data + mem_size);

    *size = end - data;
    *free_message = (message_destructor_t) free;
    return data;

   error:
    if (data != NULL) {
        free(data);
    }
    return NULL;
}

static uint8_t * parse_msg_main_agent_token(uint8_t *message_start, uint8_t *message_end, int minor, size_t *size, message_destructor_t *free_message)
{
    SPICE_GNUC_UNUSED uint8_t *pos;
    uint8_t *start = message_start;
    uint8_t *data = NULL;
    size_t mem_size, nw_size;
    uint8_t *in, *end;
    SpiceMsgMainAgentTokens *out;

    nw_size = 4;
    mem_size = sizeof(SpiceMsgMainAgentTokens);

    /* Check if message fits in reported side */
    if (start + nw_size > message_end) {
        return NULL;
    }

    /* Validated extents and calculated size */
    data = (uint8_t *)malloc(mem_size);
    if (SPICE_UNLIKELY(data == NULL)) {
        goto error;
    }
    end = data + sizeof(SpiceMsgMainAgentTokens);
    in = start;

    out = (SpiceMsgMainAgentTokens *)data;

    out->num_tokens = consume_uint32(&in);

    assert(in <= message_end);
    assert(end <= data + mem_size);

    *size = end - data;
    *free_message = (message_destructor_t) free;
    return data;

   error:
    if (data != NULL) {
        free(data);
    }
    return NULL;
}

static uint8_t * parse_msg_main_migrate_switch_host(uint8_t *message_start, uint8_t *message_end, int minor, size_t *size, message_destructor_t *free_message)
{
    SPICE_GNUC_UNUSED uint8_t *pos;
    uint8_t *start = message_start;
    uint8_t *data = NULL;
    size_t mem_size, nw_size;
    uint8_t *in, *end;
    SPICE_GNUC_UNUSED intptr_t ptr_size;
    uint32_t n_ptr=0;
    PointerInfo ptr_info[2];
    size_t host_data__extra_size;
    uint32_t host_data__array__nelements;
    size_t cert_subject_data__extra_size;
    uint32_t cert_subject_data__array__nelements;
    SpiceMsgMainMigrationSwitchHost *out;
    uint32_t i;

    { /* host_data */
        uint32_t host_data__value;
        uint32_t host_data__array__nw_size;
        uint32_t host_data__array__mem_size;
        uint32_t host_size__value;
        pos = (start + 8);
        if (SPICE_UNLIKELY(pos + 4 > message_end)) {
            goto error;
        }
        host_data__value = read_uint32(pos);
        if (SPICE_UNLIKELY(message_start + host_data__value >= message_end)) {
            goto error;
        }
        pos = start + 4;
        if (SPICE_UNLIKELY(pos + 4 > message_end)) {
            goto error;
        }
        host_size__value = read_uint32(pos);
        host_data__array__nelements = host_size__value;

        host_data__array__nw_size = host_data__array__nelements;
        host_data__array__mem_size = sizeof(uint8_t) * host_data__array__nelements;
        if (SPICE_UNLIKELY(message_start + host_data__value + host_data__array__nw_size > message_end)) {
            goto error;
        }
        host_data__extra_size = host_data__array__mem_size + /* for alignment */ 3;
    }

    { /* cert_subject_data */
        uint32_t cert_subject_data__value;
        uint32_t cert_subject_data__array__nw_size;
        uint32_t cert_subject_data__array__mem_size;
        uint32_t cert_subject_size__value;
        pos = (start + 16);
        if (SPICE_UNLIKELY(pos + 4 > message_end)) {
            goto error;
        }
        cert_subject_data__value = read_uint32(pos);
        if (SPICE_UNLIKELY(message_start + cert_subject_data__value >= message_end)) {
            goto error;
        }
        pos = start + 12;
        if (SPICE_UNLIKELY(pos + 4 > message_end)) {
            goto error;
        }
        cert_subject_size__value = read_uint32(pos);
        cert_subject_data__array__nelements = cert_subject_size__value;

        cert_subject_data__array__nw_size = cert_subject_data__array__nelements;
        cert_subject_data__array__mem_size = sizeof(uint8_t) * cert_subject_data__array__nelements;
        if (SPICE_UNLIKELY(message_start + cert_subject_data__value + cert_subject_data__array__nw_size > message_end)) {
            goto error;
        }
        cert_subject_data__extra_size = cert_subject_data__array__mem_size + /* for alignment */ 3;
    }

    nw_size = 20;
    mem_size = sizeof(SpiceMsgMainMigrationSwitchHost) + host_data__extra_size + cert_subject_data__extra_size;

    /* Check if message fits in reported side */
    if (start + nw_size > message_end) {
        return NULL;
    }

    /* Validated extents and calculated size */
    data = (uint8_t *)malloc(mem_size);
    if (SPICE_UNLIKELY(data == NULL)) {
        goto error;
    }
    end = data + sizeof(SpiceMsgMainMigrationSwitchHost);
    in = start;

    out = (SpiceMsgMainMigrationSwitchHost *)data;

    out->port = consume_uint16(&in);
    out->sport = consume_uint16(&in);
    out->host_size = consume_uint32(&in);
    ptr_info[n_ptr].offset = consume_uint32(&in);
    ptr_info[n_ptr].parse = parse_array_uint8;
    ptr_info[n_ptr].dest = (void **)&out->host_data;
    ptr_info[n_ptr].nelements = host_data__array__nelements;
    n_ptr++;
    out->cert_subject_size = consume_uint32(&in);
    ptr_info[n_ptr].offset = consume_uint32(&in);
    ptr_info[n_ptr].parse = parse_array_uint8;
    ptr_info[n_ptr].dest = (void **)&out->cert_subject_data;
    ptr_info[n_ptr].nelements = cert_subject_data__array__nelements;
    n_ptr++;

    assert(in <= message_end);

    for (i = 0; i < n_ptr; i++) {
        if (ptr_info[i].offset == 0) {
            *ptr_info[i].dest = NULL;
        } else {
            /* Align to 32 bit */
            end = (uint8_t *)SPICE_ALIGN((size_t)end, 4);
            *ptr_info[i].dest = (void *)end;
            end = ptr_info[i].parse(message_start, message_end, end, &ptr_info[i], minor);
            if (SPICE_UNLIKELY(end == NULL)) {
                goto error;
            }
        }
    }

    assert(end <= data + mem_size);

    *size = end - data;
    *free_message = (message_destructor_t) free;
    return data;

   error:
    if (data != NULL) {
        free(data);
    }
    return NULL;
}

static uint8_t * parse_MainChannel_msg(uint8_t *message_start, uint8_t *message_end, uint16_t message_type, int minor, size_t *size_out, message_destructor_t *free_message)
{
    static parse_msg_func_t funcs1[7] =  {
        parse_msg_migrate,
        parse_SpiceMsgData,
        parse_msg_set_ack,
        parse_msg_ping,
        parse_msg_wait_for_channels,
        parse_msg_disconnecting,
        parse_msg_notify
    };
    static parse_msg_func_t funcs2[11] =  {
        parse_msg_main_migrate_begin,
        parse_SpiceMsgEmpty,
        parse_msg_main_init,
        parse_msg_main_channels_list,
        parse_msg_main_mouse_mode,
        parse_msg_main_multi_media_time,
        parse_SpiceMsgEmpty,
        parse_msg_main_agent_disconnected,
        parse_SpiceMsgData,
        parse_msg_main_agent_token,
        parse_msg_main_migrate_switch_host
    };
    if (message_type >= 1 && message_type < 8) {
        return funcs1[message_type-1](message_start, message_end, minor, size_out, free_message);
    } else if (message_type >= 101 && message_type < 112) {
        return funcs2[message_type-101](message_start, message_end, minor, size_out, free_message);
    }
    return NULL;
}



static uint8_t * parse_msg_display_mode(uint8_t *message_start, uint8_t *message_end, int minor, size_t *size, message_destructor_t *free_message)
{
    SPICE_GNUC_UNUSED uint8_t *pos;
    uint8_t *start = message_start;
    uint8_t *data = NULL;
    size_t mem_size, nw_size;
    uint8_t *in, *end;
    SpiceMsgDisplayMode *out;

    nw_size = 12;
    mem_size = sizeof(SpiceMsgDisplayMode);

    /* Check if message fits in reported side */
    if (start + nw_size > message_end) {
        return NULL;
    }

    /* Validated extents and calculated size */
    data = (uint8_t *)malloc(mem_size);
    if (SPICE_UNLIKELY(data == NULL)) {
        goto error;
    }
    end = data + sizeof(SpiceMsgDisplayMode);
    in = start;

    out = (SpiceMsgDisplayMode *)data;

    out->x_res = consume_uint32(&in);
    out->y_res = consume_uint32(&in);
    out->bits = consume_uint32(&in);

    assert(in <= message_end);
    assert(end <= data + mem_size);

    *size = end - data;
    *free_message = (message_destructor_t) free;
    return data;

   error:
    if (data != NULL) {
        free(data);
    }
    return NULL;
}

static uint8_t * parse_struct_SpiceClipRects(uint8_t *message_start, uint8_t *message_end, uint8_t *struct_data, PointerInfo *this_ptr_info, int minor)
{
    uint8_t *in = message_start + this_ptr_info->offset;
    uint8_t *end;
    SpiceClipRects *out;
    uint32_t rects__nelements;
    uint32_t i;

    end = struct_data + sizeof(SpiceClipRects);
    out = (SpiceClipRects *)struct_data;

    out->num_rects = consume_uint32(&in);
    rects__nelements = out->num_rects;
    for (i = 0; i < rects__nelements; i++) {
        SpiceRect *out2;
        out2 = (SpiceRect *)end;
        end += sizeof(SpiceRect);

        out2->top = consume_int32(&in);
        out2->left = consume_int32(&in);
        out2->bottom = consume_int32(&in);
        out2->right = consume_int32(&in);
    }
    return end;
}

static uint8_t * parse_msg_display_copy_bits(uint8_t *message_start, uint8_t *message_end, int minor, size_t *size, message_destructor_t *free_message)
{
    SPICE_GNUC_UNUSED uint8_t *pos;
    uint8_t *start = message_start;
    uint8_t *data = NULL;
    size_t mem_size, nw_size;
    uint8_t *in, *end;
    SPICE_GNUC_UNUSED intptr_t ptr_size;
    uint32_t n_ptr=0;
    PointerInfo ptr_info[1];
    size_t base__nw_size, base__extra_size;
    uint32_t rects__saved_size = 0;
    SpiceMsgDisplayCopyBits *out;
    uint32_t i;

    { /* base */
        SPICE_GNUC_UNUSED uint8_t *start2 = (start + 0);
        size_t clip__nw_size, clip__extra_size;
        { /* clip */
            SPICE_GNUC_UNUSED uint8_t *start3 = (start2 + 20);
            size_t u__nw_size, u__extra_size;
            uint8_t type__value;
            { /* u */
                uint32_t u__mem_size;
                pos = start3 + 0;
                if (SPICE_UNLIKELY(pos + 1 > message_end)) {
                    goto error;
                }
                type__value = read_uint8(pos);
                if (type__value == SPICE_CLIP_TYPE_RECTS) {
                    SPICE_GNUC_UNUSED uint8_t *start4 = (start3 + 1);
                    size_t rects__nw_size, rects__mem_size;
                    uint32_t rects__nelements;
                    { /* rects */
                        uint32_t num_rects__value;
                        pos = start4 + 0;
                        if (SPICE_UNLIKELY(pos + 4 > message_end)) {
                            goto error;
                        }
                        num_rects__value = read_uint32(pos);
                        rects__nelements = num_rects__value;

                        rects__nw_size = (16) * rects__nelements;
                        rects__mem_size = sizeof(SpiceRect) * rects__nelements;
                    }

                    u__nw_size = 4 + rects__nw_size;
                    u__mem_size = sizeof(SpiceClipRects) + rects__mem_size;
                    rects__saved_size = u__nw_size;
                    u__extra_size = u__mem_size;
                } else {
                    u__nw_size = 0;
                    u__extra_size = 0;
                }

            }

            clip__nw_size = 1 + u__nw_size;
            clip__extra_size = u__extra_size;
        }

        base__nw_size = 20 + clip__nw_size;
        base__extra_size = clip__extra_size;
    }

    nw_size = 8 + base__nw_size;
    mem_size = sizeof(SpiceMsgDisplayCopyBits) + base__extra_size;

    /* Check if message fits in reported side */
    if (start + nw_size > message_end) {
        return NULL;
    }

    /* Validated extents and calculated size */
    data = (uint8_t *)malloc(mem_size);
    if (SPICE_UNLIKELY(data == NULL)) {
        goto error;
    }
    end = data + sizeof(SpiceMsgDisplayCopyBits);
    in = start;

    out = (SpiceMsgDisplayCopyBits *)data;

    /* base */ {
        out->base.surface_id = consume_uint32(&in);
        /* box */ {
            out->base.box.top = consume_int32(&in);
            out->base.box.left = consume_int32(&in);
            out->base.box.bottom = consume_int32(&in);
            out->base.box.right = consume_int32(&in);
        }
        /* clip */ {
            out->base.clip.type = consume_uint8(&in);
            if (out->base.clip.type == SPICE_CLIP_TYPE_RECTS) {
                ptr_info[n_ptr].offset = in - start;
                ptr_info[n_ptr].parse = parse_struct_SpiceClipRects;
                ptr_info[n_ptr].dest = (void **)&out->base.clip.rects;
                n_ptr++;
                in += rects__saved_size;
            }
        }
    }
    /* src_pos */ {
        out->src_pos.x = consume_int32(&in);
        out->src_pos.y = consume_int32(&in);
    }

    assert(in <= message_end);

    for (i = 0; i < n_ptr; i++) {
        if (ptr_info[i].offset == 0) {
            *ptr_info[i].dest = NULL;
        } else {
            /* Align to 32 bit */
            end = (uint8_t *)SPICE_ALIGN((size_t)end, 4);
            *ptr_info[i].dest = (void *)end;
            end = ptr_info[i].parse(message_start, message_end, end, &ptr_info[i], minor);
            if (SPICE_UNLIKELY(end == NULL)) {
                goto error;
            }
        }
    }

    assert(end <= data + mem_size);

    *size = end - data;
    *free_message = (message_destructor_t) free;
    return data;

   error:
    if (data != NULL) {
        free(data);
    }
    return NULL;
}

static uint8_t * parse_msg_display_inval_list(uint8_t *message_start, uint8_t *message_end, int minor, size_t *size, message_destructor_t *free_message)
{
    SPICE_GNUC_UNUSED uint8_t *pos;
    uint8_t *start = message_start;
    uint8_t *data = NULL;
    size_t mem_size, nw_size;
    uint8_t *in, *end;
    size_t resources__nw_size, resources__mem_size;
    uint32_t resources__nelements;
    SpiceResourceList *out;
    uint32_t i;

    { /* resources */
        uint16_t count__value;
        pos = start + 0;
        if (SPICE_UNLIKELY(pos + 2 > message_end)) {
            goto error;
        }
        count__value = read_uint16(pos);
        resources__nelements = count__value;

        resources__nw_size = (9) * resources__nelements;
        resources__mem_size = sizeof(SpiceResourceID) * resources__nelements;
    }

    nw_size = 2 + resources__nw_size;
    mem_size = sizeof(SpiceResourceList) + resources__mem_size;

    /* Check if message fits in reported side */
    if (start + nw_size > message_end) {
        return NULL;
    }

    /* Validated extents and calculated size */
    data = (uint8_t *)malloc(mem_size);
    if (SPICE_UNLIKELY(data == NULL)) {
        goto error;
    }
    end = data + sizeof(SpiceResourceList);
    in = start;

    out = (SpiceResourceList *)data;

    out->count = consume_uint16(&in);
    for (i = 0; i < resources__nelements; i++) {
        SpiceResourceID *out2;
        out2 = (SpiceResourceID *)end;
        end += sizeof(SpiceResourceID);

        out2->type = consume_uint8(&in);
        out2->id = consume_uint64(&in);
    }

    assert(in <= message_end);
    assert(end <= data + mem_size);

    *size = end - data;
    *free_message = (message_destructor_t) free;
    return data;

   error:
    if (data != NULL) {
        free(data);
    }
    return NULL;
}

static uint8_t * parse_msg_display_inval_all_pixmaps(uint8_t *message_start, uint8_t *message_end, int minor, size_t *size, message_destructor_t *free_message)
{
    SPICE_GNUC_UNUSED uint8_t *pos;
    uint8_t *start = message_start;
    uint8_t *data = NULL;
    size_t mem_size, nw_size;
    uint8_t *in, *end;
    size_t wait_list__nw_size, wait_list__mem_size;
    uint32_t wait_list__nelements;
    SpiceMsgWaitForChannels *out;
    uint32_t i;

    { /* wait_list */
        uint8_t wait_count__value;
        pos = start + 0;
        if (SPICE_UNLIKELY(pos + 1 > message_end)) {
            goto error;
        }
        wait_count__value = read_uint8(pos);
        wait_list__nelements = wait_count__value;

        wait_list__nw_size = (10) * wait_list__nelements;
        wait_list__mem_size = sizeof(SpiceWaitForChannel) * wait_list__nelements;
    }

    nw_size = 1 + wait_list__nw_size;
    mem_size = sizeof(SpiceMsgWaitForChannels) + wait_list__mem_size;

    /* Check if message fits in reported side */
    if (start + nw_size > message_end) {
        return NULL;
    }

    /* Validated extents and calculated size */
    data = (uint8_t *)malloc(mem_size);
    if (SPICE_UNLIKELY(data == NULL)) {
        goto error;
    }
    end = data + sizeof(SpiceMsgWaitForChannels);
    in = start;

    out = (SpiceMsgWaitForChannels *)data;

    out->wait_count = consume_uint8(&in);
    for (i = 0; i < wait_list__nelements; i++) {
        SpiceWaitForChannel *out2;
        out2 = (SpiceWaitForChannel *)end;
        end += sizeof(SpiceWaitForChannel);

        out2->channel_type = consume_uint8(&in);
        out2->channel_id = consume_uint8(&in);
        out2->message_serial = consume_uint64(&in);
    }

    assert(in <= message_end);
    assert(end <= data + mem_size);

    *size = end - data;
    *free_message = (message_destructor_t) free;
    return data;

   error:
    if (data != NULL) {
        free(data);
    }
    return NULL;
}

static uint8_t * parse_msg_display_inval_palette(uint8_t *message_start, uint8_t *message_end, int minor, size_t *size, message_destructor_t *free_message)
{
    SPICE_GNUC_UNUSED uint8_t *pos;
    uint8_t *start = message_start;
    uint8_t *data = NULL;
    size_t mem_size, nw_size;
    uint8_t *in, *end;
    SpiceMsgDisplayInvalOne *out;

    nw_size = 8;
    mem_size = sizeof(SpiceMsgDisplayInvalOne);

    /* Check if message fits in reported side */
    if (start + nw_size > message_end) {
        return NULL;
    }

    /* Validated extents and calculated size */
    data = (uint8_t *)malloc(mem_size);
    if (SPICE_UNLIKELY(data == NULL)) {
        goto error;
    }
    end = data + sizeof(SpiceMsgDisplayInvalOne);
    in = start;

    out = (SpiceMsgDisplayInvalOne *)data;

    out->id = consume_uint64(&in);

    assert(in <= message_end);
    assert(end <= data + mem_size);

    *size = end - data;
    *free_message = (message_destructor_t) free;
    return data;

   error:
    if (data != NULL) {
        free(data);
    }
    return NULL;
}

static uint8_t * parse_msg_display_stream_create(uint8_t *message_start, uint8_t *message_end, int minor, size_t *size, message_destructor_t *free_message)
{
    SPICE_GNUC_UNUSED uint8_t *pos;
    uint8_t *start = message_start;
    uint8_t *data = NULL;
    size_t mem_size, nw_size;
    uint8_t *in, *end;
    SPICE_GNUC_UNUSED intptr_t ptr_size;
    uint32_t n_ptr=0;
    PointerInfo ptr_info[1];
    size_t clip__nw_size, clip__extra_size;
    uint32_t rects__saved_size = 0;
    SpiceMsgDisplayStreamCreate *out;
    uint32_t i;

    { /* clip */
        SPICE_GNUC_UNUSED uint8_t *start2 = (start + 50);
        size_t u__nw_size, u__extra_size;
        uint8_t type__value;
        { /* u */
            uint32_t u__mem_size;
            pos = start2 + 0;
            if (SPICE_UNLIKELY(pos + 1 > message_end)) {
                goto error;
            }
            type__value = read_uint8(pos);
            if (type__value == SPICE_CLIP_TYPE_RECTS) {
                SPICE_GNUC_UNUSED uint8_t *start3 = (start2 + 1);
                size_t rects__nw_size, rects__mem_size;
                uint32_t rects__nelements;
                { /* rects */
                    uint32_t num_rects__value;
                    pos = start3 + 0;
                    if (SPICE_UNLIKELY(pos + 4 > message_end)) {
                        goto error;
                    }
                    num_rects__value = read_uint32(pos);
                    rects__nelements = num_rects__value;

                    rects__nw_size = (16) * rects__nelements;
                    rects__mem_size = sizeof(SpiceRect) * rects__nelements;
                }

                u__nw_size = 4 + rects__nw_size;
                u__mem_size = sizeof(SpiceClipRects) + rects__mem_size;
                rects__saved_size = u__nw_size;
                u__extra_size = u__mem_size;
            } else {
                u__nw_size = 0;
                u__extra_size = 0;
            }

        }

        clip__nw_size = 1 + u__nw_size;
        clip__extra_size = u__extra_size;
    }

    nw_size = 50 + clip__nw_size;
    mem_size = sizeof(SpiceMsgDisplayStreamCreate) + clip__extra_size;

    /* Check if message fits in reported side */
    if (start + nw_size > message_end) {
        return NULL;
    }

    /* Validated extents and calculated size */
    data = (uint8_t *)malloc(mem_size);
    if (SPICE_UNLIKELY(data == NULL)) {
        goto error;
    }
    end = data + sizeof(SpiceMsgDisplayStreamCreate);
    in = start;

    out = (SpiceMsgDisplayStreamCreate *)data;

    out->surface_id = consume_uint32(&in);
    out->id = consume_uint32(&in);
    out->flags = consume_uint8(&in);
    out->codec_type = consume_uint8(&in);
    out->stamp = consume_uint64(&in);
    out->stream_width = consume_uint32(&in);
    out->stream_height = consume_uint32(&in);
    out->src_width = consume_uint32(&in);
    out->src_height = consume_uint32(&in);
    /* dest */ {
        out->dest.top = consume_int32(&in);
        out->dest.left = consume_int32(&in);
        out->dest.bottom = consume_int32(&in);
        out->dest.right = consume_int32(&in);
    }
    /* clip */ {
        out->clip.type = consume_uint8(&in);
        if (out->clip.type == SPICE_CLIP_TYPE_RECTS) {
            ptr_info[n_ptr].offset = in - start;
            ptr_info[n_ptr].parse = parse_struct_SpiceClipRects;
            ptr_info[n_ptr].dest = (void **)&out->clip.rects;
            n_ptr++;
            in += rects__saved_size;
        }
    }

    assert(in <= message_end);

    for (i = 0; i < n_ptr; i++) {
        if (ptr_info[i].offset == 0) {
            *ptr_info[i].dest = NULL;
        } else {
            /* Align to 32 bit */
            end = (uint8_t *)SPICE_ALIGN((size_t)end, 4);
            *ptr_info[i].dest = (void *)end;
            end = ptr_info[i].parse(message_start, message_end, end, &ptr_info[i], minor);
            if (SPICE_UNLIKELY(end == NULL)) {
                goto error;
            }
        }
    }

    assert(end <= data + mem_size);

    *size = end - data;
    *free_message = (message_destructor_t) free;
    return data;

   error:
    if (data != NULL) {
        free(data);
    }
    return NULL;
}

static uint8_t * parse_msg_display_stream_data(uint8_t *message_start, uint8_t *message_end, int minor, size_t *size, message_destructor_t *free_message)
{
    SPICE_GNUC_UNUSED uint8_t *pos;
    uint8_t *start = message_start;
    uint8_t *data = NULL;
    size_t mem_size, nw_size;
    uint8_t *in, *end;
    size_t data__nw_size, data__mem_size;
    uint32_t data__nelements;
    SpiceMsgDisplayStreamData *out;

    { /* data */
        uint32_t data_size__value;
        pos = start + 8;
        if (SPICE_UNLIKELY(pos + 4 > message_end)) {
            goto error;
        }
        data_size__value = read_uint32(pos);
        data__nelements = data_size__value;

        data__nw_size = data__nelements;
        data__mem_size = sizeof(uint8_t) * data__nelements;
    }

    nw_size = 12 + data__nw_size;
    mem_size = sizeof(SpiceMsgDisplayStreamData) + data__mem_size;

    /* Check if message fits in reported side */
    if (start + nw_size > message_end) {
        return NULL;
    }

    /* Validated extents and calculated size */
    data = (uint8_t *)malloc(mem_size);
    if (SPICE_UNLIKELY(data == NULL)) {
        goto error;
    }
    end = data + sizeof(SpiceMsgDisplayStreamData);
    in = start;

    out = (SpiceMsgDisplayStreamData *)data;

    out->id = consume_uint32(&in);
    out->multi_media_time = consume_uint32(&in);
    out->data_size = consume_uint32(&in);
    memcpy(out->data, in, data__nelements);
    in += data__nelements;
    end += data__nelements;

    assert(in <= message_end);
    assert(end <= data + mem_size);

    *size = end - data;
    *free_message = (message_destructor_t) free;
    return data;

   error:
    if (data != NULL) {
        free(data);
    }
    return NULL;
}

static uint8_t * parse_msg_display_stream_clip(uint8_t *message_start, uint8_t *message_end, int minor, size_t *size, message_destructor_t *free_message)
{
    SPICE_GNUC_UNUSED uint8_t *pos;
    uint8_t *start = message_start;
    uint8_t *data = NULL;
    size_t mem_size, nw_size;
    uint8_t *in, *end;
    SPICE_GNUC_UNUSED intptr_t ptr_size;
    uint32_t n_ptr=0;
    PointerInfo ptr_info[1];
    size_t clip__nw_size, clip__extra_size;
    uint32_t rects__saved_size = 0;
    SpiceMsgDisplayStreamClip *out;
    uint32_t i;

    { /* clip */
        SPICE_GNUC_UNUSED uint8_t *start2 = (start + 4);
        size_t u__nw_size, u__extra_size;
        uint8_t type__value;
        { /* u */
            uint32_t u__mem_size;
            pos = start2 + 0;
            if (SPICE_UNLIKELY(pos + 1 > message_end)) {
                goto error;
            }
            type__value = read_uint8(pos);
            if (type__value == SPICE_CLIP_TYPE_RECTS) {
                SPICE_GNUC_UNUSED uint8_t *start3 = (start2 + 1);
                size_t rects__nw_size, rects__mem_size;
                uint32_t rects__nelements;
                { /* rects */
                    uint32_t num_rects__value;
                    pos = start3 + 0;
                    if (SPICE_UNLIKELY(pos + 4 > message_end)) {
                        goto error;
                    }
                    num_rects__value = read_uint32(pos);
                    rects__nelements = num_rects__value;

                    rects__nw_size = (16) * rects__nelements;
                    rects__mem_size = sizeof(SpiceRect) * rects__nelements;
                }

                u__nw_size = 4 + rects__nw_size;
                u__mem_size = sizeof(SpiceClipRects) + rects__mem_size;
                rects__saved_size = u__nw_size;
                u__extra_size = u__mem_size;
            } else {
                u__nw_size = 0;
                u__extra_size = 0;
            }

        }

        clip__nw_size = 1 + u__nw_size;
        clip__extra_size = u__extra_size;
    }

    nw_size = 4 + clip__nw_size;
    mem_size = sizeof(SpiceMsgDisplayStreamClip) + clip__extra_size;

    /* Check if message fits in reported side */
    if (start + nw_size > message_end) {
        return NULL;
    }

    /* Validated extents and calculated size */
    data = (uint8_t *)malloc(mem_size);
    if (SPICE_UNLIKELY(data == NULL)) {
        goto error;
    }
    end = data + sizeof(SpiceMsgDisplayStreamClip);
    in = start;

    out = (SpiceMsgDisplayStreamClip *)data;

    out->id = consume_uint32(&in);
    /* clip */ {
        out->clip.type = consume_uint8(&in);
        if (out->clip.type == SPICE_CLIP_TYPE_RECTS) {
            ptr_info[n_ptr].offset = in - start;
            ptr_info[n_ptr].parse = parse_struct_SpiceClipRects;
            ptr_info[n_ptr].dest = (void **)&out->clip.rects;
            n_ptr++;
            in += rects__saved_size;
        }
    }

    assert(in <= message_end);

    for (i = 0; i < n_ptr; i++) {
        if (ptr_info[i].offset == 0) {
            *ptr_info[i].dest = NULL;
        } else {
            /* Align to 32 bit */
            end = (uint8_t *)SPICE_ALIGN((size_t)end, 4);
            *ptr_info[i].dest = (void *)end;
            end = ptr_info[i].parse(message_start, message_end, end, &ptr_info[i], minor);
            if (SPICE_UNLIKELY(end == NULL)) {
                goto error;
            }
        }
    }

    assert(end <= data + mem_size);

    *size = end - data;
    *free_message = (message_destructor_t) free;
    return data;

   error:
    if (data != NULL) {
        free(data);
    }
    return NULL;
}

static uint8_t * parse_msg_display_stream_destroy(uint8_t *message_start, uint8_t *message_end, int minor, size_t *size, message_destructor_t *free_message)
{
    SPICE_GNUC_UNUSED uint8_t *pos;
    uint8_t *start = message_start;
    uint8_t *data = NULL;
    size_t mem_size, nw_size;
    uint8_t *in, *end;
    SpiceMsgDisplayStreamDestroy *out;

    nw_size = 4;
    mem_size = sizeof(SpiceMsgDisplayStreamDestroy);

    /* Check if message fits in reported side */
    if (start + nw_size > message_end) {
        return NULL;
    }

    /* Validated extents and calculated size */
    data = (uint8_t *)malloc(mem_size);
    if (SPICE_UNLIKELY(data == NULL)) {
        goto error;
    }
    end = data + sizeof(SpiceMsgDisplayStreamDestroy);
    in = start;

    out = (SpiceMsgDisplayStreamDestroy *)data;

    out->id = consume_uint32(&in);

    assert(in <= message_end);
    assert(end <= data + mem_size);

    *size = end - data;
    *free_message = (message_destructor_t) free;
    return data;

   error:
    if (data != NULL) {
        free(data);
    }
    return NULL;
}

static intptr_t validate_SpicePalette(uint8_t *message_start, uint8_t *message_end, uint64_t offset, int minor)
{
    uint8_t *start = message_start + offset;
    SPICE_GNUC_UNUSED uint8_t *pos;
    size_t mem_size, nw_size;
    size_t ents__nw_size, ents__mem_size;
    uint32_t ents__nelements;

    if (offset == 0) {
        return 0;
    }

    if (SPICE_UNLIKELY(start >= message_end)) {
        goto error;
    }

    { /* ents */
        uint16_t num_ents__value;
        pos = start + 8;
        if (SPICE_UNLIKELY(pos + 2 > message_end)) {
            goto error;
        }
        num_ents__value = read_uint16(pos);
        ents__nelements = num_ents__value;

        ents__nw_size = (4) * ents__nelements;
        ents__mem_size = sizeof(uint32_t) * ents__nelements;
    }

    nw_size = 10 + ents__nw_size;
    mem_size = sizeof(SpicePalette) + ents__mem_size;

    /* Check if struct fits in reported side */
    if (SPICE_UNLIKELY(start + nw_size > message_end)) {
        goto error;
    }
    return mem_size;

   error:
    return -1;
}

static intptr_t validate_SpiceImage(uint8_t *message_start, uint8_t *message_end, uint64_t offset, int minor)
{
    uint8_t *start = message_start + offset;
    SPICE_GNUC_UNUSED uint8_t *pos;
    size_t mem_size, nw_size;
    SPICE_GNUC_UNUSED intptr_t ptr_size;
    size_t u__nw_size, u__extra_size;
    uint8_t descriptor_type__value;

    if (offset == 0) {
        return 0;
    }

    if (SPICE_UNLIKELY(start >= message_end)) {
        goto error;
    }

    { /* u */
        pos = start + 8;
        if (SPICE_UNLIKELY(pos + 1 > message_end)) {
            goto error;
        }
        descriptor_type__value = read_uint8(pos);
        if (descriptor_type__value == SPICE_IMAGE_TYPE_BITMAP) {
            SPICE_GNUC_UNUSED uint8_t *start2 = (start + 18);
            size_t pal__nw_size, pal__extra_size;
            uint8_t flags__value;
            size_t data__nw_size, data__extra_size;
            uint32_t data__nelements;
            { /* pal */
                pos = start2 + 1;
                if (SPICE_UNLIKELY(pos + 1 > message_end)) {
                    goto error;
                }
                flags__value = read_uint8(pos);
                if ((flags__value & SPICE_BITMAP_FLAGS_PAL_FROM_CACHE)) {
                    pal__nw_size = 8;
                    pal__extra_size = 0;
                } else if (1) {
                    uint32_t pal_palette__value;
                    pal__nw_size = 4;
                    pos = (start2 + 14);
                    if (SPICE_UNLIKELY(pos + 4 > message_end)) {
                        goto error;
                    }
                    pal_palette__value = read_uint32(pos);
                    ptr_size = validate_SpicePalette(message_start, message_end, pal_palette__value, minor);
                    if (SPICE_UNLIKELY(ptr_size < 0)) {
                        goto error;
                    }
                    pal__extra_size = ptr_size + /* for alignment */ 3;
                } else {
                    pal__nw_size = 0;
                    pal__extra_size = 0;
                }

            }

            { /* data */
                uint32_t stride__value;
                uint32_t y__value;
                pos = start2 + 10;
                if (SPICE_UNLIKELY(pos + 4 > message_end)) {
                    goto error;
                }
                stride__value = read_uint32(pos);
                pos = start2 + 6;
                if (SPICE_UNLIKELY(pos + 4 > message_end)) {
                    goto error;
                }
                y__value = read_uint32(pos);
                data__nelements = stride__value * y__value;

                data__nw_size = data__nelements;
                data__extra_size = sizeof(SpiceChunks) + sizeof(SpiceChunk);
            }

            u__nw_size = 14 + pal__nw_size + data__nw_size;
            u__extra_size = pal__extra_size + data__extra_size;
        } else if (descriptor_type__value == SPICE_IMAGE_TYPE_QUIC) {
            SPICE_GNUC_UNUSED uint8_t *start2 = (start + 18);
            size_t data__nw_size, data__extra_size;
            uint32_t data__nelements;
            { /* data */
                uint32_t data_size__value;
                pos = start2 + 0;
                if (SPICE_UNLIKELY(pos + 4 > message_end)) {
                    goto error;
                }
                data_size__value = read_uint32(pos);
                data__nelements = data_size__value;

                data__nw_size = data__nelements;
                data__extra_size = sizeof(SpiceChunks) + sizeof(SpiceChunk);
            }

            u__nw_size = 4 + data__nw_size;
            u__extra_size = data__extra_size;
        } else if (descriptor_type__value == SPICE_IMAGE_TYPE_LZ_RGB || descriptor_type__value == SPICE_IMAGE_TYPE_GLZ_RGB) {
            SPICE_GNUC_UNUSED uint8_t *start2 = (start + 18);
            size_t data__nw_size, data__extra_size;
            uint32_t data__nelements;
            { /* data */
                uint32_t data_size__value;
                pos = start2 + 0;
                if (SPICE_UNLIKELY(pos + 4 > message_end)) {
                    goto error;
                }
                data_size__value = read_uint32(pos);
                data__nelements = data_size__value;

                data__nw_size = data__nelements;
                data__extra_size = sizeof(SpiceChunks) + sizeof(SpiceChunk);
            }

            u__nw_size = 4 + data__nw_size;
            u__extra_size = data__extra_size;
        } else if (descriptor_type__value == SPICE_IMAGE_TYPE_JPEG) {
            SPICE_GNUC_UNUSED uint8_t *start2 = (start + 18);
            size_t data__nw_size, data__extra_size;
            uint32_t data__nelements;
            { /* data */
                uint32_t data_size__value;
                pos = start2 + 0;
                if (SPICE_UNLIKELY(pos + 4 > message_end)) {
                    goto error;
                }
                data_size__value = read_uint32(pos);
                data__nelements = data_size__value;

                data__nw_size = data__nelements;
                data__extra_size = sizeof(SpiceChunks) + sizeof(SpiceChunk);
            }

            u__nw_size = 4 + data__nw_size;
            u__extra_size = data__extra_size;
        } else if (descriptor_type__value == SPICE_IMAGE_TYPE_LZ_PLT) {
            SPICE_GNUC_UNUSED uint8_t *start2 = (start + 18);
            size_t pal__nw_size, pal__extra_size;
            uint8_t flags__value;
            size_t data__nw_size, data__extra_size;
            uint32_t data__nelements;
            { /* pal */
                pos = start2 + 0;
                if (SPICE_UNLIKELY(pos + 1 > message_end)) {
                    goto error;
                }
                flags__value = read_uint8(pos);
                if ((flags__value & SPICE_BITMAP_FLAGS_PAL_FROM_CACHE)) {
                    pal__nw_size = 8;
                    pal__extra_size = 0;
                } else if (1) {
                    uint32_t pal_palette__value;
                    pal__nw_size = 4;
                    pos = (start2 + 5);
                    if (SPICE_UNLIKELY(pos + 4 > message_end)) {
                        goto error;
                    }
                    pal_palette__value = read_uint32(pos);
                    if (SPICE_UNLIKELY(pal_palette__value == 0)) {
                        goto error;
                    }
                    ptr_size = validate_SpicePalette(message_start, message_end, pal_palette__value, minor);
                    if (SPICE_UNLIKELY(ptr_size < 0)) {
                        goto error;
                    }
                    pal__extra_size = ptr_size + /* for alignment */ 3;
                } else {
                    pal__nw_size = 0;
                    pal__extra_size = 0;
                }

            }

            { /* data */
                uint32_t data_size__value;
                pos = start2 + 1;
                if (SPICE_UNLIKELY(pos + 4 > message_end)) {
                    goto error;
                }
                data_size__value = read_uint32(pos);
                data__nelements = data_size__value;

                data__nw_size = data__nelements;
                data__extra_size = sizeof(SpiceChunks) + sizeof(SpiceChunk);
            }

            u__nw_size = 5 + pal__nw_size + data__nw_size;
            u__extra_size = pal__extra_size + data__extra_size;
        } else if (descriptor_type__value == SPICE_IMAGE_TYPE_ZLIB_GLZ_RGB) {
            SPICE_GNUC_UNUSED uint8_t *start2 = (start + 18);
            size_t data__nw_size, data__extra_size;
            uint32_t data__nelements;
            { /* data */
                uint32_t data_size__value;
                pos = start2 + 4;
                if (SPICE_UNLIKELY(pos + 4 > message_end)) {
                    goto error;
                }
                data_size__value = read_uint32(pos);
                data__nelements = data_size__value;

                data__nw_size = data__nelements;
                data__extra_size = sizeof(SpiceChunks) + sizeof(SpiceChunk);
            }

            u__nw_size = 8 + data__nw_size;
            u__extra_size = data__extra_size;
        } else if (descriptor_type__value == SPICE_IMAGE_TYPE_JPEG_ALPHA) {
            SPICE_GNUC_UNUSED uint8_t *start2 = (start + 18);
            size_t data__nw_size, data__extra_size;
            uint32_t data__nelements;
            { /* data */
                uint32_t data_size__value;
                pos = start2 + 5;
                if (SPICE_UNLIKELY(pos + 4 > message_end)) {
                    goto error;
                }
                data_size__value = read_uint32(pos);
                data__nelements = data_size__value;

                data__nw_size = data__nelements;
                data__extra_size = sizeof(SpiceChunks) + sizeof(SpiceChunk);
            }

            u__nw_size = 9 + data__nw_size;
            u__extra_size = data__extra_size;
        } else if (descriptor_type__value == SPICE_IMAGE_TYPE_SURFACE) {
            SPICE_GNUC_UNUSED uint8_t *start2 = (start + 18);
            u__nw_size = 4;
            u__extra_size = 0;
        } else {
            u__nw_size = 0;
            u__extra_size = 0;
        }

    }

    nw_size = 18 + u__nw_size;
    mem_size = sizeof(SpiceImage) + u__extra_size;

    /* Check if struct fits in reported side */
    if (SPICE_UNLIKELY(start + nw_size > message_end)) {
        goto error;
    }
    return mem_size;

   error:
    return -1;
}

static uint8_t * parse_struct_SpicePalette(uint8_t *message_start, uint8_t *message_end, uint8_t *struct_data, PointerInfo *this_ptr_info, int minor)
{
    uint8_t *in = message_start + this_ptr_info->offset;
    uint8_t *end;
    SpicePalette *out;
    uint32_t ents__nelements;
    uint32_t i;

    end = struct_data + sizeof(SpicePalette);
    out = (SpicePalette *)struct_data;

    out->unique = consume_uint64(&in);
    out->num_ents = consume_uint16(&in);
    ents__nelements = out->num_ents;
    for (i = 0; i < ents__nelements; i++) {
        out->ents[i] = consume_uint32(&in);
        end += sizeof(uint32_t);
    }
    return end;
}

static uint8_t * parse_struct_SpiceImage(uint8_t *message_start, uint8_t *message_end, uint8_t *struct_data, PointerInfo *this_ptr_info, int minor)
{
    uint8_t *in = message_start + this_ptr_info->offset;
    uint8_t *end;
    SPICE_GNUC_UNUSED intptr_t ptr_size;
    uint32_t n_ptr=0;
    PointerInfo ptr_info[1];
    SpiceImage *out;
    uint32_t i;

    end = struct_data + sizeof(SpiceImage);
    out = (SpiceImage *)struct_data;

    /* descriptor */ {
        out->descriptor.id = consume_uint64(&in);
        out->descriptor.type = consume_uint8(&in);
        out->descriptor.flags = consume_uint8(&in);
        out->descriptor.width = consume_uint32(&in);
        out->descriptor.height = consume_uint32(&in);
    }
    if (out->descriptor.type == SPICE_IMAGE_TYPE_BITMAP) {
        uint32_t data__nelements;
        SpiceChunks *chunks;
        out->u.bitmap.format = consume_uint8(&in);
        out->u.bitmap.flags = consume_uint8(&in);
        out->u.bitmap.x = consume_uint32(&in);
        out->u.bitmap.y = consume_uint32(&in);
        out->u.bitmap.stride = consume_uint32(&in);
        if ((out->u.bitmap.flags & SPICE_BITMAP_FLAGS_PAL_FROM_CACHE)) {
            out->u.bitmap.palette_id = consume_uint64(&in);
        } else if (1) {
            ptr_info[n_ptr].offset = consume_uint32(&in);
            ptr_info[n_ptr].parse = parse_struct_SpicePalette;
            ptr_info[n_ptr].dest = (void **)&out->u.bitmap.palette;
            n_ptr++;
        }
        data__nelements = out->u.bitmap.stride * out->u.bitmap.y;
        /* use array as chunk */
        chunks = (SpiceChunks *)end;
        end += sizeof(SpiceChunks) + sizeof(SpiceChunk);
        out->u.bitmap.data = chunks;
        chunks->data_size = data__nelements;
        chunks->flags = 0;
        chunks->num_chunks = 1;
        chunks->chunk[0].len = data__nelements;
        chunks->chunk[0].data = in;
        in += data__nelements;
    } else if (out->descriptor.type == SPICE_IMAGE_TYPE_QUIC) {
        uint32_t data__nelements;
        SpiceChunks *chunks;
        out->u.quic.data_size = consume_uint32(&in);
        data__nelements = out->u.quic.data_size;
        /* use array as chunk */
        chunks = (SpiceChunks *)end;
        end += sizeof(SpiceChunks) + sizeof(SpiceChunk);
        out->u.quic.data = chunks;
        chunks->data_size = data__nelements;
        chunks->flags = 0;
        chunks->num_chunks = 1;
        chunks->chunk[0].len = data__nelements;
        chunks->chunk[0].data = in;
        in += data__nelements;
    } else if (out->descriptor.type == SPICE_IMAGE_TYPE_LZ_RGB || out->descriptor.type == SPICE_IMAGE_TYPE_GLZ_RGB) {
        uint32_t data__nelements;
        SpiceChunks *chunks;
        out->u.lz_rgb.data_size = consume_uint32(&in);
        data__nelements = out->u.lz_rgb.data_size;
        /* use array as chunk */
        chunks = (SpiceChunks *)end;
        end += sizeof(SpiceChunks) + sizeof(SpiceChunk);
        out->u.lz_rgb.data = chunks;
        chunks->data_size = data__nelements;
        chunks->flags = 0;
        chunks->num_chunks = 1;
        chunks->chunk[0].len = data__nelements;
        chunks->chunk[0].data = in;
        in += data__nelements;
    } else if (out->descriptor.type == SPICE_IMAGE_TYPE_JPEG) {
        uint32_t data__nelements;
        SpiceChunks *chunks;
        out->u.jpeg.data_size = consume_uint32(&in);
        data__nelements = out->u.jpeg.data_size;
        /* use array as chunk */
        chunks = (SpiceChunks *)end;
        end += sizeof(SpiceChunks) + sizeof(SpiceChunk);
        out->u.jpeg.data = chunks;
        chunks->data_size = data__nelements;
        chunks->flags = 0;
        chunks->num_chunks = 1;
        chunks->chunk[0].len = data__nelements;
        chunks->chunk[0].data = in;
        in += data__nelements;
    } else if (out->descriptor.type == SPICE_IMAGE_TYPE_LZ_PLT) {
        uint32_t data__nelements;
        SpiceChunks *chunks;
        out->u.lz_plt.flags = consume_uint8(&in);
        out->u.lz_plt.data_size = consume_uint32(&in);
        if ((out->u.lz_plt.flags & SPICE_BITMAP_FLAGS_PAL_FROM_CACHE)) {
            out->u.lz_plt.palette_id = consume_uint64(&in);
        } else if (1) {
            ptr_info[n_ptr].offset = consume_uint32(&in);
            ptr_info[n_ptr].parse = parse_struct_SpicePalette;
            ptr_info[n_ptr].dest = (void **)&out->u.lz_plt.palette;
            n_ptr++;
        }
        data__nelements = out->u.lz_plt.data_size;
        /* use array as chunk */
        chunks = (SpiceChunks *)end;
        end += sizeof(SpiceChunks) + sizeof(SpiceChunk);
        out->u.lz_plt.data = chunks;
        chunks->data_size = data__nelements;
        chunks->flags = 0;
        chunks->num_chunks = 1;
        chunks->chunk[0].len = data__nelements;
        chunks->chunk[0].data = in;
        in += data__nelements;
    } else if (out->descriptor.type == SPICE_IMAGE_TYPE_ZLIB_GLZ_RGB) {
        uint32_t data__nelements;
        SpiceChunks *chunks;
        out->u.zlib_glz.glz_data_size = consume_uint32(&in);
        out->u.zlib_glz.data_size = consume_uint32(&in);
        data__nelements = out->u.zlib_glz.data_size;
        /* use array as chunk */
        chunks = (SpiceChunks *)end;
        end += sizeof(SpiceChunks) + sizeof(SpiceChunk);
        out->u.zlib_glz.data = chunks;
        chunks->data_size = data__nelements;
        chunks->flags = 0;
        chunks->num_chunks = 1;
        chunks->chunk[0].len = data__nelements;
        chunks->chunk[0].data = in;
        in += data__nelements;
    } else if (out->descriptor.type == SPICE_IMAGE_TYPE_JPEG_ALPHA) {
        uint32_t data__nelements;
        SpiceChunks *chunks;
        out->u.jpeg_alpha.flags = consume_uint8(&in);
        out->u.jpeg_alpha.jpeg_size = consume_uint32(&in);
        out->u.jpeg_alpha.data_size = consume_uint32(&in);
        data__nelements = out->u.jpeg_alpha.data_size;
        /* use array as chunk */
        chunks = (SpiceChunks *)end;
        end += sizeof(SpiceChunks) + sizeof(SpiceChunk);
        out->u.jpeg_alpha.data = chunks;
        chunks->data_size = data__nelements;
        chunks->flags = 0;
        chunks->num_chunks = 1;
        chunks->chunk[0].len = data__nelements;
        chunks->chunk[0].data = in;
        in += data__nelements;
    } else if (out->descriptor.type == SPICE_IMAGE_TYPE_SURFACE) {
        out->u.surface.surface_id = consume_uint32(&in);
    }

    for (i = 0; i < n_ptr; i++) {
        if (ptr_info[i].offset == 0) {
            *ptr_info[i].dest = NULL;
        } else {
            /* Align to 32 bit */
            end = (uint8_t *)SPICE_ALIGN((size_t)end, 4);
            *ptr_info[i].dest = (void *)end;
            end = ptr_info[i].parse(message_start, message_end, end, &ptr_info[i], minor);
            if (SPICE_UNLIKELY(end == NULL)) {
                goto error;
            }
        }
    }

    return end;

   error:
    return NULL;
}

static uint8_t * parse_msg_display_draw_fill(uint8_t *message_start, uint8_t *message_end, int minor, size_t *size, message_destructor_t *free_message)
{
    SPICE_GNUC_UNUSED uint8_t *pos;
    uint8_t *start = message_start;
    uint8_t *data = NULL;
    size_t mem_size, nw_size;
    uint8_t *in, *end;
    SPICE_GNUC_UNUSED intptr_t ptr_size;
    uint32_t n_ptr=0;
    PointerInfo ptr_info[3];
    size_t base__nw_size, base__extra_size;
    uint32_t rects__saved_size = 0;
    size_t data__nw_size, data__extra_size;
    SpiceMsgDisplayDrawFill *out;
    uint32_t i;

    { /* base */
        SPICE_GNUC_UNUSED uint8_t *start2 = (start + 0);
        size_t clip__nw_size, clip__extra_size;
        { /* clip */
            SPICE_GNUC_UNUSED uint8_t *start3 = (start2 + 20);
            size_t u__nw_size, u__extra_size;
            uint8_t type__value;
            { /* u */
                uint32_t u__mem_size;
                pos = start3 + 0;
                if (SPICE_UNLIKELY(pos + 1 > message_end)) {
                    goto error;
                }
                type__value = read_uint8(pos);
                if (type__value == SPICE_CLIP_TYPE_RECTS) {
                    SPICE_GNUC_UNUSED uint8_t *start4 = (start3 + 1);
                    size_t rects__nw_size, rects__mem_size;
                    uint32_t rects__nelements;
                    { /* rects */
                        uint32_t num_rects__value;
                        pos = start4 + 0;
                        if (SPICE_UNLIKELY(pos + 4 > message_end)) {
                            goto error;
                        }
                        num_rects__value = read_uint32(pos);
                        rects__nelements = num_rects__value;

                        rects__nw_size = (16) * rects__nelements;
                        rects__mem_size = sizeof(SpiceRect) * rects__nelements;
                    }

                    u__nw_size = 4 + rects__nw_size;
                    u__mem_size = sizeof(SpiceClipRects) + rects__mem_size;
                    rects__saved_size = u__nw_size;
                    u__extra_size = u__mem_size;
                } else {
                    u__nw_size = 0;
                    u__extra_size = 0;
                }

            }

            clip__nw_size = 1 + u__nw_size;
            clip__extra_size = u__extra_size;
        }

        base__nw_size = 20 + clip__nw_size;
        base__extra_size = clip__extra_size;
    }

    { /* data */
        SPICE_GNUC_UNUSED uint8_t *start2 = (start + 0 + base__nw_size);
        size_t brush__nw_size, brush__extra_size;
        size_t mask__extra_size;
        { /* brush */
            SPICE_GNUC_UNUSED uint8_t *start3 = (start2 + 0);
            size_t u__nw_size, u__extra_size;
            uint8_t type__value;
            { /* u */
                pos = start3 + 0;
                if (SPICE_UNLIKELY(pos + 1 > message_end)) {
                    goto error;
                }
                type__value = read_uint8(pos);
                if (type__value == SPICE_BRUSH_TYPE_SOLID) {
                    u__nw_size = 4;
                    u__extra_size = 0;
                } else if (type__value == SPICE_BRUSH_TYPE_PATTERN) {
                    SPICE_GNUC_UNUSED uint8_t *start4 = (start3 + 1);
                    size_t pat__extra_size;
                    { /* pat */
                        uint32_t pat__value;
                        pos = (start4 + 0);
                        if (SPICE_UNLIKELY(pos + 4 > message_end)) {
                            goto error;
                        }
                        pat__value = read_uint32(pos);
                        if (SPICE_UNLIKELY(pat__value == 0)) {
                            goto error;
                        }
                        ptr_size = validate_SpiceImage(message_start, message_end, pat__value, minor);
                        if (SPICE_UNLIKELY(ptr_size < 0)) {
                            goto error;
                        }
                        pat__extra_size = ptr_size + /* for alignment */ 3;
                    }

                    u__nw_size = 12;
                    u__extra_size = pat__extra_size;
                } else {
                    u__nw_size = 0;
                    u__extra_size = 0;
                }

            }

            brush__nw_size = 1 + u__nw_size;
            brush__extra_size = u__extra_size;
        }

        { /* mask */
            SPICE_GNUC_UNUSED uint8_t *start3 = (start2 + 2 + brush__nw_size);
            size_t bitmap__extra_size;
            { /* bitmap */
                uint32_t bitmap__value;
                pos = (start3 + 9);
                if (SPICE_UNLIKELY(pos + 4 > message_end)) {
                    goto error;
                }
                bitmap__value = read_uint32(pos);
                ptr_size = validate_SpiceImage(message_start, message_end, bitmap__value, minor);
                if (SPICE_UNLIKELY(ptr_size < 0)) {
                    goto error;
                }
                bitmap__extra_size = ptr_size + /* for alignment */ 3;
            }

            mask__extra_size = bitmap__extra_size;
        }

        data__nw_size = 15 + brush__nw_size;
        data__extra_size = brush__extra_size + mask__extra_size;
    }

    nw_size = 0 + base__nw_size + data__nw_size;
    mem_size = sizeof(SpiceMsgDisplayDrawFill) + base__extra_size + data__extra_size;

    /* Check if message fits in reported side */
    if (start + nw_size > message_end) {
        return NULL;
    }

    /* Validated extents and calculated size */
    data = (uint8_t *)malloc(mem_size);
    if (SPICE_UNLIKELY(data == NULL)) {
        goto error;
    }
    end = data + sizeof(SpiceMsgDisplayDrawFill);
    in = start;

    out = (SpiceMsgDisplayDrawFill *)data;

    /* base */ {
        out->base.surface_id = consume_uint32(&in);
        /* box */ {
            out->base.box.top = consume_int32(&in);
            out->base.box.left = consume_int32(&in);
            out->base.box.bottom = consume_int32(&in);
            out->base.box.right = consume_int32(&in);
        }
        /* clip */ {
            out->base.clip.type = consume_uint8(&in);
            if (out->base.clip.type == SPICE_CLIP_TYPE_RECTS) {
                ptr_info[n_ptr].offset = in - start;
                ptr_info[n_ptr].parse = parse_struct_SpiceClipRects;
                ptr_info[n_ptr].dest = (void **)&out->base.clip.rects;
                n_ptr++;
                in += rects__saved_size;
            }
        }
    }
    /* data */ {
        /* brush */ {
            out->data.brush.type = consume_uint8(&in);
            if (out->data.brush.type == SPICE_BRUSH_TYPE_SOLID) {
                out->data.brush.u.color = consume_uint32(&in);
            } else if (out->data.brush.type == SPICE_BRUSH_TYPE_PATTERN) {
                ptr_info[n_ptr].offset = consume_uint32(&in);
                ptr_info[n_ptr].parse = parse_struct_SpiceImage;
                ptr_info[n_ptr].dest = (void **)&out->data.brush.u.pattern.pat;
                n_ptr++;
                /* pos */ {
                    out->data.brush.u.pattern.pos.x = consume_int32(&in);
                    out->data.brush.u.pattern.pos.y = consume_int32(&in);
                }
            }
        }
        out->data.rop_descriptor = consume_uint16(&in);
        /* mask */ {
            out->data.mask.flags = consume_uint8(&in);
            /* pos */ {
                out->data.mask.pos.x = consume_int32(&in);
                out->data.mask.pos.y = consume_int32(&in);
            }
            ptr_info[n_ptr].offset = consume_uint32(&in);
            ptr_info[n_ptr].parse = parse_struct_SpiceImage;
            ptr_info[n_ptr].dest = (void **)&out->data.mask.bitmap;
            n_ptr++;
        }
    }

    assert(in <= message_end);

    for (i = 0; i < n_ptr; i++) {
        if (ptr_info[i].offset == 0) {
            *ptr_info[i].dest = NULL;
        } else {
            /* Align to 32 bit */
            end = (uint8_t *)SPICE_ALIGN((size_t)end, 4);
            *ptr_info[i].dest = (void *)end;
            end = ptr_info[i].parse(message_start, message_end, end, &ptr_info[i], minor);
            if (SPICE_UNLIKELY(end == NULL)) {
                goto error;
            }
        }
    }

    assert(end <= data + mem_size);

    *size = end - data;
    *free_message = (message_destructor_t) free;
    return data;

   error:
    if (data != NULL) {
        free(data);
    }
    return NULL;
}

static uint8_t * parse_msg_display_draw_opaque(uint8_t *message_start, uint8_t *message_end, int minor, size_t *size, message_destructor_t *free_message)
{
    SPICE_GNUC_UNUSED uint8_t *pos;
    uint8_t *start = message_start;
    uint8_t *data = NULL;
    size_t mem_size, nw_size;
    uint8_t *in, *end;
    SPICE_GNUC_UNUSED intptr_t ptr_size;
    uint32_t n_ptr=0;
    PointerInfo ptr_info[4];
    size_t base__nw_size, base__extra_size;
    uint32_t rects__saved_size = 0;
    size_t data__nw_size, data__extra_size;
    SpiceMsgDisplayDrawOpaque *out;
    uint32_t i;

    { /* base */
        SPICE_GNUC_UNUSED uint8_t *start2 = (start + 0);
        size_t clip__nw_size, clip__extra_size;
        { /* clip */
            SPICE_GNUC_UNUSED uint8_t *start3 = (start2 + 20);
            size_t u__nw_size, u__extra_size;
            uint8_t type__value;
            { /* u */
                uint32_t u__mem_size;
                pos = start3 + 0;
                if (SPICE_UNLIKELY(pos + 1 > message_end)) {
                    goto error;
                }
                type__value = read_uint8(pos);
                if (type__value == SPICE_CLIP_TYPE_RECTS) {
                    SPICE_GNUC_UNUSED uint8_t *start4 = (start3 + 1);
                    size_t rects__nw_size, rects__mem_size;
                    uint32_t rects__nelements;
                    { /* rects */
                        uint32_t num_rects__value;
                        pos = start4 + 0;
                        if (SPICE_UNLIKELY(pos + 4 > message_end)) {
                            goto error;
                        }
                        num_rects__value = read_uint32(pos);
                        rects__nelements = num_rects__value;

                        rects__nw_size = (16) * rects__nelements;
                        rects__mem_size = sizeof(SpiceRect) * rects__nelements;
                    }

                    u__nw_size = 4 + rects__nw_size;
                    u__mem_size = sizeof(SpiceClipRects) + rects__mem_size;
                    rects__saved_size = u__nw_size;
                    u__extra_size = u__mem_size;
                } else {
                    u__nw_size = 0;
                    u__extra_size = 0;
                }

            }

            clip__nw_size = 1 + u__nw_size;
            clip__extra_size = u__extra_size;
        }

        base__nw_size = 20 + clip__nw_size;
        base__extra_size = clip__extra_size;
    }

    { /* data */
        SPICE_GNUC_UNUSED uint8_t *start2 = (start + 0 + base__nw_size);
        size_t src_bitmap__extra_size;
        size_t brush__nw_size, brush__extra_size;
        size_t mask__extra_size;
        { /* src_bitmap */
            uint32_t src_bitmap__value;
            pos = (start2 + 0);
            if (SPICE_UNLIKELY(pos + 4 > message_end)) {
                goto error;
            }
            src_bitmap__value = read_uint32(pos);
            ptr_size = validate_SpiceImage(message_start, message_end, src_bitmap__value, minor);
            if (SPICE_UNLIKELY(ptr_size < 0)) {
                goto error;
            }
            src_bitmap__extra_size = ptr_size + /* for alignment */ 3;
        }

        { /* brush */
            SPICE_GNUC_UNUSED uint8_t *start3 = (start2 + 20);
            size_t u__nw_size, u__extra_size;
            uint8_t type__value;
            { /* u */
                pos = start3 + 0;
                if (SPICE_UNLIKELY(pos + 1 > message_end)) {
                    goto error;
                }
                type__value = read_uint8(pos);
                if (type__value == SPICE_BRUSH_TYPE_SOLID) {
                    u__nw_size = 4;
                    u__extra_size = 0;
                } else if (type__value == SPICE_BRUSH_TYPE_PATTERN) {
                    SPICE_GNUC_UNUSED uint8_t *start4 = (start3 + 1);
                    size_t pat__extra_size;
                    { /* pat */
                        uint32_t pat__value;
                        pos = (start4 + 0);
                        if (SPICE_UNLIKELY(pos + 4 > message_end)) {
                            goto error;
                        }
                        pat__value = read_uint32(pos);
                        if (SPICE_UNLIKELY(pat__value == 0)) {
                            goto error;
                        }
                        ptr_size = validate_SpiceImage(message_start, message_end, pat__value, minor);
                        if (SPICE_UNLIKELY(ptr_size < 0)) {
                            goto error;
                        }
                        pat__extra_size = ptr_size + /* for alignment */ 3;
                    }

                    u__nw_size = 12;
                    u__extra_size = pat__extra_size;
                } else {
                    u__nw_size = 0;
                    u__extra_size = 0;
                }

            }

            brush__nw_size = 1 + u__nw_size;
            brush__extra_size = u__extra_size;
        }

        { /* mask */
            SPICE_GNUC_UNUSED uint8_t *start3 = (start2 + 23 + brush__nw_size);
            size_t bitmap__extra_size;
            { /* bitmap */
                uint32_t bitmap__value;
                pos = (start3 + 9);
                if (SPICE_UNLIKELY(pos + 4 > message_end)) {
                    goto error;
                }
                bitmap__value = read_uint32(pos);
                ptr_size = validate_SpiceImage(message_start, message_end, bitmap__value, minor);
                if (SPICE_UNLIKELY(ptr_size < 0)) {
                    goto error;
                }
                bitmap__extra_size = ptr_size + /* for alignment */ 3;
            }

            mask__extra_size = bitmap__extra_size;
        }

        data__nw_size = 36 + brush__nw_size;
        data__extra_size = src_bitmap__extra_size + brush__extra_size + mask__extra_size;
    }

    nw_size = 0 + base__nw_size + data__nw_size;
    mem_size = sizeof(SpiceMsgDisplayDrawOpaque) + base__extra_size + data__extra_size;

    /* Check if message fits in reported side */
    if (start + nw_size > message_end) {
        return NULL;
    }

    /* Validated extents and calculated size */
    data = (uint8_t *)malloc(mem_size);
    if (SPICE_UNLIKELY(data == NULL)) {
        goto error;
    }
    end = data + sizeof(SpiceMsgDisplayDrawOpaque);
    in = start;

    out = (SpiceMsgDisplayDrawOpaque *)data;

    /* base */ {
        out->base.surface_id = consume_uint32(&in);
        /* box */ {
            out->base.box.top = consume_int32(&in);
            out->base.box.left = consume_int32(&in);
            out->base.box.bottom = consume_int32(&in);
            out->base.box.right = consume_int32(&in);
        }
        /* clip */ {
            out->base.clip.type = consume_uint8(&in);
            if (out->base.clip.type == SPICE_CLIP_TYPE_RECTS) {
                ptr_info[n_ptr].offset = in - start;
                ptr_info[n_ptr].parse = parse_struct_SpiceClipRects;
                ptr_info[n_ptr].dest = (void **)&out->base.clip.rects;
                n_ptr++;
                in += rects__saved_size;
            }
        }
    }
    /* data */ {
        ptr_info[n_ptr].offset = consume_uint32(&in);
        ptr_info[n_ptr].parse = parse_struct_SpiceImage;
        ptr_info[n_ptr].dest = (void **)&out->data.src_bitmap;
        n_ptr++;
        /* src_area */ {
            out->data.src_area.top = consume_int32(&in);
            out->data.src_area.left = consume_int32(&in);
            out->data.src_area.bottom = consume_int32(&in);
            out->data.src_area.right = consume_int32(&in);
        }
        /* brush */ {
            out->data.brush.type = consume_uint8(&in);
            if (out->data.brush.type == SPICE_BRUSH_TYPE_SOLID) {
                out->data.brush.u.color = consume_uint32(&in);
            } else if (out->data.brush.type == SPICE_BRUSH_TYPE_PATTERN) {
                ptr_info[n_ptr].offset = consume_uint32(&in);
                ptr_info[n_ptr].parse = parse_struct_SpiceImage;
                ptr_info[n_ptr].dest = (void **)&out->data.brush.u.pattern.pat;
                n_ptr++;
                /* pos */ {
                    out->data.brush.u.pattern.pos.x = consume_int32(&in);
                    out->data.brush.u.pattern.pos.y = consume_int32(&in);
                }
            }
        }
        out->data.rop_descriptor = consume_uint16(&in);
        out->data.scale_mode = consume_uint8(&in);
        /* mask */ {
            out->data.mask.flags = consume_uint8(&in);
            /* pos */ {
                out->data.mask.pos.x = consume_int32(&in);
                out->data.mask.pos.y = consume_int32(&in);
            }
            ptr_info[n_ptr].offset = consume_uint32(&in);
            ptr_info[n_ptr].parse = parse_struct_SpiceImage;
            ptr_info[n_ptr].dest = (void **)&out->data.mask.bitmap;
            n_ptr++;
        }
    }

    assert(in <= message_end);

    for (i = 0; i < n_ptr; i++) {
        if (ptr_info[i].offset == 0) {
            *ptr_info[i].dest = NULL;
        } else {
            /* Align to 32 bit */
            end = (uint8_t *)SPICE_ALIGN((size_t)end, 4);
            *ptr_info[i].dest = (void *)end;
            end = ptr_info[i].parse(message_start, message_end, end, &ptr_info[i], minor);
            if (SPICE_UNLIKELY(end == NULL)) {
                goto error;
            }
        }
    }

    assert(end <= data + mem_size);

    *size = end - data;
    *free_message = (message_destructor_t) free;
    return data;

   error:
    if (data != NULL) {
        free(data);
    }
    return NULL;
}

static uint8_t * parse_msg_display_draw_copy(uint8_t *message_start, uint8_t *message_end, int minor, size_t *size, message_destructor_t *free_message)
{
    SPICE_GNUC_UNUSED uint8_t *pos;
    uint8_t *start = message_start;
    uint8_t *data = NULL;
    size_t mem_size, nw_size;
    uint8_t *in, *end;
    SPICE_GNUC_UNUSED intptr_t ptr_size;
    uint32_t n_ptr=0;
    PointerInfo ptr_info[3];
    size_t base__nw_size, base__extra_size;
    uint32_t rects__saved_size = 0;
    size_t data__extra_size;
    SpiceMsgDisplayDrawCopy *out;
    uint32_t i;

    { /* base */
        SPICE_GNUC_UNUSED uint8_t *start2 = (start + 0);
        size_t clip__nw_size, clip__extra_size;
        { /* clip */
            SPICE_GNUC_UNUSED uint8_t *start3 = (start2 + 20);
            size_t u__nw_size, u__extra_size;
            uint8_t type__value;
            { /* u */
                uint32_t u__mem_size;
                pos = start3 + 0;
                if (SPICE_UNLIKELY(pos + 1 > message_end)) {
                    goto error;
                }
                type__value = read_uint8(pos);
                if (type__value == SPICE_CLIP_TYPE_RECTS) {
                    SPICE_GNUC_UNUSED uint8_t *start4 = (start3 + 1);
                    size_t rects__nw_size, rects__mem_size;
                    uint32_t rects__nelements;
                    { /* rects */
                        uint32_t num_rects__value;
                        pos = start4 + 0;
                        if (SPICE_UNLIKELY(pos + 4 > message_end)) {
                            goto error;
                        }
                        num_rects__value = read_uint32(pos);
                        rects__nelements = num_rects__value;

                        rects__nw_size = (16) * rects__nelements;
                        rects__mem_size = sizeof(SpiceRect) * rects__nelements;
                    }

                    u__nw_size = 4 + rects__nw_size;
                    u__mem_size = sizeof(SpiceClipRects) + rects__mem_size;
                    rects__saved_size = u__nw_size;
                    u__extra_size = u__mem_size;
                } else {
                    u__nw_size = 0;
                    u__extra_size = 0;
                }

            }

            clip__nw_size = 1 + u__nw_size;
            clip__extra_size = u__extra_size;
        }

        base__nw_size = 20 + clip__nw_size;
        base__extra_size = clip__extra_size;
    }

    { /* data */
        SPICE_GNUC_UNUSED uint8_t *start2 = (start + 0 + base__nw_size);
        size_t src_bitmap__extra_size;
        size_t mask__extra_size;
        { /* src_bitmap */
            uint32_t src_bitmap__value;
            pos = (start2 + 0);
            if (SPICE_UNLIKELY(pos + 4 > message_end)) {
                goto error;
            }
            src_bitmap__value = read_uint32(pos);
            ptr_size = validate_SpiceImage(message_start, message_end, src_bitmap__value, minor);
            if (SPICE_UNLIKELY(ptr_size < 0)) {
                goto error;
            }
            src_bitmap__extra_size = ptr_size + /* for alignment */ 3;
        }

        { /* mask */
            SPICE_GNUC_UNUSED uint8_t *start3 = (start2 + 23);
            size_t bitmap__extra_size;
            { /* bitmap */
                uint32_t bitmap__value;
                pos = (start3 + 9);
                if (SPICE_UNLIKELY(pos + 4 > message_end)) {
                    goto error;
                }
                bitmap__value = read_uint32(pos);
                ptr_size = validate_SpiceImage(message_start, message_end, bitmap__value, minor);
                if (SPICE_UNLIKELY(ptr_size < 0)) {
                    goto error;
                }
                bitmap__extra_size = ptr_size + /* for alignment */ 3;
            }

            mask__extra_size = bitmap__extra_size;
        }

        data__extra_size = src_bitmap__extra_size + mask__extra_size;
    }

    nw_size = 36 + base__nw_size;
    mem_size = sizeof(SpiceMsgDisplayDrawCopy) + base__extra_size + data__extra_size;

    /* Check if message fits in reported side */
    if (start + nw_size > message_end) {
        return NULL;
    }

    /* Validated extents and calculated size */
    data = (uint8_t *)malloc(mem_size);
    if (SPICE_UNLIKELY(data == NULL)) {
        goto error;
    }
    end = data + sizeof(SpiceMsgDisplayDrawCopy);
    in = start;

    out = (SpiceMsgDisplayDrawCopy *)data;

    /* base */ {
        out->base.surface_id = consume_uint32(&in);
        /* box */ {
            out->base.box.top = consume_int32(&in);
            out->base.box.left = consume_int32(&in);
            out->base.box.bottom = consume_int32(&in);
            out->base.box.right = consume_int32(&in);
        }
        /* clip */ {
            out->base.clip.type = consume_uint8(&in);
            if (out->base.clip.type == SPICE_CLIP_TYPE_RECTS) {
                ptr_info[n_ptr].offset = in - start;
                ptr_info[n_ptr].parse = parse_struct_SpiceClipRects;
                ptr_info[n_ptr].dest = (void **)&out->base.clip.rects;
                n_ptr++;
                in += rects__saved_size;
            }
        }
    }
    /* data */ {
        ptr_info[n_ptr].offset = consume_uint32(&in);
        ptr_info[n_ptr].parse = parse_struct_SpiceImage;
        ptr_info[n_ptr].dest = (void **)&out->data.src_bitmap;
        n_ptr++;
        /* src_area */ {
            out->data.src_area.top = consume_int32(&in);
            out->data.src_area.left = consume_int32(&in);
            out->data.src_area.bottom = consume_int32(&in);
            out->data.src_area.right = consume_int32(&in);
        }
        out->data.rop_descriptor = consume_uint16(&in);
        out->data.scale_mode = consume_uint8(&in);
        /* mask */ {
            out->data.mask.flags = consume_uint8(&in);
            /* pos */ {
                out->data.mask.pos.x = consume_int32(&in);
                out->data.mask.pos.y = consume_int32(&in);
            }
            ptr_info[n_ptr].offset = consume_uint32(&in);
            ptr_info[n_ptr].parse = parse_struct_SpiceImage;
            ptr_info[n_ptr].dest = (void **)&out->data.mask.bitmap;
            n_ptr++;
        }
    }

    assert(in <= message_end);

    for (i = 0; i < n_ptr; i++) {
        if (ptr_info[i].offset == 0) {
            *ptr_info[i].dest = NULL;
        } else {
            /* Align to 32 bit */
            end = (uint8_t *)SPICE_ALIGN((size_t)end, 4);
            *ptr_info[i].dest = (void *)end;
            end = ptr_info[i].parse(message_start, message_end, end, &ptr_info[i], minor);
            if (SPICE_UNLIKELY(end == NULL)) {
                goto error;
            }
        }
    }

    assert(end <= data + mem_size);

    *size = end - data;
    *free_message = (message_destructor_t) free;
    return data;

   error:
    if (data != NULL) {
        free(data);
    }
    return NULL;
}

static uint8_t * parse_msg_display_draw_blend(uint8_t *message_start, uint8_t *message_end, int minor, size_t *size, message_destructor_t *free_message)
{
    SPICE_GNUC_UNUSED uint8_t *pos;
    uint8_t *start = message_start;
    uint8_t *data = NULL;
    size_t mem_size, nw_size;
    uint8_t *in, *end;
    SPICE_GNUC_UNUSED intptr_t ptr_size;
    uint32_t n_ptr=0;
    PointerInfo ptr_info[3];
    size_t base__nw_size, base__extra_size;
    uint32_t rects__saved_size = 0;
    size_t data__extra_size;
    SpiceMsgDisplayDrawBlend *out;
    uint32_t i;

    { /* base */
        SPICE_GNUC_UNUSED uint8_t *start2 = (start + 0);
        size_t clip__nw_size, clip__extra_size;
        { /* clip */
            SPICE_GNUC_UNUSED uint8_t *start3 = (start2 + 20);
            size_t u__nw_size, u__extra_size;
            uint8_t type__value;
            { /* u */
                uint32_t u__mem_size;
                pos = start3 + 0;
                if (SPICE_UNLIKELY(pos + 1 > message_end)) {
                    goto error;
                }
                type__value = read_uint8(pos);
                if (type__value == SPICE_CLIP_TYPE_RECTS) {
                    SPICE_GNUC_UNUSED uint8_t *start4 = (start3 + 1);
                    size_t rects__nw_size, rects__mem_size;
                    uint32_t rects__nelements;
                    { /* rects */
                        uint32_t num_rects__value;
                        pos = start4 + 0;
                        if (SPICE_UNLIKELY(pos + 4 > message_end)) {
                            goto error;
                        }
                        num_rects__value = read_uint32(pos);
                        rects__nelements = num_rects__value;

                        rects__nw_size = (16) * rects__nelements;
                        rects__mem_size = sizeof(SpiceRect) * rects__nelements;
                    }

                    u__nw_size = 4 + rects__nw_size;
                    u__mem_size = sizeof(SpiceClipRects) + rects__mem_size;
                    rects__saved_size = u__nw_size;
                    u__extra_size = u__mem_size;
                } else {
                    u__nw_size = 0;
                    u__extra_size = 0;
                }

            }

            clip__nw_size = 1 + u__nw_size;
            clip__extra_size = u__extra_size;
        }

        base__nw_size = 20 + clip__nw_size;
        base__extra_size = clip__extra_size;
    }

    { /* data */
        SPICE_GNUC_UNUSED uint8_t *start2 = (start + 0 + base__nw_size);
        size_t src_bitmap__extra_size;
        size_t mask__extra_size;
        { /* src_bitmap */
            uint32_t src_bitmap__value;
            pos = (start2 + 0);
            if (SPICE_UNLIKELY(pos + 4 > message_end)) {
                goto error;
            }
            src_bitmap__value = read_uint32(pos);
            ptr_size = validate_SpiceImage(message_start, message_end, src_bitmap__value, minor);
            if (SPICE_UNLIKELY(ptr_size < 0)) {
                goto error;
            }
            src_bitmap__extra_size = ptr_size + /* for alignment */ 3;
        }

        { /* mask */
            SPICE_GNUC_UNUSED uint8_t *start3 = (start2 + 23);
            size_t bitmap__extra_size;
            { /* bitmap */
                uint32_t bitmap__value;
                pos = (start3 + 9);
                if (SPICE_UNLIKELY(pos + 4 > message_end)) {
                    goto error;
                }
                bitmap__value = read_uint32(pos);
                ptr_size = validate_SpiceImage(message_start, message_end, bitmap__value, minor);
                if (SPICE_UNLIKELY(ptr_size < 0)) {
                    goto error;
                }
                bitmap__extra_size = ptr_size + /* for alignment */ 3;
            }

            mask__extra_size = bitmap__extra_size;
        }

        data__extra_size = src_bitmap__extra_size + mask__extra_size;
    }

    nw_size = 36 + base__nw_size;
    mem_size = sizeof(SpiceMsgDisplayDrawBlend) + base__extra_size + data__extra_size;

    /* Check if message fits in reported side */
    if (start + nw_size > message_end) {
        return NULL;
    }

    /* Validated extents and calculated size */
    data = (uint8_t *)malloc(mem_size);
    if (SPICE_UNLIKELY(data == NULL)) {
        goto error;
    }
    end = data + sizeof(SpiceMsgDisplayDrawBlend);
    in = start;

    out = (SpiceMsgDisplayDrawBlend *)data;

    /* base */ {
        out->base.surface_id = consume_uint32(&in);
        /* box */ {
            out->base.box.top = consume_int32(&in);
            out->base.box.left = consume_int32(&in);
            out->base.box.bottom = consume_int32(&in);
            out->base.box.right = consume_int32(&in);
        }
        /* clip */ {
            out->base.clip.type = consume_uint8(&in);
            if (out->base.clip.type == SPICE_CLIP_TYPE_RECTS) {
                ptr_info[n_ptr].offset = in - start;
                ptr_info[n_ptr].parse = parse_struct_SpiceClipRects;
                ptr_info[n_ptr].dest = (void **)&out->base.clip.rects;
                n_ptr++;
                in += rects__saved_size;
            }
        }
    }
    /* data */ {
        ptr_info[n_ptr].offset = consume_uint32(&in);
        ptr_info[n_ptr].parse = parse_struct_SpiceImage;
        ptr_info[n_ptr].dest = (void **)&out->data.src_bitmap;
        n_ptr++;
        /* src_area */ {
            out->data.src_area.top = consume_int32(&in);
            out->data.src_area.left = consume_int32(&in);
            out->data.src_area.bottom = consume_int32(&in);
            out->data.src_area.right = consume_int32(&in);
        }
        out->data.rop_descriptor = consume_uint16(&in);
        out->data.scale_mode = consume_uint8(&in);
        /* mask */ {
            out->data.mask.flags = consume_uint8(&in);
            /* pos */ {
                out->data.mask.pos.x = consume_int32(&in);
                out->data.mask.pos.y = consume_int32(&in);
            }
            ptr_info[n_ptr].offset = consume_uint32(&in);
            ptr_info[n_ptr].parse = parse_struct_SpiceImage;
            ptr_info[n_ptr].dest = (void **)&out->data.mask.bitmap;
            n_ptr++;
        }
    }

    assert(in <= message_end);

    for (i = 0; i < n_ptr; i++) {
        if (ptr_info[i].offset == 0) {
            *ptr_info[i].dest = NULL;
        } else {
            /* Align to 32 bit */
            end = (uint8_t *)SPICE_ALIGN((size_t)end, 4);
            *ptr_info[i].dest = (void *)end;
            end = ptr_info[i].parse(message_start, message_end, end, &ptr_info[i], minor);
            if (SPICE_UNLIKELY(end == NULL)) {
                goto error;
            }
        }
    }

    assert(end <= data + mem_size);

    *size = end - data;
    *free_message = (message_destructor_t) free;
    return data;

   error:
    if (data != NULL) {
        free(data);
    }
    return NULL;
}

static uint8_t * parse_msg_display_draw_blackness(uint8_t *message_start, uint8_t *message_end, int minor, size_t *size, message_destructor_t *free_message)
{
    SPICE_GNUC_UNUSED uint8_t *pos;
    uint8_t *start = message_start;
    uint8_t *data = NULL;
    size_t mem_size, nw_size;
    uint8_t *in, *end;
    SPICE_GNUC_UNUSED intptr_t ptr_size;
    uint32_t n_ptr=0;
    PointerInfo ptr_info[2];
    size_t base__nw_size, base__extra_size;
    uint32_t rects__saved_size = 0;
    size_t data__extra_size;
    SpiceMsgDisplayDrawBlackness *out;
    uint32_t i;

    { /* base */
        SPICE_GNUC_UNUSED uint8_t *start2 = (start + 0);
        size_t clip__nw_size, clip__extra_size;
        { /* clip */
            SPICE_GNUC_UNUSED uint8_t *start3 = (start2 + 20);
            size_t u__nw_size, u__extra_size;
            uint8_t type__value;
            { /* u */
                uint32_t u__mem_size;
                pos = start3 + 0;
                if (SPICE_UNLIKELY(pos + 1 > message_end)) {
                    goto error;
                }
                type__value = read_uint8(pos);
                if (type__value == SPICE_CLIP_TYPE_RECTS) {
                    SPICE_GNUC_UNUSED uint8_t *start4 = (start3 + 1);
                    size_t rects__nw_size, rects__mem_size;
                    uint32_t rects__nelements;
                    { /* rects */
                        uint32_t num_rects__value;
                        pos = start4 + 0;
                        if (SPICE_UNLIKELY(pos + 4 > message_end)) {
                            goto error;
                        }
                        num_rects__value = read_uint32(pos);
                        rects__nelements = num_rects__value;

                        rects__nw_size = (16) * rects__nelements;
                        rects__mem_size = sizeof(SpiceRect) * rects__nelements;
                    }

                    u__nw_size = 4 + rects__nw_size;
                    u__mem_size = sizeof(SpiceClipRects) + rects__mem_size;
                    rects__saved_size = u__nw_size;
                    u__extra_size = u__mem_size;
                } else {
                    u__nw_size = 0;
                    u__extra_size = 0;
                }

            }

            clip__nw_size = 1 + u__nw_size;
            clip__extra_size = u__extra_size;
        }

        base__nw_size = 20 + clip__nw_size;
        base__extra_size = clip__extra_size;
    }

    { /* data */
        SPICE_GNUC_UNUSED uint8_t *start2 = (start + 0 + base__nw_size);
        size_t mask__extra_size;
        { /* mask */
            SPICE_GNUC_UNUSED uint8_t *start3 = (start2 + 0);
            size_t bitmap__extra_size;
            { /* bitmap */
                uint32_t bitmap__value;
                pos = (start3 + 9);
                if (SPICE_UNLIKELY(pos + 4 > message_end)) {
                    goto error;
                }
                bitmap__value = read_uint32(pos);
                ptr_size = validate_SpiceImage(message_start, message_end, bitmap__value, minor);
                if (SPICE_UNLIKELY(ptr_size < 0)) {
                    goto error;
                }
                bitmap__extra_size = ptr_size + /* for alignment */ 3;
            }

            mask__extra_size = bitmap__extra_size;
        }

        data__extra_size = mask__extra_size;
    }

    nw_size = 13 + base__nw_size;
    mem_size = sizeof(SpiceMsgDisplayDrawBlackness) + base__extra_size + data__extra_size;

    /* Check if message fits in reported side */
    if (start + nw_size > message_end) {
        return NULL;
    }

    /* Validated extents and calculated size */
    data = (uint8_t *)malloc(mem_size);
    if (SPICE_UNLIKELY(data == NULL)) {
        goto error;
    }
    end = data + sizeof(SpiceMsgDisplayDrawBlackness);
    in = start;

    out = (SpiceMsgDisplayDrawBlackness *)data;

    /* base */ {
        out->base.surface_id = consume_uint32(&in);
        /* box */ {
            out->base.box.top = consume_int32(&in);
            out->base.box.left = consume_int32(&in);
            out->base.box.bottom = consume_int32(&in);
            out->base.box.right = consume_int32(&in);
        }
        /* clip */ {
            out->base.clip.type = consume_uint8(&in);
            if (out->base.clip.type == SPICE_CLIP_TYPE_RECTS) {
                ptr_info[n_ptr].offset = in - start;
                ptr_info[n_ptr].parse = parse_struct_SpiceClipRects;
                ptr_info[n_ptr].dest = (void **)&out->base.clip.rects;
                n_ptr++;
                in += rects__saved_size;
            }
        }
    }
    /* data */ {
        /* mask */ {
            out->data.mask.flags = consume_uint8(&in);
            /* pos */ {
                out->data.mask.pos.x = consume_int32(&in);
                out->data.mask.pos.y = consume_int32(&in);
            }
            ptr_info[n_ptr].offset = consume_uint32(&in);
            ptr_info[n_ptr].parse = parse_struct_SpiceImage;
            ptr_info[n_ptr].dest = (void **)&out->data.mask.bitmap;
            n_ptr++;
        }
    }

    assert(in <= message_end);

    for (i = 0; i < n_ptr; i++) {
        if (ptr_info[i].offset == 0) {
            *ptr_info[i].dest = NULL;
        } else {
            /* Align to 32 bit */
            end = (uint8_t *)SPICE_ALIGN((size_t)end, 4);
            *ptr_info[i].dest = (void *)end;
            end = ptr_info[i].parse(message_start, message_end, end, &ptr_info[i], minor);
            if (SPICE_UNLIKELY(end == NULL)) {
                goto error;
            }
        }
    }

    assert(end <= data + mem_size);

    *size = end - data;
    *free_message = (message_destructor_t) free;
    return data;

   error:
    if (data != NULL) {
        free(data);
    }
    return NULL;
}

static uint8_t * parse_msg_display_draw_whiteness(uint8_t *message_start, uint8_t *message_end, int minor, size_t *size, message_destructor_t *free_message)
{
    SPICE_GNUC_UNUSED uint8_t *pos;
    uint8_t *start = message_start;
    uint8_t *data = NULL;
    size_t mem_size, nw_size;
    uint8_t *in, *end;
    SPICE_GNUC_UNUSED intptr_t ptr_size;
    uint32_t n_ptr=0;
    PointerInfo ptr_info[2];
    size_t base__nw_size, base__extra_size;
    uint32_t rects__saved_size = 0;
    size_t data__extra_size;
    SpiceMsgDisplayDrawWhiteness *out;
    uint32_t i;

    { /* base */
        SPICE_GNUC_UNUSED uint8_t *start2 = (start + 0);
        size_t clip__nw_size, clip__extra_size;
        { /* clip */
            SPICE_GNUC_UNUSED uint8_t *start3 = (start2 + 20);
            size_t u__nw_size, u__extra_size;
            uint8_t type__value;
            { /* u */
                uint32_t u__mem_size;
                pos = start3 + 0;
                if (SPICE_UNLIKELY(pos + 1 > message_end)) {
                    goto error;
                }
                type__value = read_uint8(pos);
                if (type__value == SPICE_CLIP_TYPE_RECTS) {
                    SPICE_GNUC_UNUSED uint8_t *start4 = (start3 + 1);
                    size_t rects__nw_size, rects__mem_size;
                    uint32_t rects__nelements;
                    { /* rects */
                        uint32_t num_rects__value;
                        pos = start4 + 0;
                        if (SPICE_UNLIKELY(pos + 4 > message_end)) {
                            goto error;
                        }
                        num_rects__value = read_uint32(pos);
                        rects__nelements = num_rects__value;

                        rects__nw_size = (16) * rects__nelements;
                        rects__mem_size = sizeof(SpiceRect) * rects__nelements;
                    }

                    u__nw_size = 4 + rects__nw_size;
                    u__mem_size = sizeof(SpiceClipRects) + rects__mem_size;
                    rects__saved_size = u__nw_size;
                    u__extra_size = u__mem_size;
                } else {
                    u__nw_size = 0;
                    u__extra_size = 0;
                }

            }

            clip__nw_size = 1 + u__nw_size;
            clip__extra_size = u__extra_size;
        }

        base__nw_size = 20 + clip__nw_size;
        base__extra_size = clip__extra_size;
    }

    { /* data */
        SPICE_GNUC_UNUSED uint8_t *start2 = (start + 0 + base__nw_size);
        size_t mask__extra_size;
        { /* mask */
            SPICE_GNUC_UNUSED uint8_t *start3 = (start2 + 0);
            size_t bitmap__extra_size;
            { /* bitmap */
                uint32_t bitmap__value;
                pos = (start3 + 9);
                if (SPICE_UNLIKELY(pos + 4 > message_end)) {
                    goto error;
                }
                bitmap__value = read_uint32(pos);
                ptr_size = validate_SpiceImage(message_start, message_end, bitmap__value, minor);
                if (SPICE_UNLIKELY(ptr_size < 0)) {
                    goto error;
                }
                bitmap__extra_size = ptr_size + /* for alignment */ 3;
            }

            mask__extra_size = bitmap__extra_size;
        }

        data__extra_size = mask__extra_size;
    }

    nw_size = 13 + base__nw_size;
    mem_size = sizeof(SpiceMsgDisplayDrawWhiteness) + base__extra_size + data__extra_size;

    /* Check if message fits in reported side */
    if (start + nw_size > message_end) {
        return NULL;
    }

    /* Validated extents and calculated size */
    data = (uint8_t *)malloc(mem_size);
    if (SPICE_UNLIKELY(data == NULL)) {
        goto error;
    }
    end = data + sizeof(SpiceMsgDisplayDrawWhiteness);
    in = start;

    out = (SpiceMsgDisplayDrawWhiteness *)data;

    /* base */ {
        out->base.surface_id = consume_uint32(&in);
        /* box */ {
            out->base.box.top = consume_int32(&in);
            out->base.box.left = consume_int32(&in);
            out->base.box.bottom = consume_int32(&in);
            out->base.box.right = consume_int32(&in);
        }
        /* clip */ {
            out->base.clip.type = consume_uint8(&in);
            if (out->base.clip.type == SPICE_CLIP_TYPE_RECTS) {
                ptr_info[n_ptr].offset = in - start;
                ptr_info[n_ptr].parse = parse_struct_SpiceClipRects;
                ptr_info[n_ptr].dest = (void **)&out->base.clip.rects;
                n_ptr++;
                in += rects__saved_size;
            }
        }
    }
    /* data */ {
        /* mask */ {
            out->data.mask.flags = consume_uint8(&in);
            /* pos */ {
                out->data.mask.pos.x = consume_int32(&in);
                out->data.mask.pos.y = consume_int32(&in);
            }
            ptr_info[n_ptr].offset = consume_uint32(&in);
            ptr_info[n_ptr].parse = parse_struct_SpiceImage;
            ptr_info[n_ptr].dest = (void **)&out->data.mask.bitmap;
            n_ptr++;
        }
    }

    assert(in <= message_end);

    for (i = 0; i < n_ptr; i++) {
        if (ptr_info[i].offset == 0) {
            *ptr_info[i].dest = NULL;
        } else {
            /* Align to 32 bit */
            end = (uint8_t *)SPICE_ALIGN((size_t)end, 4);
            *ptr_info[i].dest = (void *)end;
            end = ptr_info[i].parse(message_start, message_end, end, &ptr_info[i], minor);
            if (SPICE_UNLIKELY(end == NULL)) {
                goto error;
            }
        }
    }

    assert(end <= data + mem_size);

    *size = end - data;
    *free_message = (message_destructor_t) free;
    return data;

   error:
    if (data != NULL) {
        free(data);
    }
    return NULL;
}

static uint8_t * parse_msg_display_draw_invers(uint8_t *message_start, uint8_t *message_end, int minor, size_t *size, message_destructor_t *free_message)
{
    SPICE_GNUC_UNUSED uint8_t *pos;
    uint8_t *start = message_start;
    uint8_t *data = NULL;
    size_t mem_size, nw_size;
    uint8_t *in, *end;
    SPICE_GNUC_UNUSED intptr_t ptr_size;
    uint32_t n_ptr=0;
    PointerInfo ptr_info[2];
    size_t base__nw_size, base__extra_size;
    uint32_t rects__saved_size = 0;
    size_t data__extra_size;
    SpiceMsgDisplayDrawInvers *out;
    uint32_t i;

    { /* base */
        SPICE_GNUC_UNUSED uint8_t *start2 = (start + 0);
        size_t clip__nw_size, clip__extra_size;
        { /* clip */
            SPICE_GNUC_UNUSED uint8_t *start3 = (start2 + 20);
            size_t u__nw_size, u__extra_size;
            uint8_t type__value;
            { /* u */
                uint32_t u__mem_size;
                pos = start3 + 0;
                if (SPICE_UNLIKELY(pos + 1 > message_end)) {
                    goto error;
                }
                type__value = read_uint8(pos);
                if (type__value == SPICE_CLIP_TYPE_RECTS) {
                    SPICE_GNUC_UNUSED uint8_t *start4 = (start3 + 1);
                    size_t rects__nw_size, rects__mem_size;
                    uint32_t rects__nelements;
                    { /* rects */
                        uint32_t num_rects__value;
                        pos = start4 + 0;
                        if (SPICE_UNLIKELY(pos + 4 > message_end)) {
                            goto error;
                        }
                        num_rects__value = read_uint32(pos);
                        rects__nelements = num_rects__value;

                        rects__nw_size = (16) * rects__nelements;
                        rects__mem_size = sizeof(SpiceRect) * rects__nelements;
                    }

                    u__nw_size = 4 + rects__nw_size;
                    u__mem_size = sizeof(SpiceClipRects) + rects__mem_size;
                    rects__saved_size = u__nw_size;
                    u__extra_size = u__mem_size;
                } else {
                    u__nw_size = 0;
                    u__extra_size = 0;
                }

            }

            clip__nw_size = 1 + u__nw_size;
            clip__extra_size = u__extra_size;
        }

        base__nw_size = 20 + clip__nw_size;
        base__extra_size = clip__extra_size;
    }

    { /* data */
        SPICE_GNUC_UNUSED uint8_t *start2 = (start + 0 + base__nw_size);
        size_t mask__extra_size;
        { /* mask */
            SPICE_GNUC_UNUSED uint8_t *start3 = (start2 + 0);
            size_t bitmap__extra_size;
            { /* bitmap */
                uint32_t bitmap__value;
                pos = (start3 + 9);
                if (SPICE_UNLIKELY(pos + 4 > message_end)) {
                    goto error;
                }
                bitmap__value = read_uint32(pos);
                ptr_size = validate_SpiceImage(message_start, message_end, bitmap__value, minor);
                if (SPICE_UNLIKELY(ptr_size < 0)) {
                    goto error;
                }
                bitmap__extra_size = ptr_size + /* for alignment */ 3;
            }

            mask__extra_size = bitmap__extra_size;
        }

        data__extra_size = mask__extra_size;
    }

    nw_size = 13 + base__nw_size;
    mem_size = sizeof(SpiceMsgDisplayDrawInvers) + base__extra_size + data__extra_size;

    /* Check if message fits in reported side */
    if (start + nw_size > message_end) {
        return NULL;
    }

    /* Validated extents and calculated size */
    data = (uint8_t *)malloc(mem_size);
    if (SPICE_UNLIKELY(data == NULL)) {
        goto error;
    }
    end = data + sizeof(SpiceMsgDisplayDrawInvers);
    in = start;

    out = (SpiceMsgDisplayDrawInvers *)data;

    /* base */ {
        out->base.surface_id = consume_uint32(&in);
        /* box */ {
            out->base.box.top = consume_int32(&in);
            out->base.box.left = consume_int32(&in);
            out->base.box.bottom = consume_int32(&in);
            out->base.box.right = consume_int32(&in);
        }
        /* clip */ {
            out->base.clip.type = consume_uint8(&in);
            if (out->base.clip.type == SPICE_CLIP_TYPE_RECTS) {
                ptr_info[n_ptr].offset = in - start;
                ptr_info[n_ptr].parse = parse_struct_SpiceClipRects;
                ptr_info[n_ptr].dest = (void **)&out->base.clip.rects;
                n_ptr++;
                in += rects__saved_size;
            }
        }
    }
    /* data */ {
        /* mask */ {
            out->data.mask.flags = consume_uint8(&in);
            /* pos */ {
                out->data.mask.pos.x = consume_int32(&in);
                out->data.mask.pos.y = consume_int32(&in);
            }
            ptr_info[n_ptr].offset = consume_uint32(&in);
            ptr_info[n_ptr].parse = parse_struct_SpiceImage;
            ptr_info[n_ptr].dest = (void **)&out->data.mask.bitmap;
            n_ptr++;
        }
    }

    assert(in <= message_end);

    for (i = 0; i < n_ptr; i++) {
        if (ptr_info[i].offset == 0) {
            *ptr_info[i].dest = NULL;
        } else {
            /* Align to 32 bit */
            end = (uint8_t *)SPICE_ALIGN((size_t)end, 4);
            *ptr_info[i].dest = (void *)end;
            end = ptr_info[i].parse(message_start, message_end, end, &ptr_info[i], minor);
            if (SPICE_UNLIKELY(end == NULL)) {
                goto error;
            }
        }
    }

    assert(end <= data + mem_size);

    *size = end - data;
    *free_message = (message_destructor_t) free;
    return data;

   error:
    if (data != NULL) {
        free(data);
    }
    return NULL;
}

static uint8_t * parse_msg_display_draw_rop3(uint8_t *message_start, uint8_t *message_end, int minor, size_t *size, message_destructor_t *free_message)
{
    SPICE_GNUC_UNUSED uint8_t *pos;
    uint8_t *start = message_start;
    uint8_t *data = NULL;
    size_t mem_size, nw_size;
    uint8_t *in, *end;
    SPICE_GNUC_UNUSED intptr_t ptr_size;
    uint32_t n_ptr=0;
    PointerInfo ptr_info[4];
    size_t base__nw_size, base__extra_size;
    uint32_t rects__saved_size = 0;
    size_t data__nw_size, data__extra_size;
    SpiceMsgDisplayDrawRop3 *out;
    uint32_t i;

    { /* base */
        SPICE_GNUC_UNUSED uint8_t *start2 = (start + 0);
        size_t clip__nw_size, clip__extra_size;
        { /* clip */
            SPICE_GNUC_UNUSED uint8_t *start3 = (start2 + 20);
            size_t u__nw_size, u__extra_size;
            uint8_t type__value;
            { /* u */
                uint32_t u__mem_size;
                pos = start3 + 0;
                if (SPICE_UNLIKELY(pos + 1 > message_end)) {
                    goto error;
                }
                type__value = read_uint8(pos);
                if (type__value == SPICE_CLIP_TYPE_RECTS) {
                    SPICE_GNUC_UNUSED uint8_t *start4 = (start3 + 1);
                    size_t rects__nw_size, rects__mem_size;
                    uint32_t rects__nelements;
                    { /* rects */
                        uint32_t num_rects__value;
                        pos = start4 + 0;
                        if (SPICE_UNLIKELY(pos + 4 > message_end)) {
                            goto error;
                        }
                        num_rects__value = read_uint32(pos);
                        rects__nelements = num_rects__value;

                        rects__nw_size = (16) * rects__nelements;
                        rects__mem_size = sizeof(SpiceRect) * rects__nelements;
                    }

                    u__nw_size = 4 + rects__nw_size;
                    u__mem_size = sizeof(SpiceClipRects) + rects__mem_size;
                    rects__saved_size = u__nw_size;
                    u__extra_size = u__mem_size;
                } else {
                    u__nw_size = 0;
                    u__extra_size = 0;
                }

            }

            clip__nw_size = 1 + u__nw_size;
            clip__extra_size = u__extra_size;
        }

        base__nw_size = 20 + clip__nw_size;
        base__extra_size = clip__extra_size;
    }

    { /* data */
        SPICE_GNUC_UNUSED uint8_t *start2 = (start + 0 + base__nw_size);
        size_t src_bitmap__extra_size;
        size_t brush__nw_size, brush__extra_size;
        size_t mask__extra_size;
        { /* src_bitmap */
            uint32_t src_bitmap__value;
            pos = (start2 + 0);
            if (SPICE_UNLIKELY(pos + 4 > message_end)) {
                goto error;
            }
            src_bitmap__value = read_uint32(pos);
            ptr_size = validate_SpiceImage(message_start, message_end, src_bitmap__value, minor);
            if (SPICE_UNLIKELY(ptr_size < 0)) {
                goto error;
            }
            src_bitmap__extra_size = ptr_size + /* for alignment */ 3;
        }

        { /* brush */
            SPICE_GNUC_UNUSED uint8_t *start3 = (start2 + 20);
            size_t u__nw_size, u__extra_size;
            uint8_t type__value;
            { /* u */
                pos = start3 + 0;
                if (SPICE_UNLIKELY(pos + 1 > message_end)) {
                    goto error;
                }
                type__value = read_uint8(pos);
                if (type__value == SPICE_BRUSH_TYPE_SOLID) {
                    u__nw_size = 4;
                    u__extra_size = 0;
                } else if (type__value == SPICE_BRUSH_TYPE_PATTERN) {
                    SPICE_GNUC_UNUSED uint8_t *start4 = (start3 + 1);
                    size_t pat__extra_size;
                    { /* pat */
                        uint32_t pat__value;
                        pos = (start4 + 0);
                        if (SPICE_UNLIKELY(pos + 4 > message_end)) {
                            goto error;
                        }
                        pat__value = read_uint32(pos);
                        if (SPICE_UNLIKELY(pat__value == 0)) {
                            goto error;
                        }
                        ptr_size = validate_SpiceImage(message_start, message_end, pat__value, minor);
                        if (SPICE_UNLIKELY(ptr_size < 0)) {
                            goto error;
                        }
                        pat__extra_size = ptr_size + /* for alignment */ 3;
                    }

                    u__nw_size = 12;
                    u__extra_size = pat__extra_size;
                } else {
                    u__nw_size = 0;
                    u__extra_size = 0;
                }

            }

            brush__nw_size = 1 + u__nw_size;
            brush__extra_size = u__extra_size;
        }

        { /* mask */
            SPICE_GNUC_UNUSED uint8_t *start3 = (start2 + 22 + brush__nw_size);
            size_t bitmap__extra_size;
            { /* bitmap */
                uint32_t bitmap__value;
                pos = (start3 + 9);
                if (SPICE_UNLIKELY(pos + 4 > message_end)) {
                    goto error;
                }
                bitmap__value = read_uint32(pos);
                ptr_size = validate_SpiceImage(message_start, message_end, bitmap__value, minor);
                if (SPICE_UNLIKELY(ptr_size < 0)) {
                    goto error;
                }
                bitmap__extra_size = ptr_size + /* for alignment */ 3;
            }

            mask__extra_size = bitmap__extra_size;
        }

        data__nw_size = 35 + brush__nw_size;
        data__extra_size = src_bitmap__extra_size + brush__extra_size + mask__extra_size;
    }

    nw_size = 0 + base__nw_size + data__nw_size;
    mem_size = sizeof(SpiceMsgDisplayDrawRop3) + base__extra_size + data__extra_size;

    /* Check if message fits in reported side */
    if (start + nw_size > message_end) {
        return NULL;
    }

    /* Validated extents and calculated size */
    data = (uint8_t *)malloc(mem_size);
    if (SPICE_UNLIKELY(data == NULL)) {
        goto error;
    }
    end = data + sizeof(SpiceMsgDisplayDrawRop3);
    in = start;

    out = (SpiceMsgDisplayDrawRop3 *)data;

    /* base */ {
        out->base.surface_id = consume_uint32(&in);
        /* box */ {
            out->base.box.top = consume_int32(&in);
            out->base.box.left = consume_int32(&in);
            out->base.box.bottom = consume_int32(&in);
            out->base.box.right = consume_int32(&in);
        }
        /* clip */ {
            out->base.clip.type = consume_uint8(&in);
            if (out->base.clip.type == SPICE_CLIP_TYPE_RECTS) {
                ptr_info[n_ptr].offset = in - start;
                ptr_info[n_ptr].parse = parse_struct_SpiceClipRects;
                ptr_info[n_ptr].dest = (void **)&out->base.clip.rects;
                n_ptr++;
                in += rects__saved_size;
            }
        }
    }
    /* data */ {
        ptr_info[n_ptr].offset = consume_uint32(&in);
        ptr_info[n_ptr].parse = parse_struct_SpiceImage;
        ptr_info[n_ptr].dest = (void **)&out->data.src_bitmap;
        n_ptr++;
        /* src_area */ {
            out->data.src_area.top = consume_int32(&in);
            out->data.src_area.left = consume_int32(&in);
            out->data.src_area.bottom = consume_int32(&in);
            out->data.src_area.right = consume_int32(&in);
        }
        /* brush */ {
            out->data.brush.type = consume_uint8(&in);
            if (out->data.brush.type == SPICE_BRUSH_TYPE_SOLID) {
                out->data.brush.u.color = consume_uint32(&in);
            } else if (out->data.brush.type == SPICE_BRUSH_TYPE_PATTERN) {
                ptr_info[n_ptr].offset = consume_uint32(&in);
                ptr_info[n_ptr].parse = parse_struct_SpiceImage;
                ptr_info[n_ptr].dest = (void **)&out->data.brush.u.pattern.pat;
                n_ptr++;
                /* pos */ {
                    out->data.brush.u.pattern.pos.x = consume_int32(&in);
                    out->data.brush.u.pattern.pos.y = consume_int32(&in);
                }
            }
        }
        out->data.rop3 = consume_uint8(&in);
        out->data.scale_mode = consume_uint8(&in);
        /* mask */ {
            out->data.mask.flags = consume_uint8(&in);
            /* pos */ {
                out->data.mask.pos.x = consume_int32(&in);
                out->data.mask.pos.y = consume_int32(&in);
            }
            ptr_info[n_ptr].offset = consume_uint32(&in);
            ptr_info[n_ptr].parse = parse_struct_SpiceImage;
            ptr_info[n_ptr].dest = (void **)&out->data.mask.bitmap;
            n_ptr++;
        }
    }

    assert(in <= message_end);

    for (i = 0; i < n_ptr; i++) {
        if (ptr_info[i].offset == 0) {
            *ptr_info[i].dest = NULL;
        } else {
            /* Align to 32 bit */
            end = (uint8_t *)SPICE_ALIGN((size_t)end, 4);
            *ptr_info[i].dest = (void *)end;
            end = ptr_info[i].parse(message_start, message_end, end, &ptr_info[i], minor);
            if (SPICE_UNLIKELY(end == NULL)) {
                goto error;
            }
        }
    }

    assert(end <= data + mem_size);

    *size = end - data;
    *free_message = (message_destructor_t) free;
    return data;

   error:
    if (data != NULL) {
        free(data);
    }
    return NULL;
}

static intptr_t validate_SpicePath(uint8_t *message_start, uint8_t *message_end, uint64_t offset, int minor)
{
    uint8_t *start = message_start + offset;
    SPICE_GNUC_UNUSED uint8_t *pos;
    size_t mem_size, nw_size;
    size_t segments__nw_size, segments__mem_size;
    uint32_t segments__nelements;
    uint32_t i;

    if (offset == 0) {
        return 0;
    }

    if (SPICE_UNLIKELY(start >= message_end)) {
        goto error;
    }

    { /* segments */
        uint32_t num_segments__value;
        uint8_t *start2 = (start + 4);
        uint32_t segments__element__nw_size;
        uint32_t segments__element__mem_size;
        pos = start + 0;
        if (SPICE_UNLIKELY(pos + 4 > message_end)) {
            goto error;
        }
        num_segments__value = read_uint32(pos);
        segments__nelements = num_segments__value;

        segments__nw_size = 0;
        segments__mem_size = 0;
        for (i = 0; i < segments__nelements; i++) {
            SPICE_GNUC_UNUSED uint8_t *start3 = start2;
            size_t points__nw_size, points__mem_size;
            uint32_t points__nelements;
            { /* points */
                uint32_t count__value;
                pos = start3 + 1;
                if (SPICE_UNLIKELY(pos + 4 > message_end)) {
                    goto error;
                }
                count__value = read_uint32(pos);
                points__nelements = count__value;

                points__nw_size = (8) * points__nelements;
                points__mem_size = sizeof(SpicePointFix) * points__nelements;
            }

            segments__element__nw_size = 5 + points__nw_size;
            segments__element__mem_size = sizeof(SpicePathSeg) + points__mem_size;
            segments__nw_size += segments__element__nw_size;
            segments__mem_size += sizeof(void *) + SPICE_ALIGN(segments__element__mem_size, 4);
            start2 += segments__element__nw_size;
        }
    }

    nw_size = 4 + segments__nw_size;
    mem_size = sizeof(SpicePath) + segments__mem_size;

    /* Check if struct fits in reported side */
    if (SPICE_UNLIKELY(start + nw_size > message_end)) {
        goto error;
    }
    return mem_size;

   error:
    return -1;
}

static uint8_t * parse_struct_SpicePath(uint8_t *message_start, uint8_t *message_end, uint8_t *struct_data, PointerInfo *this_ptr_info, int minor)
{
    uint8_t *in = message_start + this_ptr_info->offset;
    uint8_t *end;
    SpicePath *out;
    uint32_t segments__nelements;
    uint32_t i;
    void * *ptr_array;
    int ptr_array_index;
    uint32_t j;

    end = struct_data + sizeof(SpicePath);
    out = (SpicePath *)struct_data;

    out->num_segments = consume_uint32(&in);
    segments__nelements = out->num_segments;
    ptr_array_index = 0;
    ptr_array = (void **)out->segments;
    end += sizeof(void *) * segments__nelements;
    for (i = 0; i < segments__nelements; i++) {
        SpicePathSeg *out2;
        uint32_t points__nelements;
        ptr_array[ptr_array_index++] = end;
        out2 = (SpicePathSeg *)end;
        end += sizeof(SpicePathSeg);

        out2->flags = consume_uint8(&in);
        out2->count = consume_uint32(&in);
        points__nelements = out2->count;
        for (j = 0; j < points__nelements; j++) {
            SpicePointFix *out3;
            out3 = (SpicePointFix *)end;
            end += sizeof(SpicePointFix);

            out3->x = consume_int32(&in);
            out3->y = consume_int32(&in);
        }
        /* Align ptr_array element to 4 bytes */
        end = (uint8_t *)SPICE_ALIGN((size_t)end, 4);
    }
    return end;
}

static uint8_t * parse_array_int32(uint8_t *message_start, uint8_t *message_end, uint8_t *struct_data, PointerInfo *this_ptr_info, int minor)
{
    uint8_t *in = message_start + this_ptr_info->offset;
    uint8_t *end;
    uint32_t i;

    end = struct_data;
    for (i = 0; i < this_ptr_info->nelements; i++) {
        *(SPICE_FIXED28_4 *)end = consume_int32(&in);
        end += sizeof(SPICE_FIXED28_4);
    }
    return end;
}

static uint8_t * parse_msg_display_draw_stroke(uint8_t *message_start, uint8_t *message_end, int minor, size_t *size, message_destructor_t *free_message)
{
    SPICE_GNUC_UNUSED uint8_t *pos;
    uint8_t *start = message_start;
    uint8_t *data = NULL;
    size_t mem_size, nw_size;
    uint8_t *in, *end;
    SPICE_GNUC_UNUSED intptr_t ptr_size;
    uint32_t n_ptr=0;
    PointerInfo ptr_info[4];
    size_t base__nw_size, base__extra_size;
    uint32_t rects__saved_size = 0;
    size_t data__nw_size, data__extra_size;
    SpiceMsgDisplayDrawStroke *out;
    uint32_t i;

    { /* base */
        SPICE_GNUC_UNUSED uint8_t *start2 = (start + 0);
        size_t clip__nw_size, clip__extra_size;
        { /* clip */
            SPICE_GNUC_UNUSED uint8_t *start3 = (start2 + 20);
            size_t u__nw_size, u__extra_size;
            uint8_t type__value;
            { /* u */
                uint32_t u__mem_size;
                pos = start3 + 0;
                if (SPICE_UNLIKELY(pos + 1 > message_end)) {
                    goto error;
                }
                type__value = read_uint8(pos);
                if (type__value == SPICE_CLIP_TYPE_RECTS) {
                    SPICE_GNUC_UNUSED uint8_t *start4 = (start3 + 1);
                    size_t rects__nw_size, rects__mem_size;
                    uint32_t rects__nelements;
                    { /* rects */
                        uint32_t num_rects__value;
                        pos = start4 + 0;
                        if (SPICE_UNLIKELY(pos + 4 > message_end)) {
                            goto error;
                        }
                        num_rects__value = read_uint32(pos);
                        rects__nelements = num_rects__value;

                        rects__nw_size = (16) * rects__nelements;
                        rects__mem_size = sizeof(SpiceRect) * rects__nelements;
                    }

                    u__nw_size = 4 + rects__nw_size;
                    u__mem_size = sizeof(SpiceClipRects) + rects__mem_size;
                    rects__saved_size = u__nw_size;
                    u__extra_size = u__mem_size;
                } else {
                    u__nw_size = 0;
                    u__extra_size = 0;
                }

            }

            clip__nw_size = 1 + u__nw_size;
            clip__extra_size = u__extra_size;
        }

        base__nw_size = 20 + clip__nw_size;
        base__extra_size = clip__extra_size;
    }

    { /* data */
        SPICE_GNUC_UNUSED uint8_t *start2 = (start + 0 + base__nw_size);
        size_t path__extra_size;
        size_t attr__nw_size, attr__extra_size;
        size_t brush__nw_size, brush__extra_size;
        { /* path */
            uint32_t path__value;
            pos = (start2 + 0);
            if (SPICE_UNLIKELY(pos + 4 > message_end)) {
                goto error;
            }
            path__value = read_uint32(pos);
            if (SPICE_UNLIKELY(path__value == 0)) {
                goto error;
            }
            ptr_size = validate_SpicePath(message_start, message_end, path__value, minor);
            if (SPICE_UNLIKELY(ptr_size < 0)) {
                goto error;
            }
            path__extra_size = ptr_size + /* for alignment */ 3;
        }

        { /* attr */
            SPICE_GNUC_UNUSED uint8_t *start3 = (start2 + 4);
            size_t u1__nw_size;
            uint8_t flags__value;
            size_t u2__nw_size, u2__extra_size;
            { /* u1 */
                pos = start3 + 0;
                if (SPICE_UNLIKELY(pos + 1 > message_end)) {
                    goto error;
                }
                flags__value = read_uint8(pos);
                if ((flags__value & SPICE_LINE_FLAGS_STYLED)) {
                    u1__nw_size = 1;
                } else {
                    u1__nw_size = 0;
                }

            }

            { /* u2 */
                uint32_t u2__array__nelements;
                pos = start3 + 0;
                if (SPICE_UNLIKELY(pos + 1 > message_end)) {
                    goto error;
                }
                flags__value = read_uint8(pos);
                if ((flags__value & SPICE_LINE_FLAGS_STYLED)) {
                    uint32_t u2_style__value;
                    uint32_t u2__array__nw_size;
                    uint32_t u2__array__mem_size;
                    uint8_t style_nseg__value;
                    u2__nw_size = 4;
                    pos = (start3 + 1 + u1__nw_size);
                    if (SPICE_UNLIKELY(pos + 4 > message_end)) {
                        goto error;
                    }
                    u2_style__value = read_uint32(pos);
                    if (SPICE_UNLIKELY(message_start + u2_style__value >= message_end)) {
                        goto error;
                    }
                    pos = start3 + 1;
                    if (SPICE_UNLIKELY(pos + 1 > message_end)) {
                        goto error;
                    }
                    style_nseg__value = read_uint8(pos);
                    u2__array__nelements = style_nseg__value;

                    u2__array__nw_size = (4) * u2__array__nelements;
                    u2__array__mem_size = sizeof(SPICE_FIXED28_4) * u2__array__nelements;
                    if (SPICE_UNLIKELY(message_start + u2_style__value + u2__array__nw_size > message_end)) {
                        goto error;
                    }
                    u2__extra_size = u2__array__mem_size + /* for alignment */ 3;
                } else {
                    u2__nw_size = 0;
                    u2__extra_size = 0;
                }

            }

            attr__nw_size = 1 + u1__nw_size + u2__nw_size;
            attr__extra_size = u2__extra_size;
        }

        { /* brush */
            SPICE_GNUC_UNUSED uint8_t *start3 = (start2 + 4 + attr__nw_size);
            size_t u__nw_size, u__extra_size;
            uint8_t type__value;
            { /* u */
                pos = start3 + 0;
                if (SPICE_UNLIKELY(pos + 1 > message_end)) {
                    goto error;
                }
                type__value = read_uint8(pos);
                if (type__value == SPICE_BRUSH_TYPE_SOLID) {
                    u__nw_size = 4;
                    u__extra_size = 0;
                } else if (type__value == SPICE_BRUSH_TYPE_PATTERN) {
                    SPICE_GNUC_UNUSED uint8_t *start4 = (start3 + 1);
                    size_t pat__extra_size;
                    { /* pat */
                        uint32_t pat__value;
                        pos = (start4 + 0);
                        if (SPICE_UNLIKELY(pos + 4 > message_end)) {
                            goto error;
                        }
                        pat__value = read_uint32(pos);
                        if (SPICE_UNLIKELY(pat__value == 0)) {
                            goto error;
                        }
                        ptr_size = validate_SpiceImage(message_start, message_end, pat__value, minor);
                        if (SPICE_UNLIKELY(ptr_size < 0)) {
                            goto error;
                        }
                        pat__extra_size = ptr_size + /* for alignment */ 3;
                    }

                    u__nw_size = 12;
                    u__extra_size = pat__extra_size;
                } else {
                    u__nw_size = 0;
                    u__extra_size = 0;
                }

            }

            brush__nw_size = 1 + u__nw_size;
            brush__extra_size = u__extra_size;
        }

        data__nw_size = 8 + attr__nw_size + brush__nw_size;
        data__extra_size = path__extra_size + attr__extra_size + brush__extra_size;
    }

    nw_size = 0 + base__nw_size + data__nw_size;
    mem_size = sizeof(SpiceMsgDisplayDrawStroke) + base__extra_size + data__extra_size;

    /* Check if message fits in reported side */
    if (start + nw_size > message_end) {
        return NULL;
    }

    /* Validated extents and calculated size */
    data = (uint8_t *)malloc(mem_size);
    if (SPICE_UNLIKELY(data == NULL)) {
        goto error;
    }
    end = data + sizeof(SpiceMsgDisplayDrawStroke);
    in = start;

    out = (SpiceMsgDisplayDrawStroke *)data;

    /* base */ {
        out->base.surface_id = consume_uint32(&in);
        /* box */ {
            out->base.box.top = consume_int32(&in);
            out->base.box.left = consume_int32(&in);
            out->base.box.bottom = consume_int32(&in);
            out->base.box.right = consume_int32(&in);
        }
        /* clip */ {
            out->base.clip.type = consume_uint8(&in);
            if (out->base.clip.type == SPICE_CLIP_TYPE_RECTS) {
                ptr_info[n_ptr].offset = in - start;
                ptr_info[n_ptr].parse = parse_struct_SpiceClipRects;
                ptr_info[n_ptr].dest = (void **)&out->base.clip.rects;
                n_ptr++;
                in += rects__saved_size;
            }
        }
    }
    /* data */ {
        ptr_info[n_ptr].offset = consume_uint32(&in);
        ptr_info[n_ptr].parse = parse_struct_SpicePath;
        ptr_info[n_ptr].dest = (void **)&out->data.path;
        n_ptr++;
        /* attr */ {
            out->data.attr.flags = consume_uint8(&in);
            if ((out->data.attr.flags & SPICE_LINE_FLAGS_STYLED)) {
                out->data.attr.style_nseg = consume_uint8(&in);
            }
            if ((out->data.attr.flags & SPICE_LINE_FLAGS_STYLED)) {
                uint32_t style__array__nelements;
                ptr_info[n_ptr].offset = consume_uint32(&in);
                ptr_info[n_ptr].parse = parse_array_int32;
                ptr_info[n_ptr].dest = (void **)&out->data.attr.style;
                style__array__nelements = out->data.attr.style_nseg;
                ptr_info[n_ptr].nelements = style__array__nelements;
                n_ptr++;
            }
        }
        /* brush */ {
            out->data.brush.type = consume_uint8(&in);
            if (out->data.brush.type == SPICE_BRUSH_TYPE_SOLID) {
                out->data.brush.u.color = consume_uint32(&in);
            } else if (out->data.brush.type == SPICE_BRUSH_TYPE_PATTERN) {
                ptr_info[n_ptr].offset = consume_uint32(&in);
                ptr_info[n_ptr].parse = parse_struct_SpiceImage;
                ptr_info[n_ptr].dest = (void **)&out->data.brush.u.pattern.pat;
                n_ptr++;
                /* pos */ {
                    out->data.brush.u.pattern.pos.x = consume_int32(&in);
                    out->data.brush.u.pattern.pos.y = consume_int32(&in);
                }
            }
        }
        out->data.fore_mode = consume_uint16(&in);
        out->data.back_mode = consume_uint16(&in);
    }

    assert(in <= message_end);

    for (i = 0; i < n_ptr; i++) {
        if (ptr_info[i].offset == 0) {
            *ptr_info[i].dest = NULL;
        } else {
            /* Align to 32 bit */
            end = (uint8_t *)SPICE_ALIGN((size_t)end, 4);
            *ptr_info[i].dest = (void *)end;
            end = ptr_info[i].parse(message_start, message_end, end, &ptr_info[i], minor);
            if (SPICE_UNLIKELY(end == NULL)) {
                goto error;
            }
        }
    }

    assert(end <= data + mem_size);

    *size = end - data;
    *free_message = (message_destructor_t) free;
    return data;

   error:
    if (data != NULL) {
        free(data);
    }
    return NULL;
}

static intptr_t validate_SpiceString(uint8_t *message_start, uint8_t *message_end, uint64_t offset, int minor)
{
    uint8_t *start = message_start + offset;
    SPICE_GNUC_UNUSED uint8_t *pos;
    size_t mem_size, nw_size;
    size_t u__nw_size, u__extra_size;
    uint8_t flags__value;
    uint32_t i;

    if (offset == 0) {
        return 0;
    }

    if (SPICE_UNLIKELY(start >= message_end)) {
        goto error;
    }

    { /* u */
        uint32_t u__mem_size;
        uint32_t u__nelements;
        pos = start + 2;
        if (SPICE_UNLIKELY(pos + 1 > message_end)) {
            goto error;
        }
        flags__value = read_uint8(pos);
        if ((flags__value & SPICE_STRING_FLAGS_RASTER_A1)) {
            uint16_t length__value;
            uint8_t *start2 = (start + 3);
            uint32_t u__element__nw_size;
            uint32_t u__element__mem_size;
            pos = start + 0;
            if (SPICE_UNLIKELY(pos + 2 > message_end)) {
                goto error;
            }
            length__value = read_uint16(pos);
            u__nelements = length__value;

            u__nw_size = 0;
            u__mem_size = 0;
            for (i = 0; i < u__nelements; i++) {
                SPICE_GNUC_UNUSED uint8_t *start3 = start2;
                size_t data__nw_size, data__mem_size;
                uint32_t data__nelements;
                { /* data */
                    uint16_t width__value;
                    uint16_t height__value;
                    pos = start3 + 16;
                    if (SPICE_UNLIKELY(pos + 2 > message_end)) {
                        goto error;
                    }
                    width__value = read_uint16(pos);
                    pos = start3 + 18;
                    if (SPICE_UNLIKELY(pos + 2 > message_end)) {
                        goto error;
                    }
                    height__value = read_uint16(pos);
                    data__nelements = ((width__value + 7) / 8 ) * height__value;

                    data__nw_size = data__nelements;
                    data__mem_size = sizeof(uint8_t) * data__nelements;
                }

                u__element__nw_size = 20 + data__nw_size;
                u__element__mem_size = sizeof(SpiceRasterGlyph) + data__mem_size;
                u__nw_size += u__element__nw_size;
                u__mem_size += sizeof(void *) + SPICE_ALIGN(u__element__mem_size, 4);
                start2 += u__element__nw_size;
            }
            u__extra_size = u__mem_size;
        } else if ((flags__value & SPICE_STRING_FLAGS_RASTER_A4)) {
            uint16_t length__value;
            uint8_t *start2 = (start + 3);
            uint32_t u__element__nw_size;
            uint32_t u__element__mem_size;
            pos = start + 0;
            if (SPICE_UNLIKELY(pos + 2 > message_end)) {
                goto error;
            }
            length__value = read_uint16(pos);
            u__nelements = length__value;

            u__nw_size = 0;
            u__mem_size = 0;
            for (i = 0; i < u__nelements; i++) {
                SPICE_GNUC_UNUSED uint8_t *start3 = start2;
                size_t data__nw_size, data__mem_size;
                uint32_t data__nelements;
                { /* data */
                    uint16_t width__value;
                    uint16_t height__value;
                    pos = start3 + 16;
                    if (SPICE_UNLIKELY(pos + 2 > message_end)) {
                        goto error;
                    }
                    width__value = read_uint16(pos);
                    pos = start3 + 18;
                    if (SPICE_UNLIKELY(pos + 2 > message_end)) {
                        goto error;
                    }
                    height__value = read_uint16(pos);
                    data__nelements = ((4 * width__value + 7) / 8 ) * height__value;

                    data__nw_size = data__nelements;
                    data__mem_size = sizeof(uint8_t) * data__nelements;
                }

                u__element__nw_size = 20 + data__nw_size;
                u__element__mem_size = sizeof(SpiceRasterGlyph) + data__mem_size;
                u__nw_size += u__element__nw_size;
                u__mem_size += sizeof(void *) + SPICE_ALIGN(u__element__mem_size, 4);
                start2 += u__element__nw_size;
            }
            u__extra_size = u__mem_size;
        } else if ((flags__value & SPICE_STRING_FLAGS_RASTER_A8)) {
            uint16_t length__value;
            uint8_t *start2 = (start + 3);
            uint32_t u__element__nw_size;
            uint32_t u__element__mem_size;
            pos = start + 0;
            if (SPICE_UNLIKELY(pos + 2 > message_end)) {
                goto error;
            }
            length__value = read_uint16(pos);
            u__nelements = length__value;

            u__nw_size = 0;
            u__mem_size = 0;
            for (i = 0; i < u__nelements; i++) {
                SPICE_GNUC_UNUSED uint8_t *start3 = start2;
                size_t data__nw_size, data__mem_size;
                uint32_t data__nelements;
                { /* data */
                    uint16_t width__value;
                    uint16_t height__value;
                    pos = start3 + 16;
                    if (SPICE_UNLIKELY(pos + 2 > message_end)) {
                        goto error;
                    }
                    width__value = read_uint16(pos);
                    pos = start3 + 18;
                    if (SPICE_UNLIKELY(pos + 2 > message_end)) {
                        goto error;
                    }
                    height__value = read_uint16(pos);
                    data__nelements = width__value * height__value;

                    data__nw_size = data__nelements;
                    data__mem_size = sizeof(uint8_t) * data__nelements;
                }

                u__element__nw_size = 20 + data__nw_size;
                u__element__mem_size = sizeof(SpiceRasterGlyph) + data__mem_size;
                u__nw_size += u__element__nw_size;
                u__mem_size += sizeof(void *) + SPICE_ALIGN(u__element__mem_size, 4);
                start2 += u__element__nw_size;
            }
            u__extra_size = u__mem_size;
        } else {
            u__nw_size = 0;
            u__extra_size = 0;
        }

    }

    nw_size = 3 + u__nw_size;
    mem_size = sizeof(SpiceString) + u__extra_size;

    /* Check if struct fits in reported side */
    if (SPICE_UNLIKELY(start + nw_size > message_end)) {
        goto error;
    }
    return mem_size;

   error:
    return -1;
}

static uint8_t * parse_struct_SpiceString(uint8_t *message_start, uint8_t *message_end, uint8_t *struct_data, PointerInfo *this_ptr_info, int minor)
{
    uint8_t *in = message_start + this_ptr_info->offset;
    uint8_t *end;
    SpiceString *out;
    uint32_t i;

    end = struct_data + sizeof(SpiceString);
    out = (SpiceString *)struct_data;

    out->length = consume_uint16(&in);
    out->flags = consume_uint8(&in);
    if ((out->flags & SPICE_STRING_FLAGS_RASTER_A1)) {
        uint32_t glyphs__nelements;
        void * *ptr_array;
        int ptr_array_index;
        glyphs__nelements = out->length;
        ptr_array_index = 0;
        ptr_array = (void **)out->glyphs;
        end += sizeof(void *) * glyphs__nelements;
        for (i = 0; i < glyphs__nelements; i++) {
            SpiceRasterGlyph *out2;
            uint32_t data__nelements;
            ptr_array[ptr_array_index++] = end;
            out2 = (SpiceRasterGlyph *)end;
            end += sizeof(SpiceRasterGlyph);

            /* render_pos */ {
                out2->render_pos.x = consume_int32(&in);
                out2->render_pos.y = consume_int32(&in);
            }
            /* glyph_origin */ {
                out2->glyph_origin.x = consume_int32(&in);
                out2->glyph_origin.y = consume_int32(&in);
            }
            out2->width = consume_uint16(&in);
            out2->height = consume_uint16(&in);
            data__nelements = ((out2->width + 7) / 8 ) * out2->height;
            memcpy(out2->data, in, data__nelements);
            in += data__nelements;
            end += data__nelements;
            /* Align ptr_array element to 4 bytes */
            end = (uint8_t *)SPICE_ALIGN((size_t)end, 4);
        }
    } else if ((out->flags & SPICE_STRING_FLAGS_RASTER_A4)) {
        uint32_t glyphs__nelements;
        void * *ptr_array;
        int ptr_array_index;
        glyphs__nelements = out->length;
        ptr_array_index = 0;
        ptr_array = (void **)out->glyphs;
        end += sizeof(void *) * glyphs__nelements;
        for (i = 0; i < glyphs__nelements; i++) {
            SpiceRasterGlyph *out2;
            uint32_t data__nelements;
            ptr_array[ptr_array_index++] = end;
            out2 = (SpiceRasterGlyph *)end;
            end += sizeof(SpiceRasterGlyph);

            /* render_pos */ {
                out2->render_pos.x = consume_int32(&in);
                out2->render_pos.y = consume_int32(&in);
            }
            /* glyph_origin */ {
                out2->glyph_origin.x = consume_int32(&in);
                out2->glyph_origin.y = consume_int32(&in);
            }
            out2->width = consume_uint16(&in);
            out2->height = consume_uint16(&in);
            data__nelements = ((4 * out2->width + 7) / 8 ) * out2->height;
            memcpy(out2->data, in, data__nelements);
            in += data__nelements;
            end += data__nelements;
            /* Align ptr_array element to 4 bytes */
            end = (uint8_t *)SPICE_ALIGN((size_t)end, 4);
        }
    } else if ((out->flags & SPICE_STRING_FLAGS_RASTER_A8)) {
        uint32_t glyphs__nelements;
        void * *ptr_array;
        int ptr_array_index;
        glyphs__nelements = out->length;
        ptr_array_index = 0;
        ptr_array = (void **)out->glyphs;
        end += sizeof(void *) * glyphs__nelements;
        for (i = 0; i < glyphs__nelements; i++) {
            SpiceRasterGlyph *out2;
            uint32_t data__nelements;
            ptr_array[ptr_array_index++] = end;
            out2 = (SpiceRasterGlyph *)end;
            end += sizeof(SpiceRasterGlyph);

            /* render_pos */ {
                out2->render_pos.x = consume_int32(&in);
                out2->render_pos.y = consume_int32(&in);
            }
            /* glyph_origin */ {
                out2->glyph_origin.x = consume_int32(&in);
                out2->glyph_origin.y = consume_int32(&in);
            }
            out2->width = consume_uint16(&in);
            out2->height = consume_uint16(&in);
            data__nelements = out2->width * out2->height;
            memcpy(out2->data, in, data__nelements);
            in += data__nelements;
            end += data__nelements;
            /* Align ptr_array element to 4 bytes */
            end = (uint8_t *)SPICE_ALIGN((size_t)end, 4);
        }
    }
    return end;
}

static uint8_t * parse_msg_display_draw_text(uint8_t *message_start, uint8_t *message_end, int minor, size_t *size, message_destructor_t *free_message)
{
    SPICE_GNUC_UNUSED uint8_t *pos;
    uint8_t *start = message_start;
    uint8_t *data = NULL;
    size_t mem_size, nw_size;
    uint8_t *in, *end;
    SPICE_GNUC_UNUSED intptr_t ptr_size;
    uint32_t n_ptr=0;
    PointerInfo ptr_info[4];
    size_t base__nw_size, base__extra_size;
    uint32_t rects__saved_size = 0;
    size_t data__nw_size, data__extra_size;
    SpiceMsgDisplayDrawText *out;
    uint32_t i;

    { /* base */
        SPICE_GNUC_UNUSED uint8_t *start2 = (start + 0);
        size_t clip__nw_size, clip__extra_size;
        { /* clip */
            SPICE_GNUC_UNUSED uint8_t *start3 = (start2 + 20);
            size_t u__nw_size, u__extra_size;
            uint8_t type__value;
            { /* u */
                uint32_t u__mem_size;
                pos = start3 + 0;
                if (SPICE_UNLIKELY(pos + 1 > message_end)) {
                    goto error;
                }
                type__value = read_uint8(pos);
                if (type__value == SPICE_CLIP_TYPE_RECTS) {
                    SPICE_GNUC_UNUSED uint8_t *start4 = (start3 + 1);
                    size_t rects__nw_size, rects__mem_size;
                    uint32_t rects__nelements;
                    { /* rects */
                        uint32_t num_rects__value;
                        pos = start4 + 0;
                        if (SPICE_UNLIKELY(pos + 4 > message_end)) {
                            goto error;
                        }
                        num_rects__value = read_uint32(pos);
                        rects__nelements = num_rects__value;

                        rects__nw_size = (16) * rects__nelements;
                        rects__mem_size = sizeof(SpiceRect) * rects__nelements;
                    }

                    u__nw_size = 4 + rects__nw_size;
                    u__mem_size = sizeof(SpiceClipRects) + rects__mem_size;
                    rects__saved_size = u__nw_size;
                    u__extra_size = u__mem_size;
                } else {
                    u__nw_size = 0;
                    u__extra_size = 0;
                }

            }

            clip__nw_size = 1 + u__nw_size;
            clip__extra_size = u__extra_size;
        }

        base__nw_size = 20 + clip__nw_size;
        base__extra_size = clip__extra_size;
    }

    { /* data */
        SPICE_GNUC_UNUSED uint8_t *start2 = (start + 0 + base__nw_size);
        size_t str__extra_size;
        size_t fore_brush__nw_size, fore_brush__extra_size;
        size_t back_brush__nw_size, back_brush__extra_size;
        { /* str */
            uint32_t str__value;
            pos = (start2 + 0);
            if (SPICE_UNLIKELY(pos + 4 > message_end)) {
                goto error;
            }
            str__value = read_uint32(pos);
            if (SPICE_UNLIKELY(str__value == 0)) {
                goto error;
            }
            ptr_size = validate_SpiceString(message_start, message_end, str__value, minor);
            if (SPICE_UNLIKELY(ptr_size < 0)) {
                goto error;
            }
            str__extra_size = ptr_size + /* for alignment */ 3;
        }

        { /* fore_brush */
            SPICE_GNUC_UNUSED uint8_t *start3 = (start2 + 20);
            size_t u__nw_size, u__extra_size;
            uint8_t type__value;
            { /* u */
                pos = start3 + 0;
                if (SPICE_UNLIKELY(pos + 1 > message_end)) {
                    goto error;
                }
                type__value = read_uint8(pos);
                if (type__value == SPICE_BRUSH_TYPE_SOLID) {
                    u__nw_size = 4;
                    u__extra_size = 0;
                } else if (type__value == SPICE_BRUSH_TYPE_PATTERN) {
                    SPICE_GNUC_UNUSED uint8_t *start4 = (start3 + 1);
                    size_t pat__extra_size;
                    { /* pat */
                        uint32_t pat__value;
                        pos = (start4 + 0);
                        if (SPICE_UNLIKELY(pos + 4 > message_end)) {
                            goto error;
                        }
                        pat__value = read_uint32(pos);
                        if (SPICE_UNLIKELY(pat__value == 0)) {
                            goto error;
                        }
                        ptr_size = validate_SpiceImage(message_start, message_end, pat__value, minor);
                        if (SPICE_UNLIKELY(ptr_size < 0)) {
                            goto error;
                        }
                        pat__extra_size = ptr_size + /* for alignment */ 3;
                    }

                    u__nw_size = 12;
                    u__extra_size = pat__extra_size;
                } else {
                    u__nw_size = 0;
                    u__extra_size = 0;
                }

            }

            fore_brush__nw_size = 1 + u__nw_size;
            fore_brush__extra_size = u__extra_size;
        }

        { /* back_brush */
            SPICE_GNUC_UNUSED uint8_t *start3 = (start2 + 20 + fore_brush__nw_size);
            size_t u__nw_size, u__extra_size;
            uint8_t type__value;
            { /* u */
                pos = start3 + 0;
                if (SPICE_UNLIKELY(pos + 1 > message_end)) {
                    goto error;
                }
                type__value = read_uint8(pos);
                if (type__value == SPICE_BRUSH_TYPE_SOLID) {
                    u__nw_size = 4;
                    u__extra_size = 0;
                } else if (type__value == SPICE_BRUSH_TYPE_PATTERN) {
                    SPICE_GNUC_UNUSED uint8_t *start4 = (start3 + 1);
                    size_t pat__extra_size;
                    { /* pat */
                        uint32_t pat__value;
                        pos = (start4 + 0);
                        if (SPICE_UNLIKELY(pos + 4 > message_end)) {
                            goto error;
                        }
                        pat__value = read_uint32(pos);
                        if (SPICE_UNLIKELY(pat__value == 0)) {
                            goto error;
                        }
                        ptr_size = validate_SpiceImage(message_start, message_end, pat__value, minor);
                        if (SPICE_UNLIKELY(ptr_size < 0)) {
                            goto error;
                        }
                        pat__extra_size = ptr_size + /* for alignment */ 3;
                    }

                    u__nw_size = 12;
                    u__extra_size = pat__extra_size;
                } else {
                    u__nw_size = 0;
                    u__extra_size = 0;
                }

            }

            back_brush__nw_size = 1 + u__nw_size;
            back_brush__extra_size = u__extra_size;
        }

        data__nw_size = 24 + fore_brush__nw_size + back_brush__nw_size;
        data__extra_size = str__extra_size + fore_brush__extra_size + back_brush__extra_size;
    }

    nw_size = 0 + base__nw_size + data__nw_size;
    mem_size = sizeof(SpiceMsgDisplayDrawText) + base__extra_size + data__extra_size;

    /* Check if message fits in reported side */
    if (start + nw_size > message_end) {
        return NULL;
    }

    /* Validated extents and calculated size */
    data = (uint8_t *)malloc(mem_size);
    if (SPICE_UNLIKELY(data == NULL)) {
        goto error;
    }
    end = data + sizeof(SpiceMsgDisplayDrawText);
    in = start;

    out = (SpiceMsgDisplayDrawText *)data;

    /* base */ {
        out->base.surface_id = consume_uint32(&in);
        /* box */ {
            out->base.box.top = consume_int32(&in);
            out->base.box.left = consume_int32(&in);
            out->base.box.bottom = consume_int32(&in);
            out->base.box.right = consume_int32(&in);
        }
        /* clip */ {
            out->base.clip.type = consume_uint8(&in);
            if (out->base.clip.type == SPICE_CLIP_TYPE_RECTS) {
                ptr_info[n_ptr].offset = in - start;
                ptr_info[n_ptr].parse = parse_struct_SpiceClipRects;
                ptr_info[n_ptr].dest = (void **)&out->base.clip.rects;
                n_ptr++;
                in += rects__saved_size;
            }
        }
    }
    /* data */ {
        ptr_info[n_ptr].offset = consume_uint32(&in);
        ptr_info[n_ptr].parse = parse_struct_SpiceString;
        ptr_info[n_ptr].dest = (void **)&out->data.str;
        n_ptr++;
        /* back_area */ {
            out->data.back_area.top = consume_int32(&in);
            out->data.back_area.left = consume_int32(&in);
            out->data.back_area.bottom = consume_int32(&in);
            out->data.back_area.right = consume_int32(&in);
        }
        /* fore_brush */ {
            out->data.fore_brush.type = consume_uint8(&in);
            if (out->data.fore_brush.type == SPICE_BRUSH_TYPE_SOLID) {
                out->data.fore_brush.u.color = consume_uint32(&in);
            } else if (out->data.fore_brush.type == SPICE_BRUSH_TYPE_PATTERN) {
                ptr_info[n_ptr].offset = consume_uint32(&in);
                ptr_info[n_ptr].parse = parse_struct_SpiceImage;
                ptr_info[n_ptr].dest = (void **)&out->data.fore_brush.u.pattern.pat;
                n_ptr++;
                /* pos */ {
                    out->data.fore_brush.u.pattern.pos.x = consume_int32(&in);
                    out->data.fore_brush.u.pattern.pos.y = consume_int32(&in);
                }
            }
        }
        /* back_brush */ {
            out->data.back_brush.type = consume_uint8(&in);
            if (out->data.back_brush.type == SPICE_BRUSH_TYPE_SOLID) {
                out->data.back_brush.u.color = consume_uint32(&in);
            } else if (out->data.back_brush.type == SPICE_BRUSH_TYPE_PATTERN) {
                ptr_info[n_ptr].offset = consume_uint32(&in);
                ptr_info[n_ptr].parse = parse_struct_SpiceImage;
                ptr_info[n_ptr].dest = (void **)&out->data.back_brush.u.pattern.pat;
                n_ptr++;
                /* pos */ {
                    out->data.back_brush.u.pattern.pos.x = consume_int32(&in);
                    out->data.back_brush.u.pattern.pos.y = consume_int32(&in);
                }
            }
        }
        out->data.fore_mode = consume_uint16(&in);
        out->data.back_mode = consume_uint16(&in);
    }

    assert(in <= message_end);

    for (i = 0; i < n_ptr; i++) {
        if (ptr_info[i].offset == 0) {
            *ptr_info[i].dest = NULL;
        } else {
            /* Align to 32 bit */
            end = (uint8_t *)SPICE_ALIGN((size_t)end, 4);
            *ptr_info[i].dest = (void *)end;
            end = ptr_info[i].parse(message_start, message_end, end, &ptr_info[i], minor);
            if (SPICE_UNLIKELY(end == NULL)) {
                goto error;
            }
        }
    }

    assert(end <= data + mem_size);

    *size = end - data;
    *free_message = (message_destructor_t) free;
    return data;

   error:
    if (data != NULL) {
        free(data);
    }
    return NULL;
}

static uint8_t * parse_msg_display_draw_transparent(uint8_t *message_start, uint8_t *message_end, int minor, size_t *size, message_destructor_t *free_message)
{
    SPICE_GNUC_UNUSED uint8_t *pos;
    uint8_t *start = message_start;
    uint8_t *data = NULL;
    size_t mem_size, nw_size;
    uint8_t *in, *end;
    SPICE_GNUC_UNUSED intptr_t ptr_size;
    uint32_t n_ptr=0;
    PointerInfo ptr_info[2];
    size_t base__nw_size, base__extra_size;
    uint32_t rects__saved_size = 0;
    size_t data__extra_size;
    SpiceMsgDisplayDrawTransparent *out;
    uint32_t i;

    { /* base */
        SPICE_GNUC_UNUSED uint8_t *start2 = (start + 0);
        size_t clip__nw_size, clip__extra_size;
        { /* clip */
            SPICE_GNUC_UNUSED uint8_t *start3 = (start2 + 20);
            size_t u__nw_size, u__extra_size;
            uint8_t type__value;
            { /* u */
                uint32_t u__mem_size;
                pos = start3 + 0;
                if (SPICE_UNLIKELY(pos + 1 > message_end)) {
                    goto error;
                }
                type__value = read_uint8(pos);
                if (type__value == SPICE_CLIP_TYPE_RECTS) {
                    SPICE_GNUC_UNUSED uint8_t *start4 = (start3 + 1);
                    size_t rects__nw_size, rects__mem_size;
                    uint32_t rects__nelements;
                    { /* rects */
                        uint32_t num_rects__value;
                        pos = start4 + 0;
                        if (SPICE_UNLIKELY(pos + 4 > message_end)) {
                            goto error;
                        }
                        num_rects__value = read_uint32(pos);
                        rects__nelements = num_rects__value;

                        rects__nw_size = (16) * rects__nelements;
                        rects__mem_size = sizeof(SpiceRect) * rects__nelements;
                    }

                    u__nw_size = 4 + rects__nw_size;
                    u__mem_size = sizeof(SpiceClipRects) + rects__mem_size;
                    rects__saved_size = u__nw_size;
                    u__extra_size = u__mem_size;
                } else {
                    u__nw_size = 0;
                    u__extra_size = 0;
                }

            }

            clip__nw_size = 1 + u__nw_size;
            clip__extra_size = u__extra_size;
        }

        base__nw_size = 20 + clip__nw_size;
        base__extra_size = clip__extra_size;
    }

    { /* data */
        SPICE_GNUC_UNUSED uint8_t *start2 = (start + 0 + base__nw_size);
        size_t src_bitmap__extra_size;
        { /* src_bitmap */
            uint32_t src_bitmap__value;
            pos = (start2 + 0);
            if (SPICE_UNLIKELY(pos + 4 > message_end)) {
                goto error;
            }
            src_bitmap__value = read_uint32(pos);
            ptr_size = validate_SpiceImage(message_start, message_end, src_bitmap__value, minor);
            if (SPICE_UNLIKELY(ptr_size < 0)) {
                goto error;
            }
            src_bitmap__extra_size = ptr_size + /* for alignment */ 3;
        }

        data__extra_size = src_bitmap__extra_size;
    }

    nw_size = 28 + base__nw_size;
    mem_size = sizeof(SpiceMsgDisplayDrawTransparent) + base__extra_size + data__extra_size;

    /* Check if message fits in reported side */
    if (start + nw_size > message_end) {
        return NULL;
    }

    /* Validated extents and calculated size */
    data = (uint8_t *)malloc(mem_size);
    if (SPICE_UNLIKELY(data == NULL)) {
        goto error;
    }
    end = data + sizeof(SpiceMsgDisplayDrawTransparent);
    in = start;

    out = (SpiceMsgDisplayDrawTransparent *)data;

    /* base */ {
        out->base.surface_id = consume_uint32(&in);
        /* box */ {
            out->base.box.top = consume_int32(&in);
            out->base.box.left = consume_int32(&in);
            out->base.box.bottom = consume_int32(&in);
            out->base.box.right = consume_int32(&in);
        }
        /* clip */ {
            out->base.clip.type = consume_uint8(&in);
            if (out->base.clip.type == SPICE_CLIP_TYPE_RECTS) {
                ptr_info[n_ptr].offset = in - start;
                ptr_info[n_ptr].parse = parse_struct_SpiceClipRects;
                ptr_info[n_ptr].dest = (void **)&out->base.clip.rects;
                n_ptr++;
                in += rects__saved_size;
            }
        }
    }
    /* data */ {
        ptr_info[n_ptr].offset = consume_uint32(&in);
        ptr_info[n_ptr].parse = parse_struct_SpiceImage;
        ptr_info[n_ptr].dest = (void **)&out->data.src_bitmap;
        n_ptr++;
        /* src_area */ {
            out->data.src_area.top = consume_int32(&in);
            out->data.src_area.left = consume_int32(&in);
            out->data.src_area.bottom = consume_int32(&in);
            out->data.src_area.right = consume_int32(&in);
        }
        out->data.src_color = consume_uint32(&in);
        out->data.true_color = consume_uint32(&in);
    }

    assert(in <= message_end);

    for (i = 0; i < n_ptr; i++) {
        if (ptr_info[i].offset == 0) {
            *ptr_info[i].dest = NULL;
        } else {
            /* Align to 32 bit */
            end = (uint8_t *)SPICE_ALIGN((size_t)end, 4);
            *ptr_info[i].dest = (void *)end;
            end = ptr_info[i].parse(message_start, message_end, end, &ptr_info[i], minor);
            if (SPICE_UNLIKELY(end == NULL)) {
                goto error;
            }
        }
    }

    assert(end <= data + mem_size);

    *size = end - data;
    *free_message = (message_destructor_t) free;
    return data;

   error:
    if (data != NULL) {
        free(data);
    }
    return NULL;
}

static uint8_t * parse_msg_display_draw_alpha_blend(uint8_t *message_start, uint8_t *message_end, int minor, size_t *size, message_destructor_t *free_message)
{
    SPICE_GNUC_UNUSED uint8_t *pos;
    uint8_t *start = message_start;
    uint8_t *data = NULL;
    size_t mem_size, nw_size;
    uint8_t *in, *end;
    SPICE_GNUC_UNUSED intptr_t ptr_size;
    uint32_t n_ptr=0;
    PointerInfo ptr_info[2];
    size_t base__nw_size, base__extra_size;
    uint32_t rects__saved_size = 0;
    size_t data__extra_size;
    SpiceMsgDisplayDrawAlphaBlend *out;
    uint32_t i;

    { /* base */
        SPICE_GNUC_UNUSED uint8_t *start2 = (start + 0);
        size_t clip__nw_size, clip__extra_size;
        { /* clip */
            SPICE_GNUC_UNUSED uint8_t *start3 = (start2 + 20);
            size_t u__nw_size, u__extra_size;
            uint8_t type__value;
            { /* u */
                uint32_t u__mem_size;
                pos = start3 + 0;
                if (SPICE_UNLIKELY(pos + 1 > message_end)) {
                    goto error;
                }
                type__value = read_uint8(pos);
                if (type__value == SPICE_CLIP_TYPE_RECTS) {
                    SPICE_GNUC_UNUSED uint8_t *start4 = (start3 + 1);
                    size_t rects__nw_size, rects__mem_size;
                    uint32_t rects__nelements;
                    { /* rects */
                        uint32_t num_rects__value;
                        pos = start4 + 0;
                        if (SPICE_UNLIKELY(pos + 4 > message_end)) {
                            goto error;
                        }
                        num_rects__value = read_uint32(pos);
                        rects__nelements = num_rects__value;

                        rects__nw_size = (16) * rects__nelements;
                        rects__mem_size = sizeof(SpiceRect) * rects__nelements;
                    }

                    u__nw_size = 4 + rects__nw_size;
                    u__mem_size = sizeof(SpiceClipRects) + rects__mem_size;
                    rects__saved_size = u__nw_size;
                    u__extra_size = u__mem_size;
                } else {
                    u__nw_size = 0;
                    u__extra_size = 0;
                }

            }

            clip__nw_size = 1 + u__nw_size;
            clip__extra_size = u__extra_size;
        }

        base__nw_size = 20 + clip__nw_size;
        base__extra_size = clip__extra_size;
    }

    { /* data */
        SPICE_GNUC_UNUSED uint8_t *start2 = (start + 0 + base__nw_size);
        size_t src_bitmap__extra_size;
        { /* src_bitmap */
            uint32_t src_bitmap__value;
            pos = (start2 + 2);
            if (SPICE_UNLIKELY(pos + 4 > message_end)) {
                goto error;
            }
            src_bitmap__value = read_uint32(pos);
            ptr_size = validate_SpiceImage(message_start, message_end, src_bitmap__value, minor);
            if (SPICE_UNLIKELY(ptr_size < 0)) {
                goto error;
            }
            src_bitmap__extra_size = ptr_size + /* for alignment */ 3;
        }

        data__extra_size = src_bitmap__extra_size;
    }

    nw_size = 22 + base__nw_size;
    mem_size = sizeof(SpiceMsgDisplayDrawAlphaBlend) + base__extra_size + data__extra_size;

    /* Check if message fits in reported side */
    if (start + nw_size > message_end) {
        return NULL;
    }

    /* Validated extents and calculated size */
    data = (uint8_t *)malloc(mem_size);
    if (SPICE_UNLIKELY(data == NULL)) {
        goto error;
    }
    end = data + sizeof(SpiceMsgDisplayDrawAlphaBlend);
    in = start;

    out = (SpiceMsgDisplayDrawAlphaBlend *)data;

    /* base */ {
        out->base.surface_id = consume_uint32(&in);
        /* box */ {
            out->base.box.top = consume_int32(&in);
            out->base.box.left = consume_int32(&in);
            out->base.box.bottom = consume_int32(&in);
            out->base.box.right = consume_int32(&in);
        }
        /* clip */ {
            out->base.clip.type = consume_uint8(&in);
            if (out->base.clip.type == SPICE_CLIP_TYPE_RECTS) {
                ptr_info[n_ptr].offset = in - start;
                ptr_info[n_ptr].parse = parse_struct_SpiceClipRects;
                ptr_info[n_ptr].dest = (void **)&out->base.clip.rects;
                n_ptr++;
                in += rects__saved_size;
            }
        }
    }
    /* data */ {
        out->data.alpha_flags = consume_uint8(&in);
        out->data.alpha = consume_uint8(&in);
        ptr_info[n_ptr].offset = consume_uint32(&in);
        ptr_info[n_ptr].parse = parse_struct_SpiceImage;
        ptr_info[n_ptr].dest = (void **)&out->data.src_bitmap;
        n_ptr++;
        /* src_area */ {
            out->data.src_area.top = consume_int32(&in);
            out->data.src_area.left = consume_int32(&in);
            out->data.src_area.bottom = consume_int32(&in);
            out->data.src_area.right = consume_int32(&in);
        }
    }

    assert(in <= message_end);

    for (i = 0; i < n_ptr; i++) {
        if (ptr_info[i].offset == 0) {
            *ptr_info[i].dest = NULL;
        } else {
            /* Align to 32 bit */
            end = (uint8_t *)SPICE_ALIGN((size_t)end, 4);
            *ptr_info[i].dest = (void *)end;
            end = ptr_info[i].parse(message_start, message_end, end, &ptr_info[i], minor);
            if (SPICE_UNLIKELY(end == NULL)) {
                goto error;
            }
        }
    }

    assert(end <= data + mem_size);

    *size = end - data;
    *free_message = (message_destructor_t) free;
    return data;

   error:
    if (data != NULL) {
        free(data);
    }
    return NULL;
}

static uint8_t * parse_msg_display_surface_create(uint8_t *message_start, uint8_t *message_end, int minor, size_t *size, message_destructor_t *free_message)
{
    SPICE_GNUC_UNUSED uint8_t *pos;
    uint8_t *start = message_start;
    uint8_t *data = NULL;
    size_t mem_size, nw_size;
    uint8_t *in, *end;
    SpiceMsgSurfaceCreate *out;

    nw_size = 20;
    mem_size = sizeof(SpiceMsgSurfaceCreate);

    /* Check if message fits in reported side */
    if (start + nw_size > message_end) {
        return NULL;
    }

    /* Validated extents and calculated size */
    data = (uint8_t *)malloc(mem_size);
    if (SPICE_UNLIKELY(data == NULL)) {
        goto error;
    }
    end = data + sizeof(SpiceMsgSurfaceCreate);
    in = start;

    out = (SpiceMsgSurfaceCreate *)data;

    out->surface_id = consume_uint32(&in);
    out->width = consume_uint32(&in);
    out->height = consume_uint32(&in);
    out->format = consume_uint32(&in);
    out->flags = consume_uint32(&in);

    assert(in <= message_end);
    assert(end <= data + mem_size);

    *size = end - data;
    *free_message = (message_destructor_t) free;
    return data;

   error:
    if (data != NULL) {
        free(data);
    }
    return NULL;
}

static uint8_t * parse_msg_display_surface_destroy(uint8_t *message_start, uint8_t *message_end, int minor, size_t *size, message_destructor_t *free_message)
{
    SPICE_GNUC_UNUSED uint8_t *pos;
    uint8_t *start = message_start;
    uint8_t *data = NULL;
    size_t mem_size, nw_size;
    uint8_t *in, *end;
    SpiceMsgSurfaceDestroy *out;

    nw_size = 4;
    mem_size = sizeof(SpiceMsgSurfaceDestroy);

    /* Check if message fits in reported side */
    if (start + nw_size > message_end) {
        return NULL;
    }

    /* Validated extents and calculated size */
    data = (uint8_t *)malloc(mem_size);
    if (SPICE_UNLIKELY(data == NULL)) {
        goto error;
    }
    end = data + sizeof(SpiceMsgSurfaceDestroy);
    in = start;

    out = (SpiceMsgSurfaceDestroy *)data;

    out->surface_id = consume_uint32(&in);

    assert(in <= message_end);
    assert(end <= data + mem_size);

    *size = end - data;
    *free_message = (message_destructor_t) free;
    return data;

   error:
    if (data != NULL) {
        free(data);
    }
    return NULL;
}

static uint8_t * parse_DisplayChannel_msg(uint8_t *message_start, uint8_t *message_end, uint16_t message_type, int minor, size_t *size_out, message_destructor_t *free_message)
{
    static parse_msg_func_t funcs1[7] =  {
        parse_msg_migrate,
        parse_SpiceMsgData,
        parse_msg_set_ack,
        parse_msg_ping,
        parse_msg_wait_for_channels,
        parse_msg_disconnecting,
        parse_msg_notify
    };
    static parse_msg_func_t funcs2[8] =  {
        parse_msg_display_mode,
        parse_SpiceMsgEmpty,
        parse_SpiceMsgEmpty,
        parse_msg_display_copy_bits,
        parse_msg_display_inval_list,
        parse_msg_display_inval_all_pixmaps,
        parse_msg_display_inval_palette,
        parse_SpiceMsgEmpty
    };
    static parse_msg_func_t funcs3[5] =  {
        parse_msg_display_stream_create,
        parse_msg_display_stream_data,
        parse_msg_display_stream_clip,
        parse_msg_display_stream_destroy,
        parse_SpiceMsgEmpty
    };
    static parse_msg_func_t funcs4[14] =  {
        parse_msg_display_draw_fill,
        parse_msg_display_draw_opaque,
        parse_msg_display_draw_copy,
        parse_msg_display_draw_blend,
        parse_msg_display_draw_blackness,
        parse_msg_display_draw_whiteness,
        parse_msg_display_draw_invers,
        parse_msg_display_draw_rop3,
        parse_msg_display_draw_stroke,
        parse_msg_display_draw_text,
        parse_msg_display_draw_transparent,
        parse_msg_display_draw_alpha_blend,
        parse_msg_display_surface_create,
        parse_msg_display_surface_destroy
    };
    if (message_type >= 1 && message_type < 8) {
        return funcs1[message_type-1](message_start, message_end, minor, size_out, free_message);
    } else if (message_type >= 101 && message_type < 109) {
        return funcs2[message_type-101](message_start, message_end, minor, size_out, free_message);
    } else if (message_type >= 122 && message_type < 127) {
        return funcs3[message_type-122](message_start, message_end, minor, size_out, free_message);
    } else if (message_type >= 302 && message_type < 316) {
        return funcs4[message_type-302](message_start, message_end, minor, size_out, free_message);
    }
    return NULL;
}



static uint8_t * parse_msg_inputs_init(uint8_t *message_start, uint8_t *message_end, int minor, size_t *size, message_destructor_t *free_message)
{
    SPICE_GNUC_UNUSED uint8_t *pos;
    uint8_t *start = message_start;
    uint8_t *data = NULL;
    size_t mem_size, nw_size;
    uint8_t *in, *end;
    SpiceMsgInputsInit *out;

    nw_size = 2;
    mem_size = sizeof(SpiceMsgInputsInit);

    /* Check if message fits in reported side */
    if (start + nw_size > message_end) {
        return NULL;
    }

    /* Validated extents and calculated size */
    data = (uint8_t *)malloc(mem_size);
    if (SPICE_UNLIKELY(data == NULL)) {
        goto error;
    }
    end = data + sizeof(SpiceMsgInputsInit);
    in = start;

    out = (SpiceMsgInputsInit *)data;

    out->keyboard_modifiers = consume_uint16(&in);

    assert(in <= message_end);
    assert(end <= data + mem_size);

    *size = end - data;
    *free_message = (message_destructor_t) free;
    return data;

   error:
    if (data != NULL) {
        free(data);
    }
    return NULL;
}

static uint8_t * parse_msg_inputs_key_modifiers(uint8_t *message_start, uint8_t *message_end, int minor, size_t *size, message_destructor_t *free_message)
{
    SPICE_GNUC_UNUSED uint8_t *pos;
    uint8_t *start = message_start;
    uint8_t *data = NULL;
    size_t mem_size, nw_size;
    uint8_t *in, *end;
    SpiceMsgInputsKeyModifiers *out;

    nw_size = 2;
    mem_size = sizeof(SpiceMsgInputsKeyModifiers);

    /* Check if message fits in reported side */
    if (start + nw_size > message_end) {
        return NULL;
    }

    /* Validated extents and calculated size */
    data = (uint8_t *)malloc(mem_size);
    if (SPICE_UNLIKELY(data == NULL)) {
        goto error;
    }
    end = data + sizeof(SpiceMsgInputsKeyModifiers);
    in = start;

    out = (SpiceMsgInputsKeyModifiers *)data;

    out->modifiers = consume_uint16(&in);

    assert(in <= message_end);
    assert(end <= data + mem_size);

    *size = end - data;
    *free_message = (message_destructor_t) free;
    return data;

   error:
    if (data != NULL) {
        free(data);
    }
    return NULL;
}

static uint8_t * parse_InputsChannel_msg(uint8_t *message_start, uint8_t *message_end, uint16_t message_type, int minor, size_t *size_out, message_destructor_t *free_message)
{
    static parse_msg_func_t funcs1[7] =  {
        parse_msg_migrate,
        parse_SpiceMsgData,
        parse_msg_set_ack,
        parse_msg_ping,
        parse_msg_wait_for_channels,
        parse_msg_disconnecting,
        parse_msg_notify
    };
    static parse_msg_func_t funcs2[2] =  {
        parse_msg_inputs_init,
        parse_msg_inputs_key_modifiers
    };
    static parse_msg_func_t funcs3[1] =  {
        parse_SpiceMsgEmpty
    };
    if (message_type >= 1 && message_type < 8) {
        return funcs1[message_type-1](message_start, message_end, minor, size_out, free_message);
    } else if (message_type >= 101 && message_type < 103) {
        return funcs2[message_type-101](message_start, message_end, minor, size_out, free_message);
    } else if (message_type >= 111 && message_type < 112) {
        return funcs3[message_type-111](message_start, message_end, minor, size_out, free_message);
    }
    return NULL;
}



static uint8_t * parse_msg_cursor_init(uint8_t *message_start, uint8_t *message_end, int minor, size_t *size, message_destructor_t *free_message)
{
    SPICE_GNUC_UNUSED uint8_t *pos;
    uint8_t *start = message_start;
    uint8_t *data = NULL;
    size_t mem_size, nw_size;
    uint8_t *in, *end;
    size_t cursor__nw_size;
    SpiceMsgCursorInit *out;

    { /* cursor */
        SPICE_GNUC_UNUSED uint8_t *start2 = (start + 9);
        size_t u__nw_size;
        uint16_t flags__value;
        size_t data__nw_size;
        uint32_t data__nelements;
        { /* u */
            pos = start2 + 0;
            if (SPICE_UNLIKELY(pos + 2 > message_end)) {
                goto error;
            }
            flags__value = read_uint16(pos);
            if (!(flags__value & SPICE_CURSOR_FLAGS_NONE)) {
                SPICE_GNUC_UNUSED uint8_t *start3 = (start2 + 2);
                u__nw_size = 17;
            } else {
                u__nw_size = 0;
            }

        }

        { /* data */
            data__nelements = message_end - (start2 + 2 + u__nw_size);

            data__nw_size = data__nelements;
        }

        cursor__nw_size = 2 + u__nw_size + data__nw_size;
    }

    nw_size = 9 + cursor__nw_size;
    mem_size = sizeof(SpiceMsgCursorInit);

    /* Check if message fits in reported side */
    if (start + nw_size > message_end) {
        return NULL;
    }

    /* Validated extents and calculated size */
    data = (uint8_t *)malloc(mem_size);
    if (SPICE_UNLIKELY(data == NULL)) {
        goto error;
    }
    end = data + sizeof(SpiceMsgCursorInit);
    in = start;

    out = (SpiceMsgCursorInit *)data;

    /* position */ {
        out->position.x = consume_int16(&in);
        out->position.y = consume_int16(&in);
    }
    out->trail_length = consume_uint16(&in);
    out->trail_frequency = consume_uint16(&in);
    out->visible = consume_uint8(&in);
    /* cursor */ {
        uint32_t data__nelements;
        out->cursor.flags = consume_uint16(&in);
        if (!(out->cursor.flags & SPICE_CURSOR_FLAGS_NONE)) {
            out->cursor.header.unique = consume_uint64(&in);
            out->cursor.header.type = consume_uint8(&in);
            out->cursor.header.width = consume_uint16(&in);
            out->cursor.header.height = consume_uint16(&in);
            out->cursor.header.hot_spot_x = consume_uint16(&in);
            out->cursor.header.hot_spot_y = consume_uint16(&in);
        }
        data__nelements = (message_end - in) / (1);
        /* use array as pointer */
        out->cursor.data = (uint8_t *)in;
        out->cursor.data_size = data__nelements;
        in += data__nelements;
    }

    assert(in <= message_end);
    assert(end <= data + mem_size);

    *size = end - data;
    *free_message = (message_destructor_t) free;
    return data;

   error:
    if (data != NULL) {
        free(data);
    }
    return NULL;
}

static uint8_t * parse_msg_cursor_set(uint8_t *message_start, uint8_t *message_end, int minor, size_t *size, message_destructor_t *free_message)
{
    SPICE_GNUC_UNUSED uint8_t *pos;
    uint8_t *start = message_start;
    uint8_t *data = NULL;
    size_t mem_size, nw_size;
    uint8_t *in, *end;
    size_t cursor__nw_size;
    SpiceMsgCursorSet *out;

    { /* cursor */
        SPICE_GNUC_UNUSED uint8_t *start2 = (start + 5);
        size_t u__nw_size;
        uint16_t flags__value;
        size_t data__nw_size;
        uint32_t data__nelements;
        { /* u */
            pos = start2 + 0;
            if (SPICE_UNLIKELY(pos + 2 > message_end)) {
                goto error;
            }
            flags__value = read_uint16(pos);
            if (!(flags__value & SPICE_CURSOR_FLAGS_NONE)) {
                SPICE_GNUC_UNUSED uint8_t *start3 = (start2 + 2);
                u__nw_size = 17;
            } else {
                u__nw_size = 0;
            }

        }

        { /* data */
            data__nelements = message_end - (start2 + 2 + u__nw_size);

            data__nw_size = data__nelements;
        }

        cursor__nw_size = 2 + u__nw_size + data__nw_size;
    }

    nw_size = 5 + cursor__nw_size;
    mem_size = sizeof(SpiceMsgCursorSet);

    /* Check if message fits in reported side */
    if (start + nw_size > message_end) {
        return NULL;
    }

    /* Validated extents and calculated size */
    data = (uint8_t *)malloc(mem_size);
    if (SPICE_UNLIKELY(data == NULL)) {
        goto error;
    }
    end = data + sizeof(SpiceMsgCursorSet);
    in = start;

    out = (SpiceMsgCursorSet *)data;

    /* position */ {
        out->position.x = consume_int16(&in);
        out->position.y = consume_int16(&in);
    }
    out->visible = consume_uint8(&in);
    /* cursor */ {
        uint32_t data__nelements;
        out->cursor.flags = consume_uint16(&in);
        if (!(out->cursor.flags & SPICE_CURSOR_FLAGS_NONE)) {
            out->cursor.header.unique = consume_uint64(&in);
            out->cursor.header.type = consume_uint8(&in);
            out->cursor.header.width = consume_uint16(&in);
            out->cursor.header.height = consume_uint16(&in);
            out->cursor.header.hot_spot_x = consume_uint16(&in);
            out->cursor.header.hot_spot_y = consume_uint16(&in);
        }
        data__nelements = (message_end - in) / (1);
        /* use array as pointer */
        out->cursor.data = (uint8_t *)in;
        out->cursor.data_size = data__nelements;
        in += data__nelements;
    }

    assert(in <= message_end);
    assert(end <= data + mem_size);

    *size = end - data;
    *free_message = (message_destructor_t) free;
    return data;

   error:
    if (data != NULL) {
        free(data);
    }
    return NULL;
}

static uint8_t * parse_msg_cursor_move(uint8_t *message_start, uint8_t *message_end, int minor, size_t *size, message_destructor_t *free_message)
{
    SPICE_GNUC_UNUSED uint8_t *pos;
    uint8_t *start = message_start;
    uint8_t *data = NULL;
    size_t mem_size, nw_size;
    uint8_t *in, *end;
    SpiceMsgCursorMove *out;

    nw_size = 4;
    mem_size = sizeof(SpiceMsgCursorMove);

    /* Check if message fits in reported side */
    if (start + nw_size > message_end) {
        return NULL;
    }

    /* Validated extents and calculated size */
    data = (uint8_t *)malloc(mem_size);
    if (SPICE_UNLIKELY(data == NULL)) {
        goto error;
    }
    end = data + sizeof(SpiceMsgCursorMove);
    in = start;

    out = (SpiceMsgCursorMove *)data;

    /* position */ {
        out->position.x = consume_int16(&in);
        out->position.y = consume_int16(&in);
    }

    assert(in <= message_end);
    assert(end <= data + mem_size);

    *size = end - data;
    *free_message = (message_destructor_t) free;
    return data;

   error:
    if (data != NULL) {
        free(data);
    }
    return NULL;
}

static uint8_t * parse_msg_cursor_trail(uint8_t *message_start, uint8_t *message_end, int minor, size_t *size, message_destructor_t *free_message)
{
    SPICE_GNUC_UNUSED uint8_t *pos;
    uint8_t *start = message_start;
    uint8_t *data = NULL;
    size_t mem_size, nw_size;
    uint8_t *in, *end;
    SpiceMsgCursorTrail *out;

    nw_size = 4;
    mem_size = sizeof(SpiceMsgCursorTrail);

    /* Check if message fits in reported side */
    if (start + nw_size > message_end) {
        return NULL;
    }

    /* Validated extents and calculated size */
    data = (uint8_t *)malloc(mem_size);
    if (SPICE_UNLIKELY(data == NULL)) {
        goto error;
    }
    end = data + sizeof(SpiceMsgCursorTrail);
    in = start;

    out = (SpiceMsgCursorTrail *)data;

    out->length = consume_uint16(&in);
    out->frequency = consume_uint16(&in);

    assert(in <= message_end);
    assert(end <= data + mem_size);

    *size = end - data;
    *free_message = (message_destructor_t) free;
    return data;

   error:
    if (data != NULL) {
        free(data);
    }
    return NULL;
}

static uint8_t * parse_msg_cursor_inval_one(uint8_t *message_start, uint8_t *message_end, int minor, size_t *size, message_destructor_t *free_message)
{
    SPICE_GNUC_UNUSED uint8_t *pos;
    uint8_t *start = message_start;
    uint8_t *data = NULL;
    size_t mem_size, nw_size;
    uint8_t *in, *end;
    SpiceMsgDisplayInvalOne *out;

    nw_size = 8;
    mem_size = sizeof(SpiceMsgDisplayInvalOne);

    /* Check if message fits in reported side */
    if (start + nw_size > message_end) {
        return NULL;
    }

    /* Validated extents and calculated size */
    data = (uint8_t *)malloc(mem_size);
    if (SPICE_UNLIKELY(data == NULL)) {
        goto error;
    }
    end = data + sizeof(SpiceMsgDisplayInvalOne);
    in = start;

    out = (SpiceMsgDisplayInvalOne *)data;

    out->id = consume_uint64(&in);

    assert(in <= message_end);
    assert(end <= data + mem_size);

    *size = end - data;
    *free_message = (message_destructor_t) free;
    return data;

   error:
    if (data != NULL) {
        free(data);
    }
    return NULL;
}

static uint8_t * parse_CursorChannel_msg(uint8_t *message_start, uint8_t *message_end, uint16_t message_type, int minor, size_t *size_out, message_destructor_t *free_message)
{
    static parse_msg_func_t funcs1[7] =  {
        parse_msg_migrate,
        parse_SpiceMsgData,
        parse_msg_set_ack,
        parse_msg_ping,
        parse_msg_wait_for_channels,
        parse_msg_disconnecting,
        parse_msg_notify
    };
    static parse_msg_func_t funcs2[8] =  {
        parse_msg_cursor_init,
        parse_SpiceMsgEmpty,
        parse_msg_cursor_set,
        parse_msg_cursor_move,
        parse_SpiceMsgEmpty,
        parse_msg_cursor_trail,
        parse_msg_cursor_inval_one,
        parse_SpiceMsgEmpty
    };
    if (message_type >= 1 && message_type < 8) {
        return funcs1[message_type-1](message_start, message_end, minor, size_out, free_message);
    } else if (message_type >= 101 && message_type < 109) {
        return funcs2[message_type-101](message_start, message_end, minor, size_out, free_message);
    }
    return NULL;
}



static uint8_t * parse_msg_playback_data(uint8_t *message_start, uint8_t *message_end, int minor, size_t *size, message_destructor_t *free_message)
{
    SPICE_GNUC_UNUSED uint8_t *pos;
    uint8_t *start = message_start;
    uint8_t *data = NULL;
    size_t mem_size, nw_size;
    uint8_t *in, *end;
    size_t data__nw_size;
    uint32_t data__nelements;
    SpiceMsgPlaybackPacket *out;

    { /* data */
        data__nelements = message_end - (start + 4);

        data__nw_size = data__nelements;
    }

    nw_size = 4 + data__nw_size;
    mem_size = sizeof(SpiceMsgPlaybackPacket);

    /* Check if message fits in reported side */
    if (start + nw_size > message_end) {
        return NULL;
    }

    /* Validated extents and calculated size */
    data = (uint8_t *)malloc(mem_size);
    if (SPICE_UNLIKELY(data == NULL)) {
        goto error;
    }
    end = data + sizeof(SpiceMsgPlaybackPacket);
    in = start;

    out = (SpiceMsgPlaybackPacket *)data;

    out->time = consume_uint32(&in);
    /* use array as pointer */
    out->data = (uint8_t *)in;
    out->data_size = data__nelements;
    in += data__nelements;

    assert(in <= message_end);
    assert(end <= data + mem_size);

    *size = end - data;
    *free_message = (message_destructor_t) free;
    return data;

   error:
    if (data != NULL) {
        free(data);
    }
    return NULL;
}

static uint8_t * parse_msg_playback_mode(uint8_t *message_start, uint8_t *message_end, int minor, size_t *size, message_destructor_t *free_message)
{
    SPICE_GNUC_UNUSED uint8_t *pos;
    uint8_t *start = message_start;
    uint8_t *data = NULL;
    size_t mem_size, nw_size;
    uint8_t *in, *end;
    size_t data__nw_size;
    uint32_t data__nelements;
    SpiceMsgPlaybackMode *out;

    { /* data */
        data__nelements = message_end - (start + 6);

        data__nw_size = data__nelements;
    }

    nw_size = 6 + data__nw_size;
    mem_size = sizeof(SpiceMsgPlaybackMode);

    /* Check if message fits in reported side */
    if (start + nw_size > message_end) {
        return NULL;
    }

    /* Validated extents and calculated size */
    data = (uint8_t *)malloc(mem_size);
    if (SPICE_UNLIKELY(data == NULL)) {
        goto error;
    }
    end = data + sizeof(SpiceMsgPlaybackMode);
    in = start;

    out = (SpiceMsgPlaybackMode *)data;

    out->time = consume_uint32(&in);
    out->mode = consume_uint16(&in);
    /* use array as pointer */
    out->data = (uint8_t *)in;
    out->data_size = data__nelements;
    in += data__nelements;

    assert(in <= message_end);
    assert(end <= data + mem_size);

    *size = end - data;
    *free_message = (message_destructor_t) free;
    return data;

   error:
    if (data != NULL) {
        free(data);
    }
    return NULL;
}

static uint8_t * parse_msg_playback_start(uint8_t *message_start, uint8_t *message_end, int minor, size_t *size, message_destructor_t *free_message)
{
    SPICE_GNUC_UNUSED uint8_t *pos;
    uint8_t *start = message_start;
    uint8_t *data = NULL;
    size_t mem_size, nw_size;
    uint8_t *in, *end;
    SpiceMsgPlaybackStart *out;

    nw_size = 14;
    mem_size = sizeof(SpiceMsgPlaybackStart);

    /* Check if message fits in reported side */
    if (start + nw_size > message_end) {
        return NULL;
    }

    /* Validated extents and calculated size */
    data = (uint8_t *)malloc(mem_size);
    if (SPICE_UNLIKELY(data == NULL)) {
        goto error;
    }
    end = data + sizeof(SpiceMsgPlaybackStart);
    in = start;

    out = (SpiceMsgPlaybackStart *)data;

    out->channels = consume_uint32(&in);
    out->format = consume_uint16(&in);
    out->frequency = consume_uint32(&in);
    out->time = consume_uint32(&in);

    assert(in <= message_end);
    assert(end <= data + mem_size);

    *size = end - data;
    *free_message = (message_destructor_t) free;
    return data;

   error:
    if (data != NULL) {
        free(data);
    }
    return NULL;
}

static uint8_t * parse_PlaybackChannel_msg(uint8_t *message_start, uint8_t *message_end, uint16_t message_type, int minor, size_t *size_out, message_destructor_t *free_message)
{
    static parse_msg_func_t funcs1[7] =  {
        parse_msg_migrate,
        parse_SpiceMsgData,
        parse_msg_set_ack,
        parse_msg_ping,
        parse_msg_wait_for_channels,
        parse_msg_disconnecting,
        parse_msg_notify
    };
    static parse_msg_func_t funcs2[4] =  {
        parse_msg_playback_data,
        parse_msg_playback_mode,
        parse_msg_playback_start,
        parse_SpiceMsgEmpty
    };
    if (message_type >= 1 && message_type < 8) {
        return funcs1[message_type-1](message_start, message_end, minor, size_out, free_message);
    } else if (message_type >= 101 && message_type < 105) {
        return funcs2[message_type-101](message_start, message_end, minor, size_out, free_message);
    }
    return NULL;
}



static uint8_t * parse_msg_record_start(uint8_t *message_start, uint8_t *message_end, int minor, size_t *size, message_destructor_t *free_message)
{
    SPICE_GNUC_UNUSED uint8_t *pos;
    uint8_t *start = message_start;
    uint8_t *data = NULL;
    size_t mem_size, nw_size;
    uint8_t *in, *end;
    SpiceMsgRecordStart *out;

    nw_size = 10;
    mem_size = sizeof(SpiceMsgRecordStart);

    /* Check if message fits in reported side */
    if (start + nw_size > message_end) {
        return NULL;
    }

    /* Validated extents and calculated size */
    data = (uint8_t *)malloc(mem_size);
    if (SPICE_UNLIKELY(data == NULL)) {
        goto error;
    }
    end = data + sizeof(SpiceMsgRecordStart);
    in = start;

    out = (SpiceMsgRecordStart *)data;

    out->channels = consume_uint32(&in);
    out->format = consume_uint16(&in);
    out->frequency = consume_uint32(&in);

    assert(in <= message_end);
    assert(end <= data + mem_size);

    *size = end - data;
    *free_message = (message_destructor_t) free;
    return data;

   error:
    if (data != NULL) {
        free(data);
    }
    return NULL;
}

static uint8_t * parse_RecordChannel_msg(uint8_t *message_start, uint8_t *message_end, uint16_t message_type, int minor, size_t *size_out, message_destructor_t *free_message)
{
    static parse_msg_func_t funcs1[7] =  {
        parse_msg_migrate,
        parse_SpiceMsgData,
        parse_msg_set_ack,
        parse_msg_ping,
        parse_msg_wait_for_channels,
        parse_msg_disconnecting,
        parse_msg_notify
    };
    static parse_msg_func_t funcs2[2] =  {
        parse_msg_record_start,
        parse_SpiceMsgEmpty
    };
    if (message_type >= 1 && message_type < 8) {
        return funcs1[message_type-1](message_start, message_end, minor, size_out, free_message);
    } else if (message_type >= 101 && message_type < 103) {
        return funcs2[message_type-101](message_start, message_end, minor, size_out, free_message);
    }
    return NULL;
}



static uint8_t * parse_msg_tunnel_init(uint8_t *message_start, uint8_t *message_end, int minor, size_t *size, message_destructor_t *free_message)
{
    SPICE_GNUC_UNUSED uint8_t *pos;
    uint8_t *start = message_start;
    uint8_t *data = NULL;
    size_t mem_size, nw_size;
    uint8_t *in, *end;
    SpiceMsgTunnelInit *out;

    nw_size = 6;
    mem_size = sizeof(SpiceMsgTunnelInit);

    /* Check if message fits in reported side */
    if (start + nw_size > message_end) {
        return NULL;
    }

    /* Validated extents and calculated size */
    data = (uint8_t *)malloc(mem_size);
    if (SPICE_UNLIKELY(data == NULL)) {
        goto error;
    }
    end = data + sizeof(SpiceMsgTunnelInit);
    in = start;

    out = (SpiceMsgTunnelInit *)data;

    out->max_num_of_sockets = consume_uint16(&in);
    out->max_socket_data_size = consume_uint32(&in);

    assert(in <= message_end);
    assert(end <= data + mem_size);

    *size = end - data;
    *free_message = (message_destructor_t) free;
    return data;

   error:
    if (data != NULL) {
        free(data);
    }
    return NULL;
}

static uint8_t * parse_msg_tunnel_service_ip_map(uint8_t *message_start, uint8_t *message_end, int minor, size_t *size, message_destructor_t *free_message)
{
    SPICE_GNUC_UNUSED uint8_t *pos;
    uint8_t *start = message_start;
    uint8_t *data = NULL;
    size_t mem_size, nw_size;
    uint8_t *in, *end;
    size_t virtual_ip__nw_size;
    SpiceMsgTunnelServiceIpMap *out;

    { /* virtual_ip */
        SPICE_GNUC_UNUSED uint8_t *start2 = (start + 4);
        size_t u__nw_size;
        uint16_t type__value;
        { /* u */
            uint32_t u__nelements;
            pos = start2 + 0;
            if (SPICE_UNLIKELY(pos + 2 > message_end)) {
                goto error;
            }
            type__value = read_uint16(pos);
            if (type__value == SPICE_TUNNEL_IP_TYPE_IPv4) {
                u__nelements = 4;

                u__nw_size = u__nelements;
            } else {
                u__nw_size = 0;
            }

        }

        virtual_ip__nw_size = 2 + u__nw_size;
    }

    nw_size = 4 + virtual_ip__nw_size;
    mem_size = sizeof(SpiceMsgTunnelServiceIpMap);

    /* Check if message fits in reported side */
    if (start + nw_size > message_end) {
        return NULL;
    }

    /* Validated extents and calculated size */
    data = (uint8_t *)malloc(mem_size);
    if (SPICE_UNLIKELY(data == NULL)) {
        goto error;
    }
    end = data + sizeof(SpiceMsgTunnelServiceIpMap);
    in = start;

    out = (SpiceMsgTunnelServiceIpMap *)data;

    out->service_id = consume_uint32(&in);
    /* virtual_ip */ {
        out->virtual_ip.type = consume_uint16(&in);
        if (out->virtual_ip.type == SPICE_TUNNEL_IP_TYPE_IPv4) {
            uint32_t ipv4__nelements;
            ipv4__nelements = 4;
            memcpy(out->virtual_ip.u.ipv4, in, ipv4__nelements);
            in += ipv4__nelements;
        }
    }

    assert(in <= message_end);
    assert(end <= data + mem_size);

    *size = end - data;
    *free_message = (message_destructor_t) free;
    return data;

   error:
    if (data != NULL) {
        free(data);
    }
    return NULL;
}

static uint8_t * parse_msg_tunnel_socket_open(uint8_t *message_start, uint8_t *message_end, int minor, size_t *size, message_destructor_t *free_message)
{
    SPICE_GNUC_UNUSED uint8_t *pos;
    uint8_t *start = message_start;
    uint8_t *data = NULL;
    size_t mem_size, nw_size;
    uint8_t *in, *end;
    SpiceMsgTunnelSocketOpen *out;

    nw_size = 10;
    mem_size = sizeof(SpiceMsgTunnelSocketOpen);

    /* Check if message fits in reported side */
    if (start + nw_size > message_end) {
        return NULL;
    }

    /* Validated extents and calculated size */
    data = (uint8_t *)malloc(mem_size);
    if (SPICE_UNLIKELY(data == NULL)) {
        goto error;
    }
    end = data + sizeof(SpiceMsgTunnelSocketOpen);
    in = start;

    out = (SpiceMsgTunnelSocketOpen *)data;

    out->connection_id = consume_uint16(&in);
    out->service_id = consume_uint32(&in);
    out->tokens = consume_uint32(&in);

    assert(in <= message_end);
    assert(end <= data + mem_size);

    *size = end - data;
    *free_message = (message_destructor_t) free;
    return data;

   error:
    if (data != NULL) {
        free(data);
    }
    return NULL;
}

static uint8_t * parse_msg_tunnel_socket_fin(uint8_t *message_start, uint8_t *message_end, int minor, size_t *size, message_destructor_t *free_message)
{
    SPICE_GNUC_UNUSED uint8_t *pos;
    uint8_t *start = message_start;
    uint8_t *data = NULL;
    size_t mem_size, nw_size;
    uint8_t *in, *end;
    SpiceMsgTunnelSocketFin *out;

    nw_size = 2;
    mem_size = sizeof(SpiceMsgTunnelSocketFin);

    /* Check if message fits in reported side */
    if (start + nw_size > message_end) {
        return NULL;
    }

    /* Validated extents and calculated size */
    data = (uint8_t *)malloc(mem_size);
    if (SPICE_UNLIKELY(data == NULL)) {
        goto error;
    }
    end = data + sizeof(SpiceMsgTunnelSocketFin);
    in = start;

    out = (SpiceMsgTunnelSocketFin *)data;

    out->connection_id = consume_uint16(&in);

    assert(in <= message_end);
    assert(end <= data + mem_size);

    *size = end - data;
    *free_message = (message_destructor_t) free;
    return data;

   error:
    if (data != NULL) {
        free(data);
    }
    return NULL;
}

static uint8_t * parse_msg_tunnel_socket_close(uint8_t *message_start, uint8_t *message_end, int minor, size_t *size, message_destructor_t *free_message)
{
    SPICE_GNUC_UNUSED uint8_t *pos;
    uint8_t *start = message_start;
    uint8_t *data = NULL;
    size_t mem_size, nw_size;
    uint8_t *in, *end;
    SpiceMsgTunnelSocketClose *out;

    nw_size = 2;
    mem_size = sizeof(SpiceMsgTunnelSocketClose);

    /* Check if message fits in reported side */
    if (start + nw_size > message_end) {
        return NULL;
    }

    /* Validated extents and calculated size */
    data = (uint8_t *)malloc(mem_size);
    if (SPICE_UNLIKELY(data == NULL)) {
        goto error;
    }
    end = data + sizeof(SpiceMsgTunnelSocketClose);
    in = start;

    out = (SpiceMsgTunnelSocketClose *)data;

    out->connection_id = consume_uint16(&in);

    assert(in <= message_end);
    assert(end <= data + mem_size);

    *size = end - data;
    *free_message = (message_destructor_t) free;
    return data;

   error:
    if (data != NULL) {
        free(data);
    }
    return NULL;
}

static uint8_t * parse_msg_tunnel_socket_data(uint8_t *message_start, uint8_t *message_end, int minor, size_t *size, message_destructor_t *free_message)
{
    SPICE_GNUC_UNUSED uint8_t *pos;
    uint8_t *start = message_start;
    uint8_t *data = NULL;
    size_t mem_size, nw_size;
    uint8_t *in, *end;
    size_t data__nw_size, data__mem_size;
    uint32_t data__nelements;
    SpiceMsgTunnelSocketData *out;

    { /* data */
        data__nelements = message_end - (start + 2);

        data__nw_size = data__nelements;
        data__mem_size = sizeof(uint8_t) * data__nelements;
    }

    nw_size = 2 + data__nw_size;
    mem_size = sizeof(SpiceMsgTunnelSocketData) + data__mem_size;

    /* Check if message fits in reported side */
    if (start + nw_size > message_end) {
        return NULL;
    }

    /* Validated extents and calculated size */
    data = (uint8_t *)malloc(mem_size);
    if (SPICE_UNLIKELY(data == NULL)) {
        goto error;
    }
    end = data + sizeof(SpiceMsgTunnelSocketData);
    in = start;

    out = (SpiceMsgTunnelSocketData *)data;

    out->connection_id = consume_uint16(&in);
    memcpy(out->data, in, data__nelements);
    in += data__nelements;
    end += data__nelements;

    assert(in <= message_end);
    assert(end <= data + mem_size);

    *size = end - data;
    *free_message = (message_destructor_t) free;
    return data;

   error:
    if (data != NULL) {
        free(data);
    }
    return NULL;
}

static uint8_t * parse_msg_tunnel_socket_closed_ack(uint8_t *message_start, uint8_t *message_end, int minor, size_t *size, message_destructor_t *free_message)
{
    SPICE_GNUC_UNUSED uint8_t *pos;
    uint8_t *start = message_start;
    uint8_t *data = NULL;
    size_t mem_size, nw_size;
    uint8_t *in, *end;
    SpiceMsgTunnelSocketClosedAck *out;

    nw_size = 2;
    mem_size = sizeof(SpiceMsgTunnelSocketClosedAck);

    /* Check if message fits in reported side */
    if (start + nw_size > message_end) {
        return NULL;
    }

    /* Validated extents and calculated size */
    data = (uint8_t *)malloc(mem_size);
    if (SPICE_UNLIKELY(data == NULL)) {
        goto error;
    }
    end = data + sizeof(SpiceMsgTunnelSocketClosedAck);
    in = start;

    out = (SpiceMsgTunnelSocketClosedAck *)data;

    out->connection_id = consume_uint16(&in);

    assert(in <= message_end);
    assert(end <= data + mem_size);

    *size = end - data;
    *free_message = (message_destructor_t) free;
    return data;

   error:
    if (data != NULL) {
        free(data);
    }
    return NULL;
}

static uint8_t * parse_msg_tunnel_socket_token(uint8_t *message_start, uint8_t *message_end, int minor, size_t *size, message_destructor_t *free_message)
{
    SPICE_GNUC_UNUSED uint8_t *pos;
    uint8_t *start = message_start;
    uint8_t *data = NULL;
    size_t mem_size, nw_size;
    uint8_t *in, *end;
    SpiceMsgTunnelSocketTokens *out;

    nw_size = 6;
    mem_size = sizeof(SpiceMsgTunnelSocketTokens);

    /* Check if message fits in reported side */
    if (start + nw_size > message_end) {
        return NULL;
    }

    /* Validated extents and calculated size */
    data = (uint8_t *)malloc(mem_size);
    if (SPICE_UNLIKELY(data == NULL)) {
        goto error;
    }
    end = data + sizeof(SpiceMsgTunnelSocketTokens);
    in = start;

    out = (SpiceMsgTunnelSocketTokens *)data;

    out->connection_id = consume_uint16(&in);
    out->num_tokens = consume_uint32(&in);

    assert(in <= message_end);
    assert(end <= data + mem_size);

    *size = end - data;
    *free_message = (message_destructor_t) free;
    return data;

   error:
    if (data != NULL) {
        free(data);
    }
    return NULL;
}

static uint8_t * parse_TunnelChannel_msg(uint8_t *message_start, uint8_t *message_end, uint16_t message_type, int minor, size_t *size_out, message_destructor_t *free_message)
{
    static parse_msg_func_t funcs1[7] =  {
        parse_msg_migrate,
        parse_SpiceMsgData,
        parse_msg_set_ack,
        parse_msg_ping,
        parse_msg_wait_for_channels,
        parse_msg_disconnecting,
        parse_msg_notify
    };
    static parse_msg_func_t funcs2[8] =  {
        parse_msg_tunnel_init,
        parse_msg_tunnel_service_ip_map,
        parse_msg_tunnel_socket_open,
        parse_msg_tunnel_socket_fin,
        parse_msg_tunnel_socket_close,
        parse_msg_tunnel_socket_data,
        parse_msg_tunnel_socket_closed_ack,
        parse_msg_tunnel_socket_token
    };
    if (message_type >= 1 && message_type < 8) {
        return funcs1[message_type-1](message_start, message_end, minor, size_out, free_message);
    } else if (message_type >= 101 && message_type < 109) {
        return funcs2[message_type-101](message_start, message_end, minor, size_out, free_message);
    }
    return NULL;
}



static uint8_t * parse_SmartcardChannel_msg(uint8_t *message_start, uint8_t *message_end, uint16_t message_type, int minor, size_t *size_out, message_destructor_t *free_message)
{
    static parse_msg_func_t funcs1[7] =  {
        parse_msg_migrate,
        parse_SpiceMsgData,
        parse_msg_set_ack,
        parse_msg_ping,
        parse_msg_wait_for_channels,
        parse_msg_disconnecting,
        parse_msg_notify
    };
    static parse_msg_func_t funcs2[1] =  {
        parse_SpiceMsgData
    };
    if (message_type >= 1 && message_type < 8) {
        return funcs1[message_type-1](message_start, message_end, minor, size_out, free_message);
    } else if (message_type >= 101 && message_type < 102) {
        return funcs2[message_type-101](message_start, message_end, minor, size_out, free_message);
    }
    return NULL;
}

spice_parse_channel_func_t spice_get_server_channel_parser(uint32_t channel, unsigned int *max_message_type)
{
    static struct {spice_parse_channel_func_t func; unsigned int max_messages; } channels[9] =  {
        { NULL, 0},
        { parse_MainChannel_msg, 111},
        { parse_DisplayChannel_msg, 315},
        { parse_InputsChannel_msg, 111},
        { parse_CursorChannel_msg, 108},
        { parse_PlaybackChannel_msg, 104},
        { parse_RecordChannel_msg, 102},
        { parse_TunnelChannel_msg, 108},
        { parse_SmartcardChannel_msg, 101}
    };
    if (channel < 9) {
        if (max_message_type != NULL) {
            *max_message_type = channels[channel].max_messages;
        }
        return channels[channel].func;
    }
    return NULL;
}

uint8_t * spice_parse_msg(uint8_t *message_start, uint8_t *message_end, uint32_t channel, uint16_t message_type, int minor, size_t *size_out, message_destructor_t *free_message)
{
    spice_parse_channel_func_t func;
    func = spice_get_server_channel_parser(channel, NULL);
    if (func != NULL) {
        return func(message_start, message_end, message_type, minor, size_out, free_message);
    }
    return NULL;
}

#include "red_common.h"
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

static uint8_t * parse_msgc_ack_sync(uint8_t *message_start, uint8_t *message_end, int minor, size_t *size, message_destructor_t *free_message)
{
    SPICE_GNUC_UNUSED uint8_t *pos;
    uint8_t *start = message_start;
    uint8_t *data = NULL;
    size_t mem_size, nw_size;
    uint8_t *in, *end;
    SpiceMsgcAckSync *out;

    nw_size = 4;
    mem_size = sizeof(SpiceMsgcAckSync);

    /* Check if message fits in reported side */
    if (start + nw_size > message_end) {
        return NULL;
    }

    /* Validated extents and calculated size */
    data = (uint8_t *)malloc(mem_size);
    if (SPICE_UNLIKELY(data == NULL)) {
        goto error;
    }
    end = data + sizeof(SpiceMsgcAckSync);
    in = start;

    out = (SpiceMsgcAckSync *)data;

    out->generation = consume_uint32(&in);

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

static uint8_t * parse_msgc_pong(uint8_t *message_start, uint8_t *message_end, int minor, size_t *size, message_destructor_t *free_message)
{
    SPICE_GNUC_UNUSED uint8_t *pos;
    uint8_t *start = message_start;
    uint8_t *data = NULL;
    size_t mem_size, nw_size;
    uint8_t *in, *end;
    SpiceMsgPing *out;

    nw_size = 12;
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

static uint8_t * parse_msgc_disconnecting(uint8_t *message_start, uint8_t *message_end, int minor, size_t *size, message_destructor_t *free_message)
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

static uint8_t * parse_msgc_main_client_info(uint8_t *message_start, uint8_t *message_end, int minor, size_t *size, message_destructor_t *free_message)
{
    SPICE_GNUC_UNUSED uint8_t *pos;
    uint8_t *start = message_start;
    uint8_t *data = NULL;
    size_t mem_size, nw_size;
    uint8_t *in, *end;
    SpiceMsgcClientInfo *out;

    nw_size = 8;
    mem_size = sizeof(SpiceMsgcClientInfo);

    /* Check if message fits in reported side */
    if (start + nw_size > message_end) {
        return NULL;
    }

    /* Validated extents and calculated size */
    data = (uint8_t *)malloc(mem_size);
    if (SPICE_UNLIKELY(data == NULL)) {
        goto error;
    }
    end = data + sizeof(SpiceMsgcClientInfo);
    in = start;

    out = (SpiceMsgcClientInfo *)data;

    out->cache_size = consume_uint64(&in);

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

static uint8_t * parse_msgc_main_mouse_mode_request(uint8_t *message_start, uint8_t *message_end, int minor, size_t *size, message_destructor_t *free_message)
{
    SPICE_GNUC_UNUSED uint8_t *pos;
    uint8_t *start = message_start;
    uint8_t *data = NULL;
    size_t mem_size, nw_size;
    uint8_t *in, *end;
    SpiceMsgcMainMouseModeRequest *out;

    nw_size = 2;
    mem_size = sizeof(SpiceMsgcMainMouseModeRequest);

    /* Check if message fits in reported side */
    if (start + nw_size > message_end) {
        return NULL;
    }

    /* Validated extents and calculated size */
    data = (uint8_t *)malloc(mem_size);
    if (SPICE_UNLIKELY(data == NULL)) {
        goto error;
    }
    end = data + sizeof(SpiceMsgcMainMouseModeRequest);
    in = start;

    out = (SpiceMsgcMainMouseModeRequest *)data;

    out->mode = consume_uint16(&in);

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

static uint8_t * parse_msgc_main_agent_start(uint8_t *message_start, uint8_t *message_end, int minor, size_t *size, message_destructor_t *free_message)
{
    SPICE_GNUC_UNUSED uint8_t *pos;
    uint8_t *start = message_start;
    uint8_t *data = NULL;
    size_t mem_size, nw_size;
    uint8_t *in, *end;
    SpiceMsgcMainAgentStart *out;

    nw_size = 4;
    mem_size = sizeof(SpiceMsgcMainAgentStart);

    /* Check if message fits in reported side */
    if (start + nw_size > message_end) {
        return NULL;
    }

    /* Validated extents and calculated size */
    data = (uint8_t *)malloc(mem_size);
    if (SPICE_UNLIKELY(data == NULL)) {
        goto error;
    }
    end = data + sizeof(SpiceMsgcMainAgentStart);
    in = start;

    out = (SpiceMsgcMainAgentStart *)data;

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

static uint8_t * parse_msgc_main_agent_token(uint8_t *message_start, uint8_t *message_end, int minor, size_t *size, message_destructor_t *free_message)
{
    SPICE_GNUC_UNUSED uint8_t *pos;
    uint8_t *start = message_start;
    uint8_t *data = NULL;
    size_t mem_size, nw_size;
    uint8_t *in, *end;
    SpiceMsgcMainAgentTokens *out;

    nw_size = 4;
    mem_size = sizeof(SpiceMsgcMainAgentTokens);

    /* Check if message fits in reported side */
    if (start + nw_size > message_end) {
        return NULL;
    }

    /* Validated extents and calculated size */
    data = (uint8_t *)malloc(mem_size);
    if (SPICE_UNLIKELY(data == NULL)) {
        goto error;
    }
    end = data + sizeof(SpiceMsgcMainAgentTokens);
    in = start;

    out = (SpiceMsgcMainAgentTokens *)data;

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

static uint8_t * parse_MainChannel_msgc(uint8_t *message_start, uint8_t *message_end, uint16_t message_type, int minor, size_t *size_out, message_destructor_t *free_message)
{
    static parse_msg_func_t funcs1[6] =  {
        parse_msgc_ack_sync,
        parse_SpiceMsgEmpty,
        parse_msgc_pong,
        parse_SpiceMsgEmpty,
        parse_SpiceMsgData,
        parse_msgc_disconnecting
    };
    static parse_msg_func_t funcs2[8] =  {
        parse_msgc_main_client_info,
        parse_SpiceMsgEmpty,
        parse_SpiceMsgEmpty,
        parse_SpiceMsgEmpty,
        parse_msgc_main_mouse_mode_request,
        parse_msgc_main_agent_start,
        parse_SpiceMsgData,
        parse_msgc_main_agent_token
    };
    if (message_type >= 1 && message_type < 7) {
        return funcs1[message_type-1](message_start, message_end, minor, size_out, free_message);
    } else if (message_type >= 101 && message_type < 109) {
        return funcs2[message_type-101](message_start, message_end, minor, size_out, free_message);
    }
    return NULL;
}



static uint8_t * parse_msgc_display_init(uint8_t *message_start, uint8_t *message_end, int minor, size_t *size, message_destructor_t *free_message)
{
    SPICE_GNUC_UNUSED uint8_t *pos;
    uint8_t *start = message_start;
    uint8_t *data = NULL;
    size_t mem_size, nw_size;
    uint8_t *in, *end;
    SpiceMsgcDisplayInit *out;

    nw_size = 14;
    mem_size = sizeof(SpiceMsgcDisplayInit);

    /* Check if message fits in reported side */
    if (start + nw_size > message_end) {
        return NULL;
    }

    /* Validated extents and calculated size */
    data = (uint8_t *)malloc(mem_size);
    if (SPICE_UNLIKELY(data == NULL)) {
        goto error;
    }
    end = data + sizeof(SpiceMsgcDisplayInit);
    in = start;

    out = (SpiceMsgcDisplayInit *)data;

    out->pixmap_cache_id = consume_uint8(&in);
    out->pixmap_cache_size = consume_int64(&in);
    out->glz_dictionary_id = consume_uint8(&in);
    out->glz_dictionary_window_size = consume_int32(&in);

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

static uint8_t * parse_DisplayChannel_msgc(uint8_t *message_start, uint8_t *message_end, uint16_t message_type, int minor, size_t *size_out, message_destructor_t *free_message)
{
    static parse_msg_func_t funcs1[6] =  {
        parse_msgc_ack_sync,
        parse_SpiceMsgEmpty,
        parse_msgc_pong,
        parse_SpiceMsgEmpty,
        parse_SpiceMsgData,
        parse_msgc_disconnecting
    };
    static parse_msg_func_t funcs2[1] =  {
        parse_msgc_display_init
    };
    if (message_type >= 1 && message_type < 7) {
        return funcs1[message_type-1](message_start, message_end, minor, size_out, free_message);
    } else if (message_type >= 101 && message_type < 102) {
        return funcs2[message_type-101](message_start, message_end, minor, size_out, free_message);
    }
    return NULL;
}



static uint8_t * parse_msgc_inputs_key_down(uint8_t *message_start, uint8_t *message_end, int minor, size_t *size, message_destructor_t *free_message)
{
    SPICE_GNUC_UNUSED uint8_t *pos;
    uint8_t *start = message_start;
    uint8_t *data = NULL;
    size_t mem_size, nw_size;
    uint8_t *in, *end;
    SpiceMsgcKeyDown *out;

    nw_size = 4;
    mem_size = sizeof(SpiceMsgcKeyDown);

    /* Check if message fits in reported side */
    if (start + nw_size > message_end) {
        return NULL;
    }

    /* Validated extents and calculated size */
    data = (uint8_t *)malloc(mem_size);
    if (SPICE_UNLIKELY(data == NULL)) {
        goto error;
    }
    end = data + sizeof(SpiceMsgcKeyDown);
    in = start;

    out = (SpiceMsgcKeyDown *)data;

    out->code = consume_uint32(&in);

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

static uint8_t * parse_msgc_inputs_key_up(uint8_t *message_start, uint8_t *message_end, int minor, size_t *size, message_destructor_t *free_message)
{
    SPICE_GNUC_UNUSED uint8_t *pos;
    uint8_t *start = message_start;
    uint8_t *data = NULL;
    size_t mem_size, nw_size;
    uint8_t *in, *end;
    SpiceMsgcKeyUp *out;

    nw_size = 4;
    mem_size = sizeof(SpiceMsgcKeyUp);

    /* Check if message fits in reported side */
    if (start + nw_size > message_end) {
        return NULL;
    }

    /* Validated extents and calculated size */
    data = (uint8_t *)malloc(mem_size);
    if (SPICE_UNLIKELY(data == NULL)) {
        goto error;
    }
    end = data + sizeof(SpiceMsgcKeyUp);
    in = start;

    out = (SpiceMsgcKeyUp *)data;

    out->code = consume_uint32(&in);

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

static uint8_t * parse_msgc_inputs_key_modifiers(uint8_t *message_start, uint8_t *message_end, int minor, size_t *size, message_destructor_t *free_message)
{
    SPICE_GNUC_UNUSED uint8_t *pos;
    uint8_t *start = message_start;
    uint8_t *data = NULL;
    size_t mem_size, nw_size;
    uint8_t *in, *end;
    SpiceMsgcKeyModifiers *out;

    nw_size = 2;
    mem_size = sizeof(SpiceMsgcKeyModifiers);

    /* Check if message fits in reported side */
    if (start + nw_size > message_end) {
        return NULL;
    }

    /* Validated extents and calculated size */
    data = (uint8_t *)malloc(mem_size);
    if (SPICE_UNLIKELY(data == NULL)) {
        goto error;
    }
    end = data + sizeof(SpiceMsgcKeyModifiers);
    in = start;

    out = (SpiceMsgcKeyModifiers *)data;

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

static uint8_t * parse_msgc_inputs_mouse_motion(uint8_t *message_start, uint8_t *message_end, int minor, size_t *size, message_destructor_t *free_message)
{
    SPICE_GNUC_UNUSED uint8_t *pos;
    uint8_t *start = message_start;
    uint8_t *data = NULL;
    size_t mem_size, nw_size;
    uint8_t *in, *end;
    SpiceMsgcMouseMotion *out;

    nw_size = 10;
    mem_size = sizeof(SpiceMsgcMouseMotion);

    /* Check if message fits in reported side */
    if (start + nw_size > message_end) {
        return NULL;
    }

    /* Validated extents and calculated size */
    data = (uint8_t *)malloc(mem_size);
    if (SPICE_UNLIKELY(data == NULL)) {
        goto error;
    }
    end = data + sizeof(SpiceMsgcMouseMotion);
    in = start;

    out = (SpiceMsgcMouseMotion *)data;

    out->dx = consume_int32(&in);
    out->dy = consume_int32(&in);
    out->buttons_state = consume_uint16(&in);

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

static uint8_t * parse_msgc_inputs_mouse_position(uint8_t *message_start, uint8_t *message_end, int minor, size_t *size, message_destructor_t *free_message)
{
    SPICE_GNUC_UNUSED uint8_t *pos;
    uint8_t *start = message_start;
    uint8_t *data = NULL;
    size_t mem_size, nw_size;
    uint8_t *in, *end;
    SpiceMsgcMousePosition *out;

    nw_size = 11;
    mem_size = sizeof(SpiceMsgcMousePosition);

    /* Check if message fits in reported side */
    if (start + nw_size > message_end) {
        return NULL;
    }

    /* Validated extents and calculated size */
    data = (uint8_t *)malloc(mem_size);
    if (SPICE_UNLIKELY(data == NULL)) {
        goto error;
    }
    end = data + sizeof(SpiceMsgcMousePosition);
    in = start;

    out = (SpiceMsgcMousePosition *)data;

    out->x = consume_uint32(&in);
    out->y = consume_uint32(&in);
    out->buttons_state = consume_uint16(&in);
    out->display_id = consume_uint8(&in);

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

static uint8_t * parse_msgc_inputs_mouse_press(uint8_t *message_start, uint8_t *message_end, int minor, size_t *size, message_destructor_t *free_message)
{
    SPICE_GNUC_UNUSED uint8_t *pos;
    uint8_t *start = message_start;
    uint8_t *data = NULL;
    size_t mem_size, nw_size;
    uint8_t *in, *end;
    SpiceMsgcMousePress *out;

    nw_size = 3;
    mem_size = sizeof(SpiceMsgcMousePress);

    /* Check if message fits in reported side */
    if (start + nw_size > message_end) {
        return NULL;
    }

    /* Validated extents and calculated size */
    data = (uint8_t *)malloc(mem_size);
    if (SPICE_UNLIKELY(data == NULL)) {
        goto error;
    }
    end = data + sizeof(SpiceMsgcMousePress);
    in = start;

    out = (SpiceMsgcMousePress *)data;

    out->button = consume_uint8(&in);
    out->buttons_state = consume_uint16(&in);

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

static uint8_t * parse_msgc_inputs_mouse_release(uint8_t *message_start, uint8_t *message_end, int minor, size_t *size, message_destructor_t *free_message)
{
    SPICE_GNUC_UNUSED uint8_t *pos;
    uint8_t *start = message_start;
    uint8_t *data = NULL;
    size_t mem_size, nw_size;
    uint8_t *in, *end;
    SpiceMsgcMouseRelease *out;

    nw_size = 3;
    mem_size = sizeof(SpiceMsgcMouseRelease);

    /* Check if message fits in reported side */
    if (start + nw_size > message_end) {
        return NULL;
    }

    /* Validated extents and calculated size */
    data = (uint8_t *)malloc(mem_size);
    if (SPICE_UNLIKELY(data == NULL)) {
        goto error;
    }
    end = data + sizeof(SpiceMsgcMouseRelease);
    in = start;

    out = (SpiceMsgcMouseRelease *)data;

    out->button = consume_uint8(&in);
    out->buttons_state = consume_uint16(&in);

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

static uint8_t * parse_InputsChannel_msgc(uint8_t *message_start, uint8_t *message_end, uint16_t message_type, int minor, size_t *size_out, message_destructor_t *free_message)
{
    static parse_msg_func_t funcs1[6] =  {
        parse_msgc_ack_sync,
        parse_SpiceMsgEmpty,
        parse_msgc_pong,
        parse_SpiceMsgEmpty,
        parse_SpiceMsgData,
        parse_msgc_disconnecting
    };
    static parse_msg_func_t funcs2[3] =  {
        parse_msgc_inputs_key_down,
        parse_msgc_inputs_key_up,
        parse_msgc_inputs_key_modifiers
    };
    static parse_msg_func_t funcs3[4] =  {
        parse_msgc_inputs_mouse_motion,
        parse_msgc_inputs_mouse_position,
        parse_msgc_inputs_mouse_press,
        parse_msgc_inputs_mouse_release
    };
    if (message_type >= 1 && message_type < 7) {
        return funcs1[message_type-1](message_start, message_end, minor, size_out, free_message);
    } else if (message_type >= 101 && message_type < 104) {
        return funcs2[message_type-101](message_start, message_end, minor, size_out, free_message);
    } else if (message_type >= 111 && message_type < 115) {
        return funcs3[message_type-111](message_start, message_end, minor, size_out, free_message);
    }
    return NULL;
}



static uint8_t * parse_CursorChannel_msgc(uint8_t *message_start, uint8_t *message_end, uint16_t message_type, int minor, size_t *size_out, message_destructor_t *free_message)
{
    static parse_msg_func_t funcs1[6] =  {
        parse_msgc_ack_sync,
        parse_SpiceMsgEmpty,
        parse_msgc_pong,
        parse_SpiceMsgEmpty,
        parse_SpiceMsgData,
        parse_msgc_disconnecting
    };
    if (message_type >= 1 && message_type < 7) {
        return funcs1[message_type-1](message_start, message_end, minor, size_out, free_message);
    }
    return NULL;
}



static uint8_t * parse_PlaybackChannel_msgc(uint8_t *message_start, uint8_t *message_end, uint16_t message_type, int minor, size_t *size_out, message_destructor_t *free_message)
{
    static parse_msg_func_t funcs1[6] =  {
        parse_msgc_ack_sync,
        parse_SpiceMsgEmpty,
        parse_msgc_pong,
        parse_SpiceMsgEmpty,
        parse_SpiceMsgData,
        parse_msgc_disconnecting
    };
    if (message_type >= 1 && message_type < 7) {
        return funcs1[message_type-1](message_start, message_end, minor, size_out, free_message);
    }
    return NULL;
}



static uint8_t * parse_msgc_record_data(uint8_t *message_start, uint8_t *message_end, int minor, size_t *size, message_destructor_t *free_message)
{
    SPICE_GNUC_UNUSED uint8_t *pos;
    uint8_t *start = message_start;
    uint8_t *data = NULL;
    size_t mem_size, nw_size;
    uint8_t *in, *end;
    size_t data__nw_size;
    uint32_t data__nelements;
    SpiceMsgcRecordPacket *out;

    { /* data */
        data__nelements = message_end - (start + 4);

        data__nw_size = data__nelements;
    }

    nw_size = 4 + data__nw_size;
    mem_size = sizeof(SpiceMsgcRecordPacket);

    /* Check if message fits in reported side */
    if (start + nw_size > message_end) {
        return NULL;
    }

    /* Validated extents and calculated size */
    data = (uint8_t *)malloc(mem_size);
    if (SPICE_UNLIKELY(data == NULL)) {
        goto error;
    }
    end = data + sizeof(SpiceMsgcRecordPacket);
    in = start;

    out = (SpiceMsgcRecordPacket *)data;

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

static uint8_t * parse_msgc_record_mode(uint8_t *message_start, uint8_t *message_end, int minor, size_t *size, message_destructor_t *free_message)
{
    SPICE_GNUC_UNUSED uint8_t *pos;
    uint8_t *start = message_start;
    uint8_t *data = NULL;
    size_t mem_size, nw_size;
    uint8_t *in, *end;
    size_t data__nw_size;
    uint32_t data__nelements;
    SpiceMsgcRecordMode *out;

    { /* data */
        data__nelements = message_end - (start + 6);

        data__nw_size = data__nelements;
    }

    nw_size = 6 + data__nw_size;
    mem_size = sizeof(SpiceMsgcRecordMode);

    /* Check if message fits in reported side */
    if (start + nw_size > message_end) {
        return NULL;
    }

    /* Validated extents and calculated size */
    data = (uint8_t *)malloc(mem_size);
    if (SPICE_UNLIKELY(data == NULL)) {
        goto error;
    }
    end = data + sizeof(SpiceMsgcRecordMode);
    in = start;

    out = (SpiceMsgcRecordMode *)data;

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

static uint8_t * parse_msgc_record_start_mark(uint8_t *message_start, uint8_t *message_end, int minor, size_t *size, message_destructor_t *free_message)
{
    SPICE_GNUC_UNUSED uint8_t *pos;
    uint8_t *start = message_start;
    uint8_t *data = NULL;
    size_t mem_size, nw_size;
    uint8_t *in, *end;
    SpiceMsgcRecordStartMark *out;

    nw_size = 4;
    mem_size = sizeof(SpiceMsgcRecordStartMark);

    /* Check if message fits in reported side */
    if (start + nw_size > message_end) {
        return NULL;
    }

    /* Validated extents and calculated size */
    data = (uint8_t *)malloc(mem_size);
    if (SPICE_UNLIKELY(data == NULL)) {
        goto error;
    }
    end = data + sizeof(SpiceMsgcRecordStartMark);
    in = start;

    out = (SpiceMsgcRecordStartMark *)data;

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

static uint8_t * parse_RecordChannel_msgc(uint8_t *message_start, uint8_t *message_end, uint16_t message_type, int minor, size_t *size_out, message_destructor_t *free_message)
{
    static parse_msg_func_t funcs1[6] =  {
        parse_msgc_ack_sync,
        parse_SpiceMsgEmpty,
        parse_msgc_pong,
        parse_SpiceMsgEmpty,
        parse_SpiceMsgData,
        parse_msgc_disconnecting
    };
    static parse_msg_func_t funcs2[3] =  {
        parse_msgc_record_data,
        parse_msgc_record_mode,
        parse_msgc_record_start_mark
    };
    if (message_type >= 1 && message_type < 7) {
        return funcs1[message_type-1](message_start, message_end, minor, size_out, free_message);
    } else if (message_type >= 101 && message_type < 104) {
        return funcs2[message_type-101](message_start, message_end, minor, size_out, free_message);
    }
    return NULL;
}



static uint8_t * parse_msgc_tunnel_service_add(uint8_t *message_start, uint8_t *message_end, int minor, size_t *size, message_destructor_t *free_message)
{
    SPICE_GNUC_UNUSED uint8_t *pos;
    uint8_t *start = message_start;
    uint8_t *data = NULL;
    size_t mem_size, nw_size;
    uint8_t *in, *end;
    SPICE_GNUC_UNUSED intptr_t ptr_size;
    uint32_t n_ptr=0;
    PointerInfo ptr_info[2];
    size_t name__extra_size;
    size_t description__extra_size;
    size_t u__nw_size;
    uint16_t type__value;
    SpiceMsgcTunnelAddGenericService *out;
    uint32_t i;

    { /* name */
        uint32_t name__value;
        uint32_t name__array__nw_size;
        uint32_t name__array__mem_size;
        pos = (start + 14);
        if (SPICE_UNLIKELY(pos + 4 > message_end)) {
            goto error;
        }
        name__value = read_uint32(pos);
        if (SPICE_UNLIKELY(message_start + name__value >= message_end)) {
            goto error;
        }
        name__array__nw_size = spice_strnlen((char *)message_start + name__value, message_end - (message_start + name__value));
        if (SPICE_UNLIKELY(*(message_start + name__value + name__array__nw_size) != 0)) {
            goto error;
        }
        name__array__mem_size = name__array__nw_size;
        /* @nocopy, so no extra size */
        name__extra_size = 0;
    }

    { /* description */
        uint32_t description__value;
        uint32_t description__array__nw_size;
        uint32_t description__array__mem_size;
        pos = (start + 18);
        if (SPICE_UNLIKELY(pos + 4 > message_end)) {
            goto error;
        }
        description__value = read_uint32(pos);
        if (SPICE_UNLIKELY(message_start + description__value >= message_end)) {
            goto error;
        }
        description__array__nw_size = spice_strnlen((char *)message_start + description__value, message_end - (message_start + description__value));
        if (SPICE_UNLIKELY(*(message_start + description__value + description__array__nw_size) != 0)) {
            goto error;
        }
        description__array__mem_size = description__array__nw_size;
        /* @nocopy, so no extra size */
        description__extra_size = 0;
    }

    { /* u */
        pos = start + 0;
        if (SPICE_UNLIKELY(pos + 2 > message_end)) {
            goto error;
        }
        type__value = read_uint16(pos);
        if (type__value == SPICE_TUNNEL_SERVICE_TYPE_IPP) {
            SPICE_GNUC_UNUSED uint8_t *start2 = (start + 22);
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

            u__nw_size = 2 + u__nw_size;
        } else {
            u__nw_size = 0;
        }

    }

    nw_size = 22 + u__nw_size;
    mem_size = sizeof(SpiceMsgcTunnelAddGenericService) + name__extra_size + description__extra_size;

    /* Check if message fits in reported side */
    if (start + nw_size > message_end) {
        return NULL;
    }

    /* Validated extents and calculated size */
    data = (uint8_t *)malloc(mem_size);
    if (SPICE_UNLIKELY(data == NULL)) {
        goto error;
    }
    end = data + sizeof(SpiceMsgcTunnelAddGenericService);
    in = start;

    out = (SpiceMsgcTunnelAddGenericService *)data;

    out->type = consume_uint16(&in);
    out->id = consume_uint32(&in);
    out->group = consume_uint32(&in);
    out->port = consume_uint32(&in);
    /* Reuse data from network message */
    out->name = (size_t)(message_start + consume_uint32(&in));
    /* Reuse data from network message */
    out->description = (size_t)(message_start + consume_uint32(&in));
    if (out->type == SPICE_TUNNEL_SERVICE_TYPE_IPP) {
        out->u.ip.type = consume_uint16(&in);
        if (out->u.ip.type == SPICE_TUNNEL_IP_TYPE_IPv4) {
            uint32_t ipv4__nelements;
            ipv4__nelements = 4;
            memcpy(out->u.ip.u.ipv4, in, ipv4__nelements);
            in += ipv4__nelements;
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

static uint8_t * parse_msgc_tunnel_service_remove(uint8_t *message_start, uint8_t *message_end, int minor, size_t *size, message_destructor_t *free_message)
{
    SPICE_GNUC_UNUSED uint8_t *pos;
    uint8_t *start = message_start;
    uint8_t *data = NULL;
    size_t mem_size, nw_size;
    uint8_t *in, *end;
    SpiceMsgcTunnelRemoveService *out;

    nw_size = 4;
    mem_size = sizeof(SpiceMsgcTunnelRemoveService);

    /* Check if message fits in reported side */
    if (start + nw_size > message_end) {
        return NULL;
    }

    /* Validated extents and calculated size */
    data = (uint8_t *)malloc(mem_size);
    if (SPICE_UNLIKELY(data == NULL)) {
        goto error;
    }
    end = data + sizeof(SpiceMsgcTunnelRemoveService);
    in = start;

    out = (SpiceMsgcTunnelRemoveService *)data;

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

static uint8_t * parse_msgc_tunnel_socket_open_ack(uint8_t *message_start, uint8_t *message_end, int minor, size_t *size, message_destructor_t *free_message)
{
    SPICE_GNUC_UNUSED uint8_t *pos;
    uint8_t *start = message_start;
    uint8_t *data = NULL;
    size_t mem_size, nw_size;
    uint8_t *in, *end;
    SpiceMsgcTunnelSocketOpenAck *out;

    nw_size = 6;
    mem_size = sizeof(SpiceMsgcTunnelSocketOpenAck);

    /* Check if message fits in reported side */
    if (start + nw_size > message_end) {
        return NULL;
    }

    /* Validated extents and calculated size */
    data = (uint8_t *)malloc(mem_size);
    if (SPICE_UNLIKELY(data == NULL)) {
        goto error;
    }
    end = data + sizeof(SpiceMsgcTunnelSocketOpenAck);
    in = start;

    out = (SpiceMsgcTunnelSocketOpenAck *)data;

    out->connection_id = consume_uint16(&in);
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

static uint8_t * parse_msgc_tunnel_socket_open_nack(uint8_t *message_start, uint8_t *message_end, int minor, size_t *size, message_destructor_t *free_message)
{
    SPICE_GNUC_UNUSED uint8_t *pos;
    uint8_t *start = message_start;
    uint8_t *data = NULL;
    size_t mem_size, nw_size;
    uint8_t *in, *end;
    SpiceMsgcTunnelSocketOpenNack *out;

    nw_size = 2;
    mem_size = sizeof(SpiceMsgcTunnelSocketOpenNack);

    /* Check if message fits in reported side */
    if (start + nw_size > message_end) {
        return NULL;
    }

    /* Validated extents and calculated size */
    data = (uint8_t *)malloc(mem_size);
    if (SPICE_UNLIKELY(data == NULL)) {
        goto error;
    }
    end = data + sizeof(SpiceMsgcTunnelSocketOpenNack);
    in = start;

    out = (SpiceMsgcTunnelSocketOpenNack *)data;

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

static uint8_t * parse_msgc_tunnel_socket_fin(uint8_t *message_start, uint8_t *message_end, int minor, size_t *size, message_destructor_t *free_message)
{
    SPICE_GNUC_UNUSED uint8_t *pos;
    uint8_t *start = message_start;
    uint8_t *data = NULL;
    size_t mem_size, nw_size;
    uint8_t *in, *end;
    SpiceMsgcTunnelSocketFin *out;

    nw_size = 2;
    mem_size = sizeof(SpiceMsgcTunnelSocketFin);

    /* Check if message fits in reported side */
    if (start + nw_size > message_end) {
        return NULL;
    }

    /* Validated extents and calculated size */
    data = (uint8_t *)malloc(mem_size);
    if (SPICE_UNLIKELY(data == NULL)) {
        goto error;
    }
    end = data + sizeof(SpiceMsgcTunnelSocketFin);
    in = start;

    out = (SpiceMsgcTunnelSocketFin *)data;

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

static uint8_t * parse_msgc_tunnel_socket_closed(uint8_t *message_start, uint8_t *message_end, int minor, size_t *size, message_destructor_t *free_message)
{
    SPICE_GNUC_UNUSED uint8_t *pos;
    uint8_t *start = message_start;
    uint8_t *data = NULL;
    size_t mem_size, nw_size;
    uint8_t *in, *end;
    SpiceMsgcTunnelSocketClosed *out;

    nw_size = 2;
    mem_size = sizeof(SpiceMsgcTunnelSocketClosed);

    /* Check if message fits in reported side */
    if (start + nw_size > message_end) {
        return NULL;
    }

    /* Validated extents and calculated size */
    data = (uint8_t *)malloc(mem_size);
    if (SPICE_UNLIKELY(data == NULL)) {
        goto error;
    }
    end = data + sizeof(SpiceMsgcTunnelSocketClosed);
    in = start;

    out = (SpiceMsgcTunnelSocketClosed *)data;

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

static uint8_t * parse_msgc_tunnel_socket_closed_ack(uint8_t *message_start, uint8_t *message_end, int minor, size_t *size, message_destructor_t *free_message)
{
    SPICE_GNUC_UNUSED uint8_t *pos;
    uint8_t *start = message_start;
    uint8_t *data = NULL;
    size_t mem_size, nw_size;
    uint8_t *in, *end;
    SpiceMsgcTunnelSocketClosedAck *out;

    nw_size = 2;
    mem_size = sizeof(SpiceMsgcTunnelSocketClosedAck);

    /* Check if message fits in reported side */
    if (start + nw_size > message_end) {
        return NULL;
    }

    /* Validated extents and calculated size */
    data = (uint8_t *)malloc(mem_size);
    if (SPICE_UNLIKELY(data == NULL)) {
        goto error;
    }
    end = data + sizeof(SpiceMsgcTunnelSocketClosedAck);
    in = start;

    out = (SpiceMsgcTunnelSocketClosedAck *)data;

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

static uint8_t * parse_msgc_tunnel_socket_data(uint8_t *message_start, uint8_t *message_end, int minor, size_t *size, message_destructor_t *free_message)
{
    SPICE_GNUC_UNUSED uint8_t *pos;
    uint8_t *start = message_start;
    uint8_t *data = NULL;
    size_t mem_size, nw_size;
    uint8_t *in, *end;
    size_t data__nw_size, data__mem_size;
    uint32_t data__nelements;
    SpiceMsgcTunnelSocketData *out;

    { /* data */
        data__nelements = message_end - (start + 2);

        data__nw_size = data__nelements;
        data__mem_size = sizeof(uint8_t) * data__nelements;
    }

    nw_size = 2 + data__nw_size;
    mem_size = sizeof(SpiceMsgcTunnelSocketData) + data__mem_size;

    /* Check if message fits in reported side */
    if (start + nw_size > message_end) {
        return NULL;
    }

    /* Validated extents and calculated size */
    data = (uint8_t *)malloc(mem_size);
    if (SPICE_UNLIKELY(data == NULL)) {
        goto error;
    }
    end = data + sizeof(SpiceMsgcTunnelSocketData);
    in = start;

    out = (SpiceMsgcTunnelSocketData *)data;

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

static uint8_t * parse_msgc_tunnel_socket_token(uint8_t *message_start, uint8_t *message_end, int minor, size_t *size, message_destructor_t *free_message)
{
    SPICE_GNUC_UNUSED uint8_t *pos;
    uint8_t *start = message_start;
    uint8_t *data = NULL;
    size_t mem_size, nw_size;
    uint8_t *in, *end;
    SpiceMsgcTunnelSocketTokens *out;

    nw_size = 6;
    mem_size = sizeof(SpiceMsgcTunnelSocketTokens);

    /* Check if message fits in reported side */
    if (start + nw_size > message_end) {
        return NULL;
    }

    /* Validated extents and calculated size */
    data = (uint8_t *)malloc(mem_size);
    if (SPICE_UNLIKELY(data == NULL)) {
        goto error;
    }
    end = data + sizeof(SpiceMsgcTunnelSocketTokens);
    in = start;

    out = (SpiceMsgcTunnelSocketTokens *)data;

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

static uint8_t * parse_TunnelChannel_msgc(uint8_t *message_start, uint8_t *message_end, uint16_t message_type, int minor, size_t *size_out, message_destructor_t *free_message)
{
    static parse_msg_func_t funcs1[6] =  {
        parse_msgc_ack_sync,
        parse_SpiceMsgEmpty,
        parse_msgc_pong,
        parse_SpiceMsgEmpty,
        parse_SpiceMsgData,
        parse_msgc_disconnecting
    };
    static parse_msg_func_t funcs2[9] =  {
        parse_msgc_tunnel_service_add,
        parse_msgc_tunnel_service_remove,
        parse_msgc_tunnel_socket_open_ack,
        parse_msgc_tunnel_socket_open_nack,
        parse_msgc_tunnel_socket_fin,
        parse_msgc_tunnel_socket_closed,
        parse_msgc_tunnel_socket_closed_ack,
        parse_msgc_tunnel_socket_data,
        parse_msgc_tunnel_socket_token
    };
    if (message_type >= 1 && message_type < 7) {
        return funcs1[message_type-1](message_start, message_end, minor, size_out, free_message);
    } else if (message_type >= 101 && message_type < 110) {
        return funcs2[message_type-101](message_start, message_end, minor, size_out, free_message);
    }
    return NULL;
}



static uint8_t * parse_SmartcardChannel_msgc(uint8_t *message_start, uint8_t *message_end, uint16_t message_type, int minor, size_t *size_out, message_destructor_t *free_message)
{
    static parse_msg_func_t funcs1[6] =  {
        parse_msgc_ack_sync,
        parse_SpiceMsgEmpty,
        parse_msgc_pong,
        parse_SpiceMsgEmpty,
        parse_SpiceMsgData,
        parse_msgc_disconnecting
    };
    static parse_msg_func_t funcs2[1] =  {
        parse_SpiceMsgData
    };
    if (message_type >= 1 && message_type < 7) {
        return funcs1[message_type-1](message_start, message_end, minor, size_out, free_message);
    } else if (message_type >= 101 && message_type < 102) {
        return funcs2[message_type-101](message_start, message_end, minor, size_out, free_message);
    }
    return NULL;
}

spice_parse_channel_func_t spice_get_client_channel_parser(uint32_t channel, unsigned int *max_message_type)
{
    static struct {spice_parse_channel_func_t func; unsigned int max_messages; } channels[9] =  {
        { NULL, 0},
        { parse_MainChannel_msgc, 108},
        { parse_DisplayChannel_msgc, 101},
        { parse_InputsChannel_msgc, 114},
        { parse_CursorChannel_msgc, 6},
        { parse_PlaybackChannel_msgc, 6},
        { parse_RecordChannel_msgc, 103},
        { parse_TunnelChannel_msgc, 109},
        { parse_SmartcardChannel_msgc, 101}
    };
    if (channel < 9) {
        if (max_message_type != NULL) {
            *max_message_type = channels[channel].max_messages;
        }
        return channels[channel].func;
    }
    return NULL;
}

uint8_t * spice_parse_reply(uint8_t *message_start, uint8_t *message_end, uint32_t channel, uint16_t message_type, int minor, size_t *size_out, message_destructor_t *free_message)
{
    spice_parse_channel_func_t func;
    func = spice_get_client_channel_parser(channel, NULL);
    if (func != NULL) {
        return func(message_start, message_end, message_type, minor, size_out, free_message);
    }
    return NULL;
}

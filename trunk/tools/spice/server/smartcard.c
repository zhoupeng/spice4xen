#include "server/char_device.h"
#include "server/red_channel.h"
#include "server/smartcard.h"
#include "vscard_common.h"

#define SMARTCARD_MAX_READERS 10

typedef struct SmartCardDeviceState {
    SpiceCharDeviceState base;
    reader_id_t          reader_id;
    uint32_t             attached;
    uint8_t             *buf;
    uint32_t             buf_size;
    uint8_t             *buf_pos;
    uint32_t             buf_used;
} SmartCardDeviceState;

enum {
    PIPE_ITEM_TYPE_ERROR=1,
    PIPE_ITEM_TYPE_READER_ADD_RESPONSE,
    PIPE_ITEM_TYPE_MSG,
};

typedef struct ErrorItem {
    PipeItem base;
    reader_id_t reader_id;
    uint32_t error;
} ErrorItem;

typedef struct ReaderAddResponseItem {
    PipeItem base;
    uint32_t reader_id;
} ReaderAddResponseItem;

typedef struct MsgItem {
    PipeItem base;
    VSCMsgHeader* vheader;
} MsgItem;

typedef struct SmartCardChannel {
    RedChannel base;
} SmartCardChannel;

static SmartCardChannel *g_smartcard_channel = NULL;

static struct Readers {
    uint32_t num;
    SpiceCharDeviceInstance* sin[SMARTCARD_MAX_READERS];
} g_smartcard_readers = {0, {NULL}};

static SpiceCharDeviceInstance* smartcard_readers_get_unattached();
static SpiceCharDeviceInstance* smartcard_readers_get(reader_id_t reader_id);
static int smartcard_char_device_add_to_readers(SpiceCharDeviceInstance *sin);
static void smartcard_char_device_attach(
    SpiceCharDeviceInstance *char_device, SmartCardChannel *smartcard_channel);
static void smartcard_char_device_detach(
    SpiceCharDeviceInstance *char_device, SmartCardChannel *smartcard_channel);
static void smartcard_channel_write_to_reader(
    SmartCardChannel *smartcard_channel, VSCMsgHeader *vheader);

static void smartcard_char_device_on_message_from_device(
    SmartCardDeviceState *state, VSCMsgHeader *header);
static void smartcard_on_message_from_device(
    SmartCardChannel *smartcard_channel, VSCMsgHeader *vheader);
static SmartCardDeviceState* smartcard_device_state_new();
static void smartcard_device_state_free(SmartCardDeviceState* st);

void smartcard_char_device_wakeup(SpiceCharDeviceInstance *sin)
{
    SmartCardDeviceState* state = SPICE_CONTAINEROF(
                            sin->st, SmartCardDeviceState, base);
    SpiceCharDeviceInterface *sif = SPICE_CONTAINEROF(sin->base.sif, SpiceCharDeviceInterface, base);
    VSCMsgHeader *vheader = (VSCMsgHeader*)state->buf;
    int n;
    int remaining;

    while ((n = sif->read(sin, state->buf_pos, state->buf_size - state->buf_used)) > 0) {
        state->buf_pos += n;
        state->buf_used += n;
        if (state->buf_used < sizeof(VSCMsgHeader)) {
            continue;
        }
        if (vheader->length > state->buf_size) {
            state->buf_size = MAX(state->buf_size*2, vheader->length + sizeof(VSCMsgHeader));
            state->buf = spice_realloc(state->buf, state->buf_size);
            ASSERT(state->buf != NULL);
        }
        if (state->buf_used - sizeof(VSCMsgHeader) < vheader->length) {
            continue;
        }
        smartcard_char_device_on_message_from_device(state, vheader);
        remaining = state->buf_used - sizeof(VSCMsgHeader) > vheader->length;
        if (remaining > 0) {
            memcpy(state->buf, state->buf_pos, remaining);
        }
        state->buf_pos = state->buf;
        state->buf_used = remaining;
    }
}

void smartcard_char_device_on_message_from_device(
    SmartCardDeviceState *state,
    VSCMsgHeader *vheader)
{
    VSCMsgHeader *sent_header;

    switch (vheader->type) {
        case VSC_Init:
            return;
            break;
        case VSC_ReaderAddResponse:
            /* The device sends this for vscclient, we send one ourselves,
             * a second would be an error. */
            return;
            break;
        case VSC_Reconnect:
            /* Ignore VSC_Reconnect messages, spice channel reconnection does the same. */
            return;
            break;
        default:
            break;
    }
    ASSERT(state->reader_id != VSCARD_UNDEFINED_READER_ID);
    ASSERT(g_smartcard_channel != NULL);
    sent_header = spice_memdup(vheader, sizeof(*vheader) + vheader->length);
    sent_header->reader_id = state->reader_id;
    smartcard_on_message_from_device(g_smartcard_channel, sent_header);
}

static void smartcard_readers_detach_all(SmartCardChannel *smartcard_channel)
{
    int i;

    for (i = 0 ; i < g_smartcard_readers.num; ++i) {
        smartcard_char_device_detach(g_smartcard_readers.sin[i],
                                     smartcard_channel);
    }
}

static int smartcard_char_device_add_to_readers(SpiceCharDeviceInstance *char_device)
{
    SmartCardDeviceState* state = SPICE_CONTAINEROF(
                            char_device->st, SmartCardDeviceState, base);

    if (g_smartcard_readers.num >= SMARTCARD_MAX_READERS) {
        return -1;
    }
    state->reader_id = g_smartcard_readers.num;
    g_smartcard_readers.sin[g_smartcard_readers.num++] = char_device;
    return 0;
}

static SpiceCharDeviceInstance *smartcard_readers_get(reader_id_t reader_id)
{
    ASSERT(reader_id < g_smartcard_readers.num);
    return g_smartcard_readers.sin[reader_id];
}

static SpiceCharDeviceInstance *smartcard_readers_get_unattached()
{
    int i;
    SmartCardDeviceState* state;

    for (i = 0; i < g_smartcard_readers.num; ++i) {
        state = SPICE_CONTAINEROF(g_smartcard_readers.sin[i]->st,
                                  SmartCardDeviceState, base);
        if (!state->attached) {
            return g_smartcard_readers.sin[i];
        }
    }
    return NULL;
}

static SmartCardDeviceState* smartcard_device_state_new()
{
    SmartCardDeviceState *st;

    st = spice_new0(SmartCardDeviceState, 1);
    st->base.wakeup = smartcard_char_device_wakeup;
    st->reader_id = VSCARD_UNDEFINED_READER_ID;
    st->attached = FALSE;
    st->buf_size = APDUBufSize + sizeof(VSCMsgHeader);
    st->buf = spice_malloc(st->buf_size);
    st->buf_pos = st->buf;
    st->buf_used = 0;
    return st;
}

static void smartcard_device_state_free(SmartCardDeviceState* st)
{
    free(st->buf);
    free(st);
}

void smartcard_device_disconnect(SpiceCharDeviceInstance *char_device)
{
    SmartCardDeviceState *st = SPICE_CONTAINEROF(char_device->st,
        SmartCardDeviceState, base);

    smartcard_device_state_free(st);
}

int smartcard_device_connect(SpiceCharDeviceInstance *char_device)
{
    SmartCardDeviceState *st;

    st = smartcard_device_state_new();
    char_device->st = &st->base;
    if (smartcard_char_device_add_to_readers(char_device) == -1) {
        smartcard_device_state_free(st);
        return -1;
    }
    return 0;
}

static void smartcard_char_device_attach(
    SpiceCharDeviceInstance *char_device, SmartCardChannel *smartcard_channel)
{
    SmartCardDeviceState *st = SPICE_CONTAINEROF(char_device->st, SmartCardDeviceState, base);

    if (st->attached == TRUE) {
        return;
    }
    st->attached = TRUE;
    VSCMsgHeader vheader = {.type = VSC_ReaderAdd, .reader_id=st->reader_id,
        .length=0};
    smartcard_channel_write_to_reader(smartcard_channel, &vheader);
}

static void smartcard_char_device_detach(
    SpiceCharDeviceInstance *char_device, SmartCardChannel *smartcard_channel)
{
    SmartCardDeviceState *st = SPICE_CONTAINEROF(char_device->st, SmartCardDeviceState, base);

    if (st->attached == FALSE) {
        return;
    }
    st->attached = FALSE;
    VSCMsgHeader vheader = {.type = VSC_ReaderRemove, .reader_id=st->reader_id,
        .length=0};
    smartcard_channel_write_to_reader(smartcard_channel, &vheader);
}

static int smartcard_channel_config_socket(RedChannel *channel)
{
    return TRUE;
}

static uint8_t *smartcard_channel_alloc_msg_rcv_buf(RedChannel *channel, SpiceDataHeader *msg_header)
{
    //red_printf("allocing %d bytes", msg_header->size);
    return spice_malloc(msg_header->size);
}

static void smartcard_channel_release_msg_rcv_buf(RedChannel *channel, SpiceDataHeader *msg_header,
                                               uint8_t *msg)
{
    red_printf("freeing %d bytes", msg_header->size);
    free(msg);
}

static void smartcard_channel_send_data(RedChannel *channel, PipeItem *item, VSCMsgHeader *vheader)
{
    ASSERT(channel);
    ASSERT(vheader);
    red_channel_init_send_data(channel, SPICE_MSG_SMARTCARD_DATA, item);
    red_channel_add_buf(channel, vheader, sizeof(VSCMsgHeader));
    if (vheader->length > 0) {
        red_channel_add_buf(channel, (uint8_t*)(vheader+1), vheader->length);
    }
    red_channel_begin_send_message(channel);
}

static void smartcard_channel_send_message(RedChannel *channel, PipeItem *item,
    uint32_t reader_id, VSCMsgType type, uint8_t* data, uint32_t len)
{
    VSCMsgHeader mhHeader;
    //SpiceMarshaller* m = msg->marshaller();

    mhHeader.type = type;
    mhHeader.length = len;
    mhHeader.reader_id = reader_id;
    //_marshallers->msg_SpiceMsgData(m, &msgdata);
    //spice_marshaller_add(m, (uint8_t*)&mhHeader, sizeof(mhHeader));
    //spice_marshaller_add(m, data, len);
    //marshaller_outgoing_write(msg);

    smartcard_channel_send_data(channel, item, &mhHeader);
}

static void smartcard_channel_send_error(
    SmartCardChannel *smartcard_channel, PipeItem *item)
{
    ErrorItem* error_item = (ErrorItem*)item;
    VSCMsgError error;

    error.code = error_item->error;
    smartcard_channel_send_message(&smartcard_channel->base, item, error_item->reader_id,
        VSC_Error, (uint8_t*)&error, sizeof(error));
}

static void smartcard_channel_send_reader_add_response(
    SmartCardChannel *smartcard_channel, PipeItem *item)
{
    ReaderAddResponseItem* rar_item = (ReaderAddResponseItem*)item;
    VSCMsgReaderAddResponse rar;

    smartcard_channel_send_message(&smartcard_channel->base, item, rar_item->reader_id,
        VSC_ReaderAddResponse, (uint8_t*)&rar, sizeof(rar));
}

static void smartcard_channel_send_msg(
    SmartCardChannel *smartcard_channel, PipeItem *item)
{
    MsgItem* msg_item = (MsgItem*)item;

    smartcard_channel_send_data(&smartcard_channel->base, item, msg_item->vheader);
}

static void smartcard_channel_send_item(RedChannel *channel, PipeItem *item)
{
    SmartCardChannel *smartcard_channel = (SmartCardChannel *)channel;

    red_channel_reset_send_data(channel);
    switch (item->type) {
    case PIPE_ITEM_TYPE_ERROR:
        smartcard_channel_send_error(smartcard_channel, item);
        break;
    case PIPE_ITEM_TYPE_READER_ADD_RESPONSE:
        smartcard_channel_send_reader_add_response(smartcard_channel, item);
        break;
    case PIPE_ITEM_TYPE_MSG:
        smartcard_channel_send_msg(smartcard_channel, item);
    }
}


static void smartcard_channel_release_pipe_item(RedChannel *channel, PipeItem *item, int item_pushed)
{
    free(item);
    if (item->type == PIPE_ITEM_TYPE_MSG) {
        free(((MsgItem*)item)->vheader);
    }
}

static void smartcard_channel_disconnect(RedChannel *channel)
{
    smartcard_readers_detach_all((SmartCardChannel*)channel);
    red_channel_destroy(channel);
    g_smartcard_channel = NULL;
}

/* this is called from both device input and client input. since the device is
 * a usb device, the context is still the main thread (kvm_main_loop, timers)
 * so no mutex is required. */
static void smartcard_channel_pipe_add(SmartCardChannel *channel, PipeItem *item)
{
    red_channel_pipe_add(&channel->base, item);
}

static void smartcard_push_error(SmartCardChannel* channel, reader_id_t reader_id, VSCErrorCode error)
{
    ErrorItem *error_item = spice_new0(ErrorItem, 1);

    error_item->base.type = PIPE_ITEM_TYPE_ERROR;
    error_item->reader_id = reader_id;
    error_item->error = error;
    smartcard_channel_pipe_add(channel, &error_item->base);
}

static void smartcard_push_reader_add_response(SmartCardChannel *channel, uint32_t reader_id)
{
    ReaderAddResponseItem *rar_item = spice_new0(ReaderAddResponseItem, 1);

    rar_item->base.type = PIPE_ITEM_TYPE_READER_ADD_RESPONSE;
    rar_item->reader_id = reader_id;
    smartcard_channel_pipe_add(channel, &rar_item->base);
}

static void smartcard_push_vscmsg(SmartCardChannel *channel, VSCMsgHeader *vheader)
{
    MsgItem *msg_item = spice_new0(MsgItem, 1);

    msg_item->base.type = PIPE_ITEM_TYPE_MSG;
    msg_item->vheader = vheader;
    smartcard_channel_pipe_add(channel, &msg_item->base);
}

void smartcard_on_message_from_device(SmartCardChannel *smartcard_channel,
    VSCMsgHeader* vheader)
{
    smartcard_push_vscmsg(smartcard_channel, vheader);
}

static void smartcard_remove_reader(SmartCardChannel *smartcard_channel, reader_id_t reader_id)
{
    SpiceCharDeviceInstance *char_device = smartcard_readers_get(reader_id);
    SmartCardDeviceState *state;

    if (char_device == NULL) {
        smartcard_push_error(smartcard_channel, reader_id,
            VSC_GENERAL_ERROR);
        return;
    }

    state = SPICE_CONTAINEROF(char_device->st, SmartCardDeviceState, base);
    if (state->attached == FALSE) {
        smartcard_push_error(smartcard_channel, reader_id,
            VSC_GENERAL_ERROR);
        return;
    }
    smartcard_char_device_detach(char_device, smartcard_channel);
}

static void smartcard_add_reader(SmartCardChannel *smartcard_channel, uint8_t *name)
{
    // TODO - save name somewhere
    SpiceCharDeviceInstance *char_device =
            smartcard_readers_get_unattached();
    SmartCardDeviceState *state;

    if (char_device != NULL) {
        state = SPICE_CONTAINEROF(char_device->st, SmartCardDeviceState, base);
        smartcard_char_device_attach(char_device, smartcard_channel);
        smartcard_push_reader_add_response(smartcard_channel, state->reader_id);
    } else {
        smartcard_push_error(smartcard_channel, VSCARD_UNDEFINED_READER_ID,
            VSC_CANNOT_ADD_MORE_READERS);
    }
}

static void smartcard_channel_write_to_reader(
    SmartCardChannel *smartcard_channel, VSCMsgHeader *vheader)
{
    SpiceCharDeviceInstance *sin;
    SpiceCharDeviceInterface *sif;
    uint32_t n;

    ASSERT(vheader->reader_id >= 0 &&
           vheader->reader_id <= g_smartcard_readers.num);
    sin = g_smartcard_readers.sin[vheader->reader_id];
    sif = SPICE_CONTAINEROF(sin->base.sif, SpiceCharDeviceInterface, base);
    n = sif->write(sin, (uint8_t*)vheader,
                   vheader->length + sizeof(VSCMsgHeader));
    // TODO - add ring
    ASSERT(n == vheader->length + sizeof(VSCMsgHeader));
}

static int smartcard_channel_handle_message(RedChannel *channel, SpiceDataHeader *header, uint8_t *msg)
{
    VSCMsgHeader* vheader = (VSCMsgHeader*)msg;
    SmartCardChannel* smartcard_channel = (SmartCardChannel*)channel;

    ASSERT(header->size == vheader->length + sizeof(VSCMsgHeader));
    switch (vheader->type) {
        case VSC_ReaderAdd:
            smartcard_add_reader(smartcard_channel, msg + sizeof(VSCMsgHeader));
            return TRUE;
            break;
        case VSC_ReaderRemove:
            smartcard_remove_reader(smartcard_channel, vheader->reader_id);
            return TRUE;
            break;
        case VSC_ReaderAddResponse:
            /* We shouldn't get this - we only send it */
            return TRUE;
            break;
        case VSC_Init:
        case VSC_Error:
        case VSC_ATR:
        case VSC_CardRemove:
        case VSC_APDU:
            break; // passed on to device
        default:
            printf("ERROR: unexpected message on smartcard channel\n");
            return TRUE;
    }

    if (vheader->reader_id >= g_smartcard_readers.num) {
        red_printf("ERROR: received message for non existent reader: %d, %d, %d", vheader->reader_id,
            vheader->type, vheader->length);
        return FALSE;
    }
    smartcard_channel_write_to_reader(smartcard_channel, vheader);
    return TRUE;
}

static void smartcard_link(Channel *channel, RedsStreamContext *peer,
                        int migration, int num_common_caps,
                        uint32_t *common_caps, int num_caps,
                        uint32_t *caps)
{
    if (g_smartcard_channel) {
        red_channel_destroy(&g_smartcard_channel->base);
    }
    g_smartcard_channel =
        (SmartCardChannel *)red_channel_create(sizeof(*g_smartcard_channel),
                                        peer, core,
                                        migration, FALSE /* handle_acks */,
                                        smartcard_channel_config_socket,
                                        smartcard_channel_disconnect,
                                        smartcard_channel_handle_message,
                                        smartcard_channel_alloc_msg_rcv_buf,
                                        smartcard_channel_release_msg_rcv_buf,
                                        smartcard_channel_send_item,
                                        smartcard_channel_release_pipe_item);
    if (!g_smartcard_channel) {
        return;
    }
    red_channel_init_outgoing_messages_window(&g_smartcard_channel->base);
}

static void smartcard_shutdown(Channel *channel)
{
}

static void smartcard_migrate(Channel *channel)
{
}

void smartcard_channel_init()
{
    Channel *channel;

    channel = spice_new0(Channel, 1);
    channel->type = SPICE_CHANNEL_SMARTCARD;
    channel->link = smartcard_link;
    channel->shutdown = smartcard_shutdown;
    channel->migrate = smartcard_migrate;
    reds_register_channel(channel);
}


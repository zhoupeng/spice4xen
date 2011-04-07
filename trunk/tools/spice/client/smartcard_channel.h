#ifndef __SMART_CARD_H__
#define __SMART_CARD_H__

#include <map>

#include <vreadert.h>
#include <vscard_common.h>
#include <eventt.h>

#include "red_channel.h"
#include "red_peer.h"

class Application;

struct SmartcardOptions {
    std::vector<std::string> certs;
    std::string dbname;
    bool enable;
    SmartcardOptions();
};

void smartcard_init(const SmartcardOptions* options);

struct ReaderData {
    ReaderData() :
        vreader(NULL),
        reader_id(VSCARD_UNDEFINED_READER_ID),
        name(NULL),
        card_insert_pending(false)
    {}
    VReader *vreader;
    reader_id_t reader_id;
    char* name;
    bool card_insert_pending;
};

void virtual_card_remove();
void virtual_card_insert();

class SmartCardChannel;

class VEventEvent : public Event {
public:
    VEventEvent(SmartCardChannel* smartcard_channel, VEvent* vevent);
    ~VEventEvent();
    SmartCardChannel* _smartcard_channel;
    VReader* _vreader;
    VEvent* _vevent;
};

class ReaderAddEvent: public VEventEvent {
public:
    ReaderAddEvent(SmartCardChannel* smartcard_channel, VEvent* vevent)
        : VEventEvent(smartcard_channel, vevent) {}
    virtual void response(AbstractProcessLoop& events_loop);
};

class ReaderRemoveEvent: public VEventEvent {
public:
    ReaderRemoveEvent(SmartCardChannel* smartcard_channel, VEvent* vevent)
        : VEventEvent(smartcard_channel, vevent) {}
    virtual void response(AbstractProcessLoop& events_loop);
};

class CardInsertEvent: public VEventEvent {
public:
    CardInsertEvent(SmartCardChannel* smartcard_channel, VEvent* vevent)
        : VEventEvent(smartcard_channel, vevent) {}
    virtual void response(AbstractProcessLoop& events_loop);
};

class CardRemoveEvent: public VEventEvent {
public:
    CardRemoveEvent(SmartCardChannel* smartcard_channel, VEvent* vevent)
        : VEventEvent(smartcard_channel, vevent) {}
    virtual void response(AbstractProcessLoop& events_loop);
};

class VSCMessageEvent: public Event {
public:
    VSCMessageEvent(SmartCardChannel* smartcard_channel,
        VSCMsgHeader* vheader);
    ~VSCMessageEvent();
    SmartCardChannel* _smartcard_channel;
    VSCMsgHeader* _vheader;
    virtual void response(AbstractProcessLoop& events_loop);
};

class SmartCardChannel : public RedChannel {

public:
    SmartCardChannel(RedClient& client, uint32_t id);
    void handle_smartcard_data(RedPeer::InMessage* message);

    void virtual_card_remove();
    void virtual_card_insert();
    static ChannelFactory& Factory();
protected:
    virtual void on_connect();
    virtual void on_disconnect();

private:
    static void* cac_card_events_thread_entry(void* data);
    void cac_card_events_thread_main();
    void send_message(reader_id_t reader_id, VSCMsgType type, uint8_t* data, uint32_t len);

    Thread* _event_thread;

    Application* _app;

    VReaderList *_reader_list;
    typedef std::map<reader_id_t, ReaderData*> readers_by_id_t;
    readers_by_id_t _readers_by_id;
    typedef std::map<VReader*, ReaderData*> readers_by_vreader_t;
    readers_by_vreader_t _readers_by_vreader;
    readers_by_vreader_t _unallocated_readers_by_vreader;

    ReaderData* reader_data_from_vreader(VReader* vreader);
    ReaderData* reader_data_from_reader_id(reader_id_t reader_id);
    void add_unallocated_reader(VReader* vreader, const char* name);
    ReaderData* add_reader(reader_id_t reader_id);
    void remove_reader(ReaderData* data);
    void send_reader_added(const char* reader_name);
    void send_reader_removed(reader_id_t reader_id);
    void send_atr(VReader* vreader);

    friend class ReaderAddEvent;
    friend class ReaderRemoveEvent;
    friend class CardInsertEvent;
    friend class CardRemoveEvent;
    friend class VSCMessageEvent;
};

#endif // __SMART_CARD_H__


#ifndef __CHAR_DEVICE_H__
#define __CHAR_DEVICE_H__

#include "server/spice-experimental.h"

struct SpiceCharDeviceState {
    void (*wakeup)(SpiceCharDeviceInstance *sin);
};

#endif // __CHAR_DEVICE_H__


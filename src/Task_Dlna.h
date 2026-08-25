#ifndef TASK_DLNA_H
#define TASK_DLNA_H

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

// PrioDlnaClient's network I/O (SSDP discovery, HTTP GET/POST) is fully
// blocking, so it must never run on DisplayTask - that would freeze the whole
// screen/input for the entire discovery/browse duration. DlnaTask owns the
// only PrioDlnaClient instance and is the only thread allowed to touch it;
// PrioDlnaBrowser (on DisplayTask) talks to it only through these two queues.

enum DlnaCommandType {
    DLNA_CMD_SEEK,
    DLNA_CMD_BROWSE
};

struct DlnaCommand {
    DlnaCommandType command;
    uint8_t serverIndex;
    char objectId[64];
};

enum DlnaEventType {
    DLNA_EVT_SEEK_READY,
    DLNA_EVT_SEEK_FAILED,
    DLNA_EVT_BROWSE_READY,
    DLNA_EVT_BROWSE_FAILED
};

// Carries only small/fixed-size data - PrioDlnaBrowser reads the actual result
// lists itself via PrioDlnaClient::getServer()/getBrowseResult() (safe: it only
// does so for the screen it's currently showing, and always moves off that
// screen before DlnaTask is asked to start a new command that would replace
// the underlying data).
struct DlnaEvent {
    DlnaEventType type;
    uint16_t count;
};

void DlnaTask(void *parameter);

#endif // TASK_DLNA_H

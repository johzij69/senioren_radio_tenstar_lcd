#include "Task_Dlna.h"
#include "PrioDlnaClient.h"

// The one and only PrioDlnaClient instance. Only DlnaTask's thread may call
// methods on it (seekServer/browseServer/loop) - PrioDlnaBrowser on DisplayTask
// only ever reads it back via getServer()/getBrowseResult().
PrioDlnaClient dlnaClient;

extern QueueHandle_t DlnaEventQueue;

// Set by the ready callbacks, cleared right before issuing the matching command.
// PrioDlnaClient's internal state machine can fall back to IDLE on a failed
// HTTP round-trip (bad connect, timeout, ...) without ever calling the ready
// callback - if that happens we'd otherwise never tell PrioDlnaBrowser, and it
// would sit on "Laden..." forever. Watching for "back to IDLE but ready never
// fired" catches that silent-failure path too.
static volatile bool s_seekReadyFired = false;
static volatile bool s_browseReadyFired = false;

static void onDlnaInfo(const char* info) {
    Serial.print("DLNA: ");
    Serial.println(info);
}

static void onDlnaSeekReady(uint8_t numberOfServers) {
    s_seekReadyFired = true;
    DlnaEvent evt{ DLNA_EVT_SEEK_READY, numberOfServers };
    xQueueSend(DlnaEventQueue, &evt, 0);
}

static void onDlnaBrowseReady(uint16_t numberReturned, uint16_t totalMatches) {
    (void)totalMatches;
    s_browseReadyFired = true;
    DlnaEvent evt{ DLNA_EVT_BROWSE_READY, numberReturned };
    xQueueSend(DlnaEventQueue, &evt, 0);
}

void DlnaTask(void *parameter) {
    QueueHandle_t commandQueue = static_cast<QueueHandle_t>(parameter);

    dlnaClient.setCallbacks(onDlnaInfo, nullptr, onDlnaSeekReady, nullptr, onDlnaBrowseReady);

    Serial.println("DlnaTask: gestart");

    bool waitingForSeek = false;
    bool waitingForBrowse = false;

    while (true) {
        DlnaCommand cmd;
        if (xQueueReceive(commandQueue, &cmd, 0) == pdTRUE) {
            if (cmd.command == DLNA_CMD_SEEK) {
                s_seekReadyFired = false;
                if (!dlnaClient.seekServer()) {
                    DlnaEvent evt{ DLNA_EVT_SEEK_FAILED, 0 };
                    xQueueSend(DlnaEventQueue, &evt, 0);
                } else {
                    waitingForSeek = true;
                }
            } else if (cmd.command == DLNA_CMD_BROWSE) {
                s_browseReadyFired = false;
                if (dlnaClient.browseServer(cmd.serverIndex, cmd.objectId) != 0) {
                    DlnaEvent evt{ DLNA_EVT_BROWSE_FAILED, 0 };
                    xQueueSend(DlnaEventQueue, &evt, 0);
                } else {
                    waitingForBrowse = true;
                }
            }
        }

        // This can block for a while (SSDP wait, blocking HTTP GET/POST) - that's
        // exactly why it must run here and not on DisplayTask.
        dlnaClient.loop();

        if (waitingForSeek && dlnaClient.getState() == PrioDlnaClient::IDLE) {
            waitingForSeek = false;
            if (!s_seekReadyFired) {
                DlnaEvent evt{ DLNA_EVT_SEEK_FAILED, 0 };
                xQueueSend(DlnaEventQueue, &evt, 0);
            }
        }
        if (waitingForBrowse && dlnaClient.getState() == PrioDlnaClient::IDLE) {
            waitingForBrowse = false;
            if (!s_browseReadyFired) {
                DlnaEvent evt{ DLNA_EVT_BROWSE_FAILED, 0 };
                xQueueSend(DlnaEventQueue, &evt, 0);
            }
        }

        vTaskDelay(5 / portTICK_PERIOD_MS);
    }
}

# include "generalHelpers.h"
#include <esp_heap_caps.h>
#include <mbedtls/platform.h>

// Blokken vanaf deze grootte gaan naar PSRAM; kleintjes blijven intern zodat de
// (tragere) PSRAM-toegang de TLS-handshake niet onnodig vertraagt.
static const size_t TLS_PSRAM_THRESHOLD = 512;

// De Arduino-core is gebouwd met CONFIG_MBEDTLS_INTERNAL_MEM_ALLOC, dus mbedTLS
// eist standaard intern RAM (~36 kB per verbinding) en faalt met -32512 zodra de
// interne heap gefragmenteerd raakt. De precompiled lib laat zich niet
// herconfigureren, maar mbedtls_calloc/free zijn function pointers.
static void *tlsPsramCalloc(size_t n, size_t size)
{
    size_t total = 0;
    if (__builtin_mul_overflow(n, size, &total) || total == 0) {
        return nullptr;
    }

    void *ptr = nullptr;
    if (total >= TLS_PSRAM_THRESHOLD) {
        ptr = heap_caps_calloc(n, size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    }
    if (!ptr) {
        ptr = heap_caps_calloc(n, size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    }
    return ptr;
}

static void tlsPsramFree(void *ptr)
{
    if (ptr) {
        heap_caps_free(ptr);
    }
}

void enableTlsPsramAllocator()
{
    if (!psramFound()) {
        Serial.println("[MEM] Geen PSRAM gevonden, TLS blijft interne heap gebruiken");
        return;
    }

    if (mbedtls_platform_set_calloc_free(tlsPsramCalloc, tlsPsramFree) != 0) {
        Serial.println("[MEM] Kon mbedTLS allocator niet omzetten naar PSRAM");
        return;
    }

    Serial.printf("[MEM] mbedTLS allocaties >= %u bytes gaan naar PSRAM (%u bytes vrij)\n",
                  (unsigned)TLS_PSRAM_THRESHOLD, (unsigned)ESP.getFreePsram());
}

bool updatePageOffset(int selectedIndex, int totalItems, int visibleRows, int &scrollOffset)
{
    int oldOffset = scrollOffset;

    if (selectedIndex >= scrollOffset + visibleRows) {
        scrollOffset = selectedIndex;
    } else if (selectedIndex < scrollOffset) {
        scrollOffset = selectedIndex - visibleRows + 1;
    }

    if (scrollOffset > totalItems - 1) scrollOffset = totalItems - 1;
    if (scrollOffset < 0) scrollOffset = 0;

    return oldOffset != scrollOffset;
}

void searchAndReplace(String *htmlString, String findPattern, String replaceWith)
{
  int index = 0;
  while ((index = htmlString->indexOf(findPattern, index)) != -1)
  {
    htmlString->replace(findPattern, replaceWith);
    // Update index to search for next occurrence
    index += replaceWith.length();
  }
}


void printBinary(int v, int num_places)
{
	int mask = 0, n;

	for (n = 1; n <= num_places; n++) {
		mask = (mask << 1) | 0x0001;
	}
	v = v & mask;  // truncate v to specified number of places

	while (num_places) {

		if (v & (0x0001 << (num_places - 1))) {
			Serial.print("1");
		}
		else {
			Serial.print("0");
		}

		--num_places;
		if (((num_places % 4) == 0) && (num_places != 0)) {
			Serial.print("_");
		}
	}
	Serial.println("");
}

// Functie om de core van een taak te printen
void printTaskCore(TaskHandle_t taskHandle, const char *taskName)
{
    BaseType_t coreID = xTaskGetCoreID(taskHandle);
    if (coreID == tskNO_AFFINITY)
    {
        Serial.printf("Task %s is not pinned to any core.\n", taskName);
    }
    else
    {
        Serial.printf("Task %s is running on core %d.\n", taskName, coreID);
    }
}
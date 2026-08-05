# Optie 2: Meer Geheugen Vrijmaken op ESP32 (COMPLEX)

## Drastische Geheugen Optimalisaties

### 1. Suspend andere tasks tijdens image download
```cpp
// In fetchImage.cpp, VOOR WiFiClientSecure allocatie:

void downloadTask(void* pvParameters) {
    // Suspend alle niet-essentiële tasks
    extern TaskHandle_t displayTaskHandle;
    extern TaskHandle_t audioTaskHandle;
    extern TaskHandle_t webServerTaskHandle;
    
    bool displayWasSuspended = false;
    bool audioWasSuspended = false;
    bool webWasSuspended = false;
    
    if (displayTaskHandle != NULL) {
        vTaskSuspend(displayTaskHandle);
        displayWasSuspended = true;
    }
    if (audioTaskHandle != NULL) {
        vTaskSuspend(audioTaskHandle);
        audioWasSuspended = true;
    }
    if (webServerTaskHandle != NULL) {
        vTaskSuspend(webServerTaskHandle);
        webWasSuspended = true;
    }
    
    // Force garbage collection
    delay(100);
    
    Serial.printf("[MEMORY] Free heap after suspend: %d bytes\n", ESP.getFreeHeap());
    
    // ... SSL download code ...
    
    // Resume tasks
    if (displayWasSuspended) vTaskResume(displayTaskHandle);
    if (audioWasSuspended) vTaskResume(audioTaskHandle);
    if (webWasSuspended) vTaskResume(webServerTaskHandle);
}
```

### 2. Gebruik PSRAM voor SSL buffers
```cpp
// platformio.ini
build_flags = 
    -DCONFIG_MBEDTLS_EXTERNAL_MEM_ALLOC=1
    -DCONFIG_SPIRAM_TRY_ALLOCATE_WIFI=1
    -DCONFIG_SPIRAM_USE_MALLOC=1
```

### 3. Kleinere certificaat chain op HAProxy
```bash
# Gebruik ALLEEN server cert, geen intermediates
cat server.crt > /etc/haproxy/ssl/minimal.pem
cat server.key >> /etc/haproxy/ssl/minimal.pem

# Update HAProxy
bind :444 ssl crt /etc/haproxy/ssl/minimal.pem ssl-min-ver TLSv1.2 ssl-max-ver TLSv1.2
```

### 4. Gebruik TLS 1.2 met meest minimale cipher
```cpp
// In fetchImage.cpp
secureClient->setInsecure();

// Force TLS 1.2 alleen (geen 1.3)
// Dit zou in theory mbedTLS config moeten zijn, maar is complex in Arduino
```

## Waarom dit NIET de voorkeur heeft:

1. ❌ Complex en fragiel
2. ❌ Kan andere functionaliteit breken
3. ❌ Nog steeds geen garantie (33KB → 40KB is te krap)
4. ❌ Moeilijk te debuggen
5. ❌ Niet toekomstbestendig (TLS evolueert naar meer geheugen)

## Conclusie

**Gebruik Optie 1 (HTTP Proxy)** - Het is simpeler, betrouwbaarder en veiliger.

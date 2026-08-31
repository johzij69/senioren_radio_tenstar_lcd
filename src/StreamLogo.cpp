#include "StreamLogo.h"

#define FORMAT_LITTLEFS_IF_FAILED false
static const char *FALLBACK_LOGO_JPG = "/StreamLogos/webradio-default.jpg";
static const char *FALLBACK_LOGO_PNG = "/StreamLogos/webradio-default.jpg";

// Return the minimum of two values a and b
#define minimum(a, b) (((a) < (b)) ? (a) : (b))

// Static variables for PNG rendering
static TFT_eSPI* s_tft_for_png = nullptr;
static int s_png_xpos = 0;
static int s_png_ypos = 0;
static PNG s_png;
static uint16_t s_png_yield_count = 0;
static int s_png_clip_w = LOGO_BOX_W;
static int s_png_clip_h = LOGO_BOX_H;

// Static PNG line buffer (allocated in PSRAM to prevent stack overflow)
uint16_t* StreamLogo::s_png_line_buffer = nullptr;

StreamLogo::StreamLogo(TFT_eSPI *tft)
{
    this->tft = tft;
    image_folder = "/StreamLogos/";
}

void StreamLogo::begin()
{

    // only execute ones on new esp
    if(!LittleFS.begin(FORMAT_LITTLEFS_IF_FAILED)){
        Serial.println("LittleFS Mount Failed");

    }

    LittleFS.begin();
    // LittleFS.remove("/StreamLogosNPO-Radio1.jpg");
  //  LittleFS.format();


    PrioLittleFS lFS;
    lFS.listLittleFS();

    // Create StreamImages folder if it doesn't exist
    if (!LittleFS.exists(image_folder))
    {
        Serial.println("creating folder " + image_folder);
        LittleFS.mkdir(image_folder);
    }

    if (LittleFS.exists(image_folder))
    {
        Serial.println("folder exists " + image_folder);
    }
    
    // Alloceer PNG line buffer in PSRAM (voorkomt stack overflow)
    if (!s_png_line_buffer) {
        s_png_line_buffer = (uint16_t*)ps_malloc(MAX_IMAGE_WIDTH * sizeof(uint16_t));
        if (!s_png_line_buffer) {
            Serial.println("[PNG] ERROR: Failed to allocate PNG line buffer in PSRAM");
        } else {
            Serial.printf("[PNG] Line buffer allocated in PSRAM (%d bytes)\n", MAX_IMAGE_WIDTH * 2);
        }
    }
}

void StreamLogo::clearBox()
{
    tft->fillRect(box_x, box_y, LOGO_BOX_W, LOGO_BOX_H, TFT_BLACK);
}

void StreamLogo::Show(int16_t x, int16_t y, String url)
{
    box_x = x;
    box_y = y;
    clearBox();

    if (url.length() == 0)
    {
        Serial.println("[WARN] Lege logo URL, gebruik fallback logo.");
        if (fileExists(FALLBACK_LOGO_JPG))
        {
            drawJpeg(FALLBACK_LOGO_JPG, x, y);
        }
        return;
    }

    String filename = image_folder + url.substring(url.lastIndexOf("/") + 1);

    // Download the image from the internet if it doesn't exist locally AND it's a URL
    if (!LittleFS.exists(filename) && (url.startsWith("http://") || url.startsWith("https://")))
    {
        LoadImage(url, filename);
    }

    if (fileExists(filename))
    {
        // Detect file type by magic bytes instead of extension
        File testFile = LittleFS.open(filename.c_str(), "r");
        if (testFile) {
            uint8_t magic[4] = {0};
            size_t bytesRead = testFile.readBytes((char*)magic, 4);
            testFile.close();
            
            // Yield to other tasks after file operation
            yield();
            
            if (bytesRead >= 4) {
                // Check for PNG: 89 50 4E 47
                bool isPng = (magic[0] == 0x89 && magic[1] == 0x50 && magic[2] == 0x4E && magic[3] == 0x47);
                // Check for JPEG: FF D8 FF
                bool isJpeg = (magic[0] == 0xFF && magic[1] == 0xD8 && magic[2] == 0xFF);
                
                if (isPng) {
                    Serial.println("[LOGO] Detected PNG format");
                    drawPng(filename.c_str(), x, y);
                } else if (isJpeg) {
                    Serial.println("[LOGO] Detected JPEG format");
                    drawJpeg(filename.c_str(), x, y);
                } else {
                    Serial.printf("[LOGO] Unknown format (magic: %02X %02X %02X %02X), trying JPEG\n", 
                                  magic[0], magic[1], magic[2], magic[3]);
                    drawJpeg(filename.c_str(), x, y);
                }
            } else {
                Serial.println("[WARN] Could not read magic bytes, defaulting to JPEG");
                drawJpeg(filename.c_str(), x, y);
            }
        } else {
            Serial.println("[WARN] Could not open file for format detection");
            drawJpeg(filename.c_str(), x, y);
        }
        return;
    }

    Serial.println("[WARN] Logo niet beschikbaar na download, gebruik fallback logo.");
    if (fileExists(FALLBACK_LOGO_JPG))
    {
        drawJpeg(FALLBACK_LOGO_JPG, x, y);
    }
    else if (fileExists(FALLBACK_LOGO_PNG))
    {
        Serial.println("[WARN] Alleen PNG fallback gevonden, render nog niet ondersteund.");
    }
    else
    {
        Serial.println("[ERROR] Geen fallback logo aanwezig in /StreamLogos.");
    }
}

bool StreamLogo::LoadImage(String url, String filename)
{

    bool result = false;
    result = getFile(url, filename);
    return result;
}

void StreamLogo::drawJpeg(const char *filename, int xpos, int ypos)
{
    // Open het bestand
    File jpegFile = LittleFS.open(filename, "r");

    if (!jpegFile)
    {
        Serial.print("ERROR: File \"");
        Serial.print(filename);
        Serial.println("\" not found!");
        return;
    }

    if (jpegFile.size() == 0)
    {
        Serial.print("ERROR: File \"");
        Serial.print(filename);
        Serial.println("\" file size is 0!");
        jpegFile.close();  // Sluit bestand om LittleFS fouten te voorkomen
        return;
    }

    Serial.println(String(jpegFile.size()));
    Serial.println("===========================");
    Serial.print("Drawing file: ");
    Serial.println(filename);
    Serial.println("===========================");

    // Gebruik bestandshandle in plaats van bestandsnaam
    bool decoded = JpegDec.decodeFsFile(jpegFile);

    if (decoded)
    {
        Serial.println("rendering image");
        int drawX = xpos + (LOGO_BOX_W - (int)JpegDec.width) / 2;
        int drawY = ypos + (LOGO_BOX_H - (int)JpegDec.height) / 2;
        if (drawX < xpos) drawX = xpos;
        if (drawY < ypos) drawY = ypos;
        renderJPEG(drawX, drawY);
    }
    else
    {
        Serial.println("Jpeg file format not supported!");
    }

    jpegFile.close();  // Zorg ervoor dat het bestand correct wordt gesloten
}


bool StreamLogo::fileExists(String path)
{
    return LittleFS.exists(path);
}

void StreamLogo::renderJPEG(int xpos, int ypos)
{

    // retrieve information about the image
    uint16_t *pImg;
    uint16_t mcu_w = JpegDec.MCUWidth;
    uint16_t mcu_h = JpegDec.MCUHeight;
    int max_x = xpos + (int)JpegDec.width;
    int max_y = ypos + (int)JpegDec.height;

    // Nooit buiten het vaste logo-vak tekenen
    if (max_x > box_x + LOGO_BOX_W) max_x = box_x + LOGO_BOX_W;
    if (max_y > box_y + LOGO_BOX_H) max_y = box_y + LOGO_BOX_H;

    // save the current image block size
    uint32_t win_w = mcu_w;
    uint32_t win_h = mcu_h;

    // record the current time so we can measure how long it takes to draw an image
    uint32_t drawTime = millis();

    // Counter for yielding to other tasks
    int blockCount = 0;

    // read each MCU block until there are no more
    while (JpegDec.read())
    {
        // Yield every 5 MCU blocks to prevent blocking audio task
        if (++blockCount >= 5) {
            blockCount = 0;
            yield();
        }

        // save a pointer to the image block
        pImg = JpegDec.pImage;

        // calculate where the image block should be drawn on the screen
        int mcu_x = JpegDec.MCUx * mcu_w + xpos; // Calculate coordinates of top left corner of current MCU
        int mcu_y = JpegDec.MCUy * mcu_h + ypos;

        // crop the block against the right/bottom edge of the logo box
        int block_w = (mcu_x + mcu_w <= max_x) ? mcu_w : max_x - mcu_x;
        int block_h = (mcu_y + mcu_h <= max_y) ? mcu_h : max_y - mcu_y;
        if (block_w <= 0 || block_h <= 0)
            continue;

        win_w = block_w;
        win_h = block_h;

        // copy pixels into a contiguous block
        if (win_w != mcu_w)
        {
            uint16_t *cImg;
            int p = 0;
            cImg = pImg + win_w;
            for (int h = 1; h < win_h; h++)
            {
                p += mcu_w;
                for (int w = 0; w < win_w; w++)
                {
                    *cImg = *(pImg + w + p);
                    cImg++;
                }
            }
        }

        // calculate how many pixels must be drawn
        uint32_t mcu_pixels = win_w * win_h;

        tft->startWrite();

        // draw image MCU block only if it will fit on the screen
        if ((mcu_x + win_w) <= tft->width() && (mcu_y + win_h) <= tft->height())
        {

            // Now set a MCU bounding window on the TFT to push pixels into (x, y, x + width - 1, y + height - 1)
            tft->setAddrWindow(mcu_x, mcu_y, win_w, win_h);

            // Write all MCU pixels to the TFT window
            while (mcu_pixels--)
            {
                // Push each pixel to the TFT MCU area
                tft->pushColor(*pImg++);
            }
        }
        else if ((mcu_y + (int)win_h) >= tft->height())
            JpegDec.abort(); // Image has run off bottom of screen so abort decoding

        tft->endWrite();
    }

    // calculate how long it took to draw the image
    drawTime = millis() - drawTime;

    // print the results to the serial port
    Serial.print(F("Total render time was    : "));
    Serial.print(drawTime);
    Serial.println(F(" ms"));
    Serial.println(F(""));
}

//====================================================================================
//                                      PNG Support
//====================================================================================

void StreamLogo::drawPng(const char *filename, int xpos, int ypos)
{
    // Open het bestand
    File pngFile = LittleFS.open(filename, "r");

    if (!pngFile)
    {
        Serial.print("[PNG] ERROR: File \"");
        Serial.print(filename);
        Serial.println("\" not found!");
        return;
    }

    if (pngFile.size() == 0)
    {
        Serial.print("[PNG] ERROR: File \"");
        Serial.print(filename);
        Serial.println("\" file size is 0!");
        pngFile.close();
        return;
    }

    // Verify file format by checking magic bytes
    uint8_t magic[8];
    pngFile.readBytes((char*)magic, 8);
    pngFile.seek(0); // Reset to beginning
    
    // Check for PNG: 89 50 4E 47 0D 0A 1A 0A
    if (!(magic[0] == 0x89 && magic[1] == 0x50 && magic[2] == 0x4E && magic[3] == 0x47)) {
        Serial.printf("[PNG] ERROR: Not a valid PNG file (magic: %02X %02X %02X %02X)\\n", 
                      magic[0], magic[1], magic[2], magic[3]);
        pngFile.close();
        return;
    }

    Serial.printf("[PNG] Drawing: %s (%d bytes)\n", filename, pngFile.size());

    // Read file into buffer for PNGdec
    size_t fileSize = pngFile.size();
    uint8_t* buffer = (uint8_t*)ps_malloc(fileSize);  // Use PSRAM for large images
    
    if (!buffer) {
        Serial.println("[PNG] ERROR: Failed to allocate memory for PNG buffer");
        pngFile.close();
        return;
    }
    
    size_t bytesRead = 0;
    const size_t chunkSize = 1024;
    while (bytesRead < fileSize) {
        size_t remaining = fileSize - bytesRead;
        size_t toRead = remaining < chunkSize ? remaining : chunkSize;
        size_t readNow = pngFile.readBytes((char*)buffer + bytesRead, toRead);
        if (readNow == 0) {
            break;
        }
        bytesRead += readNow;
    }
    pngFile.close();
    
    if (bytesRead != fileSize) {
        Serial.println("[PNG] ERROR: Failed to read PNG file completely");
        free(buffer);
        return;
    }

    // Open PNG from RAM buffer
    int rc = s_png.openRAM(buffer, fileSize, pngDraw);
    
    if (rc == PNG_SUCCESS)
    {
        Serial.printf("[PNG] Image: %dx%d, %dbpp\n", s_png.getWidth(), s_png.getHeight(), s_png.getBpp());
        renderPNG(xpos, ypos);
        s_png.close();
    }
    else
    {
        Serial.printf("[PNG] Decode failed: %d\n", rc);
    }

    free(buffer);
}

int StreamLogo::pngDraw(PNGDRAW *pDraw)
{
    if (!s_tft_for_png || !s_png_line_buffer) return 1;
    
    // Yield every 10 lines to prevent blocking
    if (++s_png_yield_count >= 10) {
        s_png_yield_count = 0;
        yield();
    }
    
    // Convert PNG line to RGB565 and render
    if (pDraw->y >= s_png_clip_h) return 1;

    int lineWidth = pDraw->iWidth;
    if (lineWidth > s_png_clip_w) lineWidth = s_png_clip_w;
    if (lineWidth <= 0) return 1;

    s_png.getLineAsRGB565(pDraw, s_png_line_buffer, PNG_RGB565_BIG_ENDIAN, 0xffffffff);

    s_tft_for_png->pushImage(s_png_xpos, s_png_ypos + pDraw->y, lineWidth, 1, s_png_line_buffer);
    return 1;
}

void StreamLogo::renderPNG(int xpos, int ypos)
{
    int img_width = s_png.getWidth();
    int img_height = s_png.getHeight();

    // s_png_line_buffer is MAX_IMAGE_WIDTH breed; bredere images zouden hem overschrijven
    if (img_width > MAX_IMAGE_WIDTH) {
        Serial.printf("[PNG] Image too wide (%d px, max %d) - skipped\n", img_width, MAX_IMAGE_WIDTH);
        return;
    }

    // Set static variables for callback
    s_tft_for_png = tft;
    s_png_xpos = xpos + (LOGO_BOX_W - img_width) / 2;
    s_png_ypos = ypos + (LOGO_BOX_H - img_height) / 2;
    if (s_png_xpos < xpos) s_png_xpos = xpos;
    if (s_png_ypos < ypos) s_png_ypos = ypos;
    s_png_clip_w = (box_x + LOGO_BOX_W) - s_png_xpos;
    s_png_clip_h = (box_y + LOGO_BOX_H) - s_png_ypos;
    s_png_yield_count = 0;  // Reset yield counter for new render
    
    // Decode and render PNG line-by-line
    unsigned long drawTime = millis();
    int rc = s_png.decode(nullptr, 0);
    drawTime = millis() - drawTime;
    
    if (rc != PNG_SUCCESS) {
        Serial.printf("[PNG] Decode failed: %d\n", rc);
    } else {
        Serial.printf("[PNG] Render time: %lums\n", drawTime);
    }
    
    s_tft_for_png = nullptr;
}


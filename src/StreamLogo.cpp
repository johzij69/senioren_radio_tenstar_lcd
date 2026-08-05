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

void StreamLogo::Show(int16_t x, int16_t y, String url)
{
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
        renderJPEG(xpos, ypos);
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
    uint32_t max_x = JpegDec.width;
    uint32_t max_y = JpegDec.height;

    // Jpeg images are draw as a set of image block (tiles) called Minimum Coding Units (MCUs)
    // Typically these MCUs are 16x16 pixel blocks
    // Determine the width and height of the right and bottom edge image blocks
    uint32_t min_w = minimum(mcu_w, max_x % mcu_w);
    uint32_t min_h = minimum(mcu_h, max_y % mcu_h);

    // save the current image block size
    uint32_t win_w = mcu_w;
    uint32_t win_h = mcu_h;

    // record the current time so we can measure how long it takes to draw an image
    uint32_t drawTime = millis();

    // save the coordinate of the right and bottom edges to assist image cropping
    // to the screen size
    max_x += xpos;
    max_y += ypos;

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

        // check if the image block size needs to be changed for the right edge
        if (mcu_x + mcu_w <= max_x)
            win_w = mcu_w;
        else
            win_w = min_w;
        // check if the image block size needs to be changed for the bottom edge
        if (mcu_y + mcu_h <= max_y)
            win_h = mcu_h;
        else
            win_h = min_h;

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
        else if ((mcu_y + win_h) >= tft->height())
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
    s_png.getLineAsRGB565(pDraw, s_png_line_buffer, PNG_RGB565_BIG_ENDIAN, 0xffffffff);
    
    s_tft_for_png->pushImage(s_png_xpos, s_png_ypos + pDraw->y, pDraw->iWidth, 1, s_png_line_buffer);
    return 1;
}

void StreamLogo::renderPNG(int xpos, int ypos)
{
    int img_width = s_png.getWidth();
    int img_height = s_png.getHeight();
    
    // Set static variables for callback
    s_tft_for_png = tft;
    s_png_xpos = xpos;
    s_png_ypos = ypos;
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


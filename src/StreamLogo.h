#ifndef StreamLogo_H
#define StreamLogo_H

#include <TFT_eSPI.h>
// #include "smallFont.h"  // bar
#include "middleFont.h" // bar
#include "LittleFS.h"
#include "JPEGDecoder.h"
#include "fetchImage.h"
#include <PNGdec.h>
#include "PrioLittleFS.h"

#define MAX_IMAGE_WIDTH 240

class StreamLogo
{

public:
    StreamLogo(TFT_eSPI *tft);
    void begin();
    void Show(int16_t x, int16_t y, String url);

private:
    String image_folder;
    TFT_eSPI *tft;

    bool fileExists(String path);
    bool LoadImage(String url, String filename);
    void renderJPEG(int xpos, int ypos);
    void drawJpeg(const char *filename, int xpos, int ypos);

};

#endif
#ifndef StreamLogo_H
#define StreamLogo_H

#include <TFT_eSPI.h>
// #include "smallFont.h"  // bar
#include "middleFont.h" // bar
#include "LittleFS.h"
#include "JPEGDecoder.h"
#include "fetchImage.h"
#include <PNGdec.h>

   bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t *bitmap);

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
// void StreamLogo::pngDraw(PNGDRAW *pDraw);
//    bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t *bitmap);
    void draw(int16_t x, int16_t y, String file);
    void renderJPEG(int xpos, int ypos) ;
};

#endif
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

// Vaste afmetingen van het logo/cover-vak op het scherm
#define LOGO_BOX_W 150
#define LOGO_BOX_H 150

class StreamLogo
{

public:
    StreamLogo(TFT_eSPI *tft);
    void begin();
    void Show(int16_t x, int16_t y, String url);

private:
    String image_folder;
    TFT_eSPI *tft;

    int16_t box_x = 0;
    int16_t box_y = 0;

    void clearBox();

    bool fileExists(String path);
    bool LoadImage(String url, String filename);
    void renderJPEG(int xpos, int ypos);
    void drawJpeg(const char *filename, int xpos, int ypos);
    
    // PNG Support
    void drawPng(const char *filename, int xpos, int ypos);
    static int pngDraw(PNGDRAW *pDraw);
    void renderPNG(int xpos, int ypos);
    
    // Static line buffer for PNG decode (in PSRAM to prevent stack overflow)
    static uint16_t* s_png_line_buffer;

};

#endif
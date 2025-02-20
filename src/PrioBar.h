#ifndef PrioBar_H
#define PrioBar_H

#include <TFT_eSPI.h>
// #include "smallFont.h"  // bar
#include "middleFont.h" // bar

class PrioBar
{

public:
    int32_t width_set;

    PrioBar(TFT_eSPI *tft);
    void begin(int32_t x, int32_t y, int32_t width, int32_t height, uint16_t fgcolor, uint16_t bgcolor, int max_value);
    void draw(int value);

private:
    int _max_value;
    int32_t _pos_x;
    int32_t _pos_y;
    int32_t _width;
    int32_t _height;

    TFT_eSprite barSpr;

    uint16_t _bgcolor;
    uint16_t _fgcolor;

    unsigned short grays[15];
    unsigned short blue;
};

#endif
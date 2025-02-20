#include "PrioBar.h"

PrioBar::PrioBar(TFT_eSPI *tft) : barSpr(TFT_eSprite(tft))
{

    blue = 0x3DB9;
    int co = 225;
    for (int i = 0; i < 15; i++)
    {
        grays[i] = tft->color565(co, co, co);
        co = co - 15;
    }
}

void PrioBar::begin(int32_t x, int32_t y, int32_t width, int32_t height, uint16_t fgcolor, uint16_t bgcolor, int max_value)
{

    _bgcolor = bgcolor;
    _fgcolor = fgcolor;
    _max_value = max_value;
    _pos_x = x;
    _pos_y = y;
    _width = width;
    width_set = width;
    _height = height;
    barSpr.createSprite(_width, 320);
    barSpr.setTextColor(_fgcolor, _bgcolor);
    barSpr.setTextDatum(4);
}

void PrioBar::draw(int value)
{

    barSpr.fillSprite(_bgcolor);
    barSpr.drawRect(0, 0, _width, _height, grays[8]);
    barSpr.loadFont(middleFont);
    barSpr.setTextColor(blue, _bgcolor);
    barSpr.drawString(String(value), 15, 19, 4);
    barSpr.unloadFont();
    for (int i = 0; i < _max_value; i++)
        barSpr.fillRect(2, _height - (i * 10), _width - 4, 8, grays[8]);
    for (int i = 0; i < value; i++)
        barSpr.fillRect(2, _height - (i * 10), _width - 4, 8, blue);
    barSpr.pushSprite(_pos_x, _pos_y);
}
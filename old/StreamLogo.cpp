#include "StreamLogo.h"
#define FORMAT_LITTLEFS_IF_FAILED true

// Return the minimum of two values a and b
#define minimum(a,b)     (((a) < (b)) ? (a) : (b))
StreamLogo::StreamLogo(TFT_eSPI *tft)
{
    // TFT_eSPI tft = TFT_eSPI();

    this->tft = tft;
    image_folder = "/StreamLogos";
}

void StreamLogo::begin()
{
    
    // only execute ones on new esp
    // if(!LittleFS.begin(FORMAT_LITTLEFS_IF_FAILED)){
    //     Serial.println("LittleFS Mount Failed");
        
    // }
    
    LittleFS.begin();
    // Create StreamImages folder if it doesn't exist
    if (!LittleFS.exists(image_folder))
    {
        Serial.println("creating folder " + image_folder);
            LittleFS.mkdir(image_folder);
    }

    if (LittleFS.exists(image_folder)) {

        Serial.println("folder exists " + image_folder);
    }


  // The jpeg image can be scaled by a factor of 1, 2, 4, or 8
  TJpgDec.setJpgScale(1);

  // The byte order can be swapped (set true for TFT_eSPI)
  TJpgDec.setSwapBytes(true);

  // The decoder must be given the exact name of the rendering function above
  TJpgDec.setCallback(tft_output);

}

void StreamLogo::Show(int16_t x, int16_t y, String url)
{
    String filename = image_folder + url.substring(url.lastIndexOf("/") + 1);

    // Download the image from the internet if it doesn't exist locally
    if (!fileExists(filename))
    {
        Serial.println("loadingImage");
        LoadImage(url, filename);
    }
    Serial.println("loadingImage finished");
    if (fileExists(filename))
    {
        draw(x, y, filename);
    }
}

bool StreamLogo::LoadImage(String url, String filename)
{

    bool result = false;
    // TJpgDec.setJpgScale(1);
    // TJpgDec.setSwapBytes(true);
    // // TJpgDec.setCallback(tft_output);

    result = getFile(url, filename);
    

    return result;
}

void StreamLogo::draw(int16_t x, int16_t y, String file)
{
    TJpgDec.drawFsJpg(x, y, file, LittleFS);
}



//=========================================v==========================================
//                                      pngDraw
//====================================================================================
// This next function will be called during decoding of the png file to
// render each image line to the TFT.  If you use a different TFT library
// you will need to adapt this function to suit.
// Callback function to draw pixels to the display
// void StreamLogo::pngDraw(PNGDRAW *pDraw) {
//   uint16_t lineBuffer[MAX_IMAGE_WIDTH];
//   png.getLineAsRGB565(pDraw, lineBuffer, PNG_RGB565_BIG_ENDIAN, 0xffffffff);
//   tft.pushImage(xpos, ypos + pDraw->y, pDraw->iWidth, 1, lineBuffer);
// }


// bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t *bitmap)
// {

//     // Stop further decoding as image is running off bottom of screen
//     if (y >= tft->height())
//         return 0;

//     // This function will clip the image block rendering automatically at the TFT boundaries
//     tft->pushImage(x, y, w, h, bitmap);

//     // Return 1 to decode next block
//     return 1;
// }

bool StreamLogo::fileExists(String path)
{
    return LittleFS.exists(path);
}

void StreamLogo::renderJPEG(int xpos, int ypos) {

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

  // read each MCU block until there are no more
  while (JpegDec.read()) {
	  
    // save a pointer to the image block
    pImg = JpegDec.pImage ;

    // calculate where the image block should be drawn on the screen
    int mcu_x = JpegDec.MCUx * mcu_w + xpos;  // Calculate coordinates of top left corner of current MCU
    int mcu_y = JpegDec.MCUy * mcu_h + ypos;

    // check if the image block size needs to be changed for the right edge
    if (mcu_x + mcu_w <= max_x) win_w = mcu_w;
    else win_w = min_w;

    // check if the image block size needs to be changed for the bottom edge
    if (mcu_y + mcu_h <= max_y) win_h = mcu_h;
    else win_h = min_h;

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

    tft.startWrite();

    // draw image MCU block only if it will fit on the screen
    if (( mcu_x + win_w ) <= tft.width() && ( mcu_y + win_h ) <= tft.height())
    {

      // Now set a MCU bounding window on the TFT to push pixels into (x, y, x + width - 1, y + height - 1)
      tft.setAddrWindow(mcu_x, mcu_y, win_w, win_h);

      // Write all MCU pixels to the TFT window
      while (mcu_pixels--) {
        // Push each pixel to the TFT MCU area
        tft.pushColor(*pImg++);
      }

    }
    else if ( (mcu_y + win_h) >= tft.height()) JpegDec.abort(); // Image has run off bottom of screen so abort decoding

    tft.endWrite();
  }

  // calculate how long it took to draw the image
  drawTime = millis() - drawTime;

  // print the results to the serial port
  Serial.print(F(  "Total render time was    : ")); Serial.print(drawTime); Serial.println(F(" ms"));
  Serial.println(F(""));
}


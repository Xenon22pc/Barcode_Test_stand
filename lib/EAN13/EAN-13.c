#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "EAN-13.h"


static int binaryCode[96];

int ean13Pattern[10][6] = {
    {0, 0, 0, 0, 0, 0},   // 0
    {0, 0, 1, 0, 1, 1},   // 1
    {0, 0, 1, 1, 0, 1},   // 2
    {0, 0, 1, 1, 1, 0},   // 3
    {0, 1, 0, 0, 1, 1},   // 4
    {0, 1, 1, 0, 0, 1},   // 5
    {0, 1, 1, 1, 0, 0},   // 6
    {0, 1, 0, 1, 0, 1},   // 7
    {0, 1, 0, 1, 1, 0},   // 8
    {0, 1, 1, 0, 1, 0}    // 9
  };

int pixelsPattern[3][10][7] = {
  {
    {0, 0, 0, 1, 1, 0, 1},    // 0
    {0, 0, 1, 1, 0, 0, 1},    // 1
    {0, 0, 1, 0, 0, 1, 1},    // 2
    {0, 1, 1, 1, 1, 0, 1},    // 3
    {0, 1, 0, 0, 0, 1, 1},    // 4
    {0, 1, 1, 0, 0, 0, 1},    // 5
    {0, 1, 0, 1, 1, 1, 1},    // 6
    {0, 1, 1, 1, 0, 1, 1},    // 7
    {0, 1, 1, 0, 1, 1, 1},    // 8
    {0, 0, 0, 1, 0, 1, 1}     // 9
  },
  {
    {0, 1, 0, 0, 1, 1, 1},    // 0
    {0, 1, 1, 0, 0, 1, 1},    // 1
    {0, 0, 1, 1, 0, 1, 1},    // 2
    {0, 1, 0, 0, 0, 0, 1},    // 3
    {0, 0, 1, 1, 1, 0, 1},    // 4
    {0, 1, 1, 1, 0, 0, 1},    // 5
    {0, 0, 0, 0, 1, 0, 1},    // 6
    {0, 0, 1, 0, 0, 0, 1},    // 7
    {0, 0, 0, 1, 0, 0, 1},    // 8
    {0, 0, 1, 0, 1, 1, 1}     // 9
  },
  {
    {1, 1, 1, 0, 0, 1, 0},    // 0
    {1, 1, 0, 0, 1, 1, 0},    // 1
    {1, 1, 0, 1, 1, 0, 0},    // 2
    {1, 0, 0, 0, 0, 1, 0},    // 3
    {1, 0, 1, 1, 1, 0, 0},    // 4
    {1, 0, 0, 1, 1, 1, 0},    // 5
    {1, 0, 1, 0, 0, 0, 0},    // 6
    {1, 0, 0, 0, 1, 0, 0},    // 7
    {1, 0, 0, 1, 0, 0, 0},    // 8
    {1, 1, 1, 0, 1, 0, 0}     // 9
  }
};

static void ean2bin(char *code) {
  int firstDigit = code[0] - '0';

  int digitPos;
  int digit;
  int i;

  // Start segments
  binaryCode[0] = 1;
  binaryCode[1] = 0;
  binaryCode[2] = 1;
  int resPos = 3;

  // Segments for first part
  for (digitPos = 1; digitPos < 7; ++digitPos)
  {
    digit = code[digitPos] - '0';
    for (i = 0; i < 7; i++) {
      binaryCode[resPos] = pixelsPattern[ean13Pattern[firstDigit][digitPos-1]][digit][i];
      resPos++;
    }
  }

  // Medium segments
  binaryCode[resPos] = 0;
  resPos++;
  binaryCode[resPos] = 1;
  resPos++;
  binaryCode[resPos] = 0;
  resPos++;
  binaryCode[resPos] = 1;
  resPos++;
  binaryCode[resPos] = 0;
  resPos++;

  // Segments for second part
  for (digitPos = 7; digitPos < 13; ++digitPos) {
    digit = code[digitPos] - '0';
    for (i = 0; i < 7; i++) {
      binaryCode[resPos] = pixelsPattern[2][digit][i];
      resPos++;
    }
  }

  // Stop segments
  binaryCode[resPos] = 1;
  resPos++;
  binaryCode[resPos] = 0;
  resPos++;
  binaryCode[resPos] = 1;
}

void drawBarCode(uint16_t start_y, uint8_t hight, uint8_t width, char *code) {
  uint16_t start_x = (296-(95*width))/2;
  ean2bin(code);
  for (int x = 0; x < 96; x++) {
    if (binaryCode[x]) Paint_DrawLine(start_x+x*width, start_y, start_x+x*width, start_y+hight, BLACK, width, LINE_STYLE_SOLID);
    else Paint_DrawLine(start_x+x*width, start_y, start_x+x*width, start_y+hight, WHITE, width, LINE_STYLE_SOLID);
  }
}

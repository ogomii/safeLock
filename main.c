#include "LCD_ILI9325.h"
#include "Open1768_LCD.h"
#include "asciiLib.h"
#include <stdbool.h> 
#include "GPIO_LPC17xx.h"
#include "LPC17xx.h"

#define MAX_COL_IDX 7
#define LETTER_HEIGHT 16
#define LETTER_WIDTH 8

static const PIN RAW_PINS[] = {
  {0U, 8U },
  {0U, 9U },
  {0U, 7U },
  {0U, 6U }
}

static const PIN COL_PINS[] = {
  {0U, 17U },
  {0U, 18U },
  {0U, 15U },
  {0U, 16U }
}

struct Frame{
	uint16_t xStart;
	uint16_t xEnd;
	uint16_t yStart;
	uint16_t yEnd;
};

void draw(const struct Frame* frame, const uint16_t color)
{
	lcdWriteReg(HADRPOS_RAM_START, frame->xStart);
	lcdWriteReg(HADRPOS_RAM_END, frame->xEnd);
	lcdWriteReg(VADRPOS_RAM_START, frame->yStart);
	lcdWriteReg(VADRPOS_RAM_END, frame->yEnd);
	
	lcdSetCursor(frame->xStart, frame->yStart);
	
	lcdWriteIndex(DATA_RAM);
	for(int x = frame->xStart; x <= frame->xEnd; x++)
	{
		for(int y = frame->yStart; y <= frame->yEnd; y++)
		{
			lcdWriteData(color);
		}
	}
}

void clearScreen()
{
	struct Frame screenWideFrame = {0, LCD_MAX_X, 0, LCD_MAX_Y};

	draw(&screenWideFrame, LCDWhite);
}

bool shoulPixelBeDrawn(int rowValue, int column)
{
	return (rowValue >> (MAX_COL_IDX-column))%2 == 1;
}

void drawLetter(struct Frame* frame, char letter)
{
	unsigned char letterBuffer[16];
	GetASCIICode(0, letterBuffer, letter);
	for(int row = 0; row < LETTER_HEIGHT; row++)
	{
		for(int col = 0; col < LETTER_WIDTH; col++)
		{
			if(shoulPixelBeDrawn(letterBuffer[row], col))
			{
				lcdWriteReg(ADRX_RAM, frame->xStart + col);
				lcdWriteReg(ADRY_RAM, frame->yStart + row);
				lcdWriteReg(DATA_RAM, LCDBlack);
			}
		}
	}
}

void writeLetters(const char* letters, const struct Frame* startingPossition)
{
	int numberOfLetters = strlen(letters);
	struct Frame letterPossition = {
		startingPossition->xStart,
		startingPossition->xEnd,
		startingPossition->yStart,
		startingPossition->yEnd
	};
	for(int letterIdx = 0; letterIdx < numberOfLetters; letterIdx++)
	{
		drawLetter(&letterPossition, letters[letterIdx]);
		letterPossition.xStart += 10;
		letterPossition.xEnd += 10;
	}
}

void gpioSetup()
{
	for(int n = 0; n < 4; n++)
	{
		PIN_Configure (RAW_PINS[n].Portnum, RAW_PINS[n].Pinnum, PIN_FUNC_0, PIN_PINMODE_PULLDOWN, PIN_PINMODE_NORMAL);
		GPIO_SetDir   (RAW_PINS[n].Portnum, RAW_PINS[n].Pinnum, GPIO_DIR_OUTPUT);
	}
	for(int n = 0; n < 4; n++)
	{
		PIN_Configure (COL_PINS[n].Portnum, COL_PINS[n].Pinnum, PIN_FUNC_0, PIN_PINMODE_PULLDOWN, PIN_PINMODE_NORMAL);
		GPIO_SetDir   (COL_PINS[n].Portnum, COL_PINS[n].Pinnum, GPIO_DIR_INPUT);
	}
}

int keyboardScan()
{
	for(int col = 0; col < 4; col++)
	{
		GPIO_PinWrite (COL_PINS[n].Portnum, COL_PINS[n].Pinnum, 0U);
		for(int raw = 0; raw < 4; raw++)
		{
			if(GPIO_PinRead (RAW_PINS[raw].Portnum, RAW_PINS[raw].Pinnum) == 0)
				return col * 4 + raw;
		}
	}
	return -1;
}

int main()
{
	
	lcdConfiguration();
	init_ILI9325();

	clearScreen();
	
	struct Frame letterFrame = {50, 50 + LETTER_WIDTH, 50, 50 + LETTER_HEIGHT};
	char letters[5] = {'a', 'b', 'c', 'd', '!'};
	writeLetters(letters, &letterFrame);
	
	gpioSetup();
	int keyPressed = keyboardScan();
	if(keyPressed != -1)
	{
		struct Frame frame = {100, 100+LETTER_WIDTH, 100, 100+LETTER_HEIGHT};
		char symbol = keyPressed + '0';
		drawLetter(&frame, symbol);
	}

	struct Frame frame = {100, 200, 100, 250};
	draw(&frame, LCDRed);

	while(1){}
}

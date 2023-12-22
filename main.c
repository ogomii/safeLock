#include "LCD_ILI9325.h"
#include "Open1768_LCD.h"
#include "asciiLib.h"
#include <stdbool.h> 
#include "GPIO_LPC17xx.h"
#include <LPC17xx.h>
#include <PIN_LPC17xx.h>

#define MAX_COL_IDX 7
#define LETTER_HEIGHT 16
#define LETTER_WIDTH 8

static const PIN COL_PINS[] = {
  {0U, 8U },
  {0U, 9U },
  {1U, 31U },
  {0U, 6U }
};

static const PIN ROW_PINS[] = {
  {0U, 17U },
  {0U, 18U },
  {0U, 15U },
  {0U, 16U }
};

static const char KEYBOARD_MAP[] = {
	'1', '2', '3', 'A',
	'4', '5', '6', 'B',
	'7', '8', '9', 'C',
	'0', 'F', 'E', 'D'
};

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

void writeLetters(const char* letters, const struct Frame* startingPossition, const int numberOfLetters)
{
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
		PIN_Configure (COL_PINS[n].Portnum, COL_PINS[n].Pinnum, PIN_FUNC_0, PIN_PINMODE_PULLDOWN, PIN_PINMODE_NORMAL);
		GPIO_SetDir   (COL_PINS[n].Portnum, COL_PINS[n].Pinnum, GPIO_DIR_OUTPUT);
	}
	for(int n = 0; n < 4; n++)
	{
		PIN_Configure (ROW_PINS[n].Portnum, ROW_PINS[n].Pinnum, PIN_FUNC_0, PIN_PINMODE_PULLDOWN, PIN_PINMODE_NORMAL);
		GPIO_SetDir   (ROW_PINS[n].Portnum, ROW_PINS[n].Pinnum, GPIO_DIR_INPUT);
	}
}

int keyboardScan()
{
	for(int col = 0; col <= 4; col++)
	{
		for(int i = 0; i <= 4; i++) GPIO_PinWrite (COL_PINS[i].Portnum, COL_PINS[i].Pinnum, 0U);
		
		GPIO_PinWrite (COL_PINS[col].Portnum, COL_PINS[col].Pinnum, 1U);
		for(int row = 0; row < 4; row++)
		{
			if(GPIO_PinRead (ROW_PINS[row].Portnum, ROW_PINS[row].Pinnum) == 1)
				return col * 4 + row;
		}
	}
	return -1;
}

void debugKeypadPrint()
{
	char lettersCol[4] = {
		GPIO_PinRead (ROW_PINS[0].Portnum, ROW_PINS[0].Pinnum)+ '0',
		GPIO_PinRead (ROW_PINS[1].Portnum, ROW_PINS[1].Pinnum)+ '0',
		GPIO_PinRead (ROW_PINS[2].Portnum, ROW_PINS[2].Pinnum)+ '0',
		GPIO_PinRead (ROW_PINS[3].Portnum, ROW_PINS[3].Pinnum)+ '0'};
	struct Frame letterFrameRow = {50, 50 + LETTER_WIDTH, 50, 50 + LETTER_HEIGHT};
	writeLetters(lettersCol, &letterFrameRow, 4);

	char lettersRow[4] = {
		GPIO_PinRead (COL_PINS[0].Portnum, COL_PINS[0].Pinnum)+ '0',
		GPIO_PinRead (COL_PINS[1].Portnum, COL_PINS[1].Pinnum)+ '0',
		GPIO_PinRead (COL_PINS[2].Portnum, COL_PINS[2].Pinnum)+ '0',
		GPIO_PinRead (COL_PINS[3].Portnum, COL_PINS[3].Pinnum)+ '0'};
		struct Frame letterFrameCol = {50, 50 + LETTER_WIDTH, 90, 90 + LETTER_HEIGHT};
	writeLetters(lettersRow, &letterFrameCol, 4);
}

void writeKeypadInput()
{		
	int keyPressed = keyboardScan();
	struct Frame keyFrame = {100, 100+LETTER_WIDTH, 100, 100+LETTER_HEIGHT};
	if(keyPressed != -1)
	{
		char symbol = KEYBOARD_MAP[keyPressed];
		drawLetter(&keyFrame, symbol);
		
		//debugKeypadPrint();
	}
}

//real time clock
void configure_lpc_rtc()
{
		LPC_RTC->CCR = 1; // clock control register, wlaczenie zegara
}

void writeYear(struct Frame* dateFrame)
{
	static const int numberOfValues = 5;
	int thousand = LPC_RTC->YEAR / 1000;
	int hundread = LPC_RTC->YEAR % 100;
	int tens = LPC_RTC->YEAR % 10;
	int ones = LPC_RTC->YEAR % 1;
	char dateValues[numberOfValues] = {
		 thousand + '0',
		 hundread + '0',
		 tens + '0',
		 ones + '0',
		 '.'
	};
	writeLetters(dateValues, dateFrame, numberOfValues);
	dateFrame->xStart += LETTER_WIDTH*(numberOfValues+1);
	dateFrame->xEnd += LETTER_WIDTH*(numberOfValues+1);
}

void writeMonth(struct Frame* dateFrame)
{
	static const int numberOfValues = 3;
	int tens = LPC_RTC->MONTH % 10;
	int ones = (LPC_RTC->MONTH - tens);
	char dateValues[numberOfValues] = {
		 tens + '0',
		 ones + '0',
		 '.'
	};
	writeLetters(dateValues, dateFrame, numberOfValues);
	dateFrame->xStart += LETTER_WIDTH*(numberOfValues+1);
	dateFrame->xEnd += LETTER_WIDTH*(numberOfValues+1);
}

void writeDay(struct Frame* dateFrame)
{
	static const int numberOfValues = 3;
	int tens = LPC_RTC->DOY / 10;
	int ones = LPC_RTC->DOY / 1;
	char dateValues[numberOfValues] = {
		 tens + '0',
		 ones + '0',
		 '.'
	};
	writeLetters(dateValues, dateFrame, numberOfValues);
	dateFrame->xStart += LETTER_WIDTH*(numberOfValues+1);
	dateFrame->xEnd += LETTER_WIDTH*(numberOfValues+1);
}

void writeHour(struct Frame* dateFrame)
{
	static const int numberOfValues = 3;
	int tens = LPC_RTC->HOUR / 10;
	int ones = LPC_RTC->HOUR / 1;
	char dateValues[numberOfValues] = {
		 tens + '0',
		 ones + '0',
		 '.'
	};
	writeLetters(dateValues, dateFrame, numberOfValues);
	dateFrame->xStart += LETTER_WIDTH*(numberOfValues+1);
	dateFrame->xEnd += LETTER_WIDTH*(numberOfValues+1);
}

void writeMinute(struct Frame* dateFrame)
{
	static const int numberOfValues = 3;
	int tens = LPC_RTC->MIN / 10;
	int ones = LPC_RTC->MIN / 1;
	char dateValues[numberOfValues] = {
		 tens + '0',
		 ones + '0',
		 '.'
	};
	writeLetters(dateValues, dateFrame, numberOfValues);
	dateFrame->xStart += LETTER_WIDTH*(numberOfValues+1);
	dateFrame->xEnd += LETTER_WIDTH*(numberOfValues+1);
}

void writeSec(struct Frame* dateFrame)
{
	static const int numberOfValues = 3;
	int tens = LPC_RTC->SEC / 10;
	int ones = LPC_RTC->SEC / 1;
	char dateValues[numberOfValues] = {
		 tens + '0',
		 ones + '0',
		 '.'
	};
	writeLetters(dateValues, dateFrame, numberOfValues);
	dateFrame->xStart += LETTER_WIDTH*(numberOfValues+1);
	dateFrame->xEnd += LETTER_WIDTH*(numberOfValues+1);
}

void writeClockDate()
{
	struct Frame dateFrame = {10, 10 + LETTER_WIDTH, 300, 300 + LETTER_HEIGHT};
	writeYear(&dateFrame);
	writeMonth(&dateFrame);
	writeDay(&dateFrame);
	writeHour(&dateFrame);
	writeMinute(&dateFrame);
	writeSec(&dateFrame);
}

int main()
{
	
	lcdConfiguration();
	init_ILI9325();
	gpioSetup();
	configure_lpc_rtc();
	
	clearScreen();

	 LPC_RTC->YEAR = 2023;
	 LPC_RTC->MONTH = 12;
	while(1)
	{
		writeKeypadInput();
		writeClockDate();
		clearScreen();
		
	}
		
	//struct Frame frame = {100, 200, 100, 250};
	//draw(&frame, LCDRed);
}

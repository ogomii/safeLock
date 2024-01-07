#include "LCD_ILI9325.h"
#include "Open1768_LCD.h"
#include "asciiLib.h"
#include <stdbool.h> 
#include "GPIO_LPC17xx.h"
#include <LPC17xx.h>
#include <PIN_LPC17xx.h>

#include <cmsis_os2.h>

#define MAX_COL_IDX 7
#define LETTER_HEIGHT 16
#define LETTER_WIDTH 8

static const PIN ROW_PINS[] = {
  {0U, 0U },
  {0U, 1U },
  {2U, 11U },
  {2U, 12U }
};

static const PIN COL_PINS[] = {
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

static const int KEYPAD_VALUES[] = {
	1, 2, 3, 0,
	4, 5, 6, 0,
	7, 8, 9, 0,
	0, 0, 0, 0
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
		PIN_Configure (ROW_PINS[n].Portnum, ROW_PINS[n].Pinnum, PIN_FUNC_0, PIN_PINMODE_PULLDOWN, PIN_PINMODE_NORMAL);
		GPIO_SetDir   (ROW_PINS[n].Portnum, ROW_PINS[n].Pinnum, GPIO_DIR_OUTPUT);
	}
	for(int n = 0; n < 4; n++)
	{
		PIN_Configure (COL_PINS[n].Portnum, COL_PINS[n].Pinnum, PIN_FUNC_0, PIN_PINMODE_PULLDOWN, PIN_PINMODE_NORMAL);
		GPIO_SetDir   (COL_PINS[n].Portnum, COL_PINS[n].Pinnum, GPIO_DIR_INPUT);
	}
}

int keyboardScan()
{
	for(int row = 0; row < 4; row++)
	{
		for(int i = 0; i < 4; i++) GPIO_PinWrite (ROW_PINS[i].Portnum, ROW_PINS[i].Pinnum, 0U);
		
		GPIO_PinWrite (ROW_PINS[row].Portnum, ROW_PINS[row].Pinnum, 1U);
		for(int col = 0; col < 4; col++)
		{
			if(GPIO_PinRead (COL_PINS[col].Portnum, COL_PINS[col].Pinnum) == 1)
				return row * 4 + col;
		}
	}
	return -1;
}

void debugKeypadPrint()
{
	char lettersCol[4] = {
		GPIO_PinRead (COL_PINS[0].Portnum, COL_PINS[0].Pinnum)+ '0',
		GPIO_PinRead (COL_PINS[1].Portnum, COL_PINS[1].Pinnum)+ '0',
		GPIO_PinRead (COL_PINS[2].Portnum, COL_PINS[2].Pinnum)+ '0',
		GPIO_PinRead (COL_PINS[3].Portnum, COL_PINS[3].Pinnum)+ '0'};
	struct Frame letterFrameRow = {50, 50 + LETTER_WIDTH, 50, 50 + LETTER_HEIGHT};
	writeLetters(lettersCol, &letterFrameRow, 4);

	char lettersRow[4] = {
		GPIO_PinRead (ROW_PINS[0].Portnum, ROW_PINS[0].Pinnum)+ '0',
		GPIO_PinRead (ROW_PINS[1].Portnum, ROW_PINS[1].Pinnum)+ '0',
		GPIO_PinRead (ROW_PINS[2].Portnum, ROW_PINS[2].Pinnum)+ '0',
		GPIO_PinRead (ROW_PINS[3].Portnum, ROW_PINS[3].Pinnum)+ '0'};
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
		
		
	}
	debugKeypadPrint();
}

//real time clock
void configure_lpc_rtc()
{
		LPC_RTC->CCR = 1; // clock control register, wlaczenie zegara
}

void writeYear(struct Frame* dateFrame)
{
	static const int numberOfValues = 5;
	int year = LPC_RTC->YEAR;
	int ones = year % 10;
	year /= 10;
	int tens = year % 10;
	year /= 10;
	int hundread = year % 10;
	year /= 10;
	int thousand = year;
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
	int value = LPC_RTC->MONTH;
	int ones = value % 10;
	value /= 10;
	int tens = value % 10;
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
	int value = LPC_RTC->DOY;
	int ones = value % 10;
	value /= 10;
	int tens = value % 10;
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
	int value = LPC_RTC->HOUR;
	int ones = value % 10;
	value /= 10;
	int tens = value % 10;
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
	int value = LPC_RTC->MIN;
	int ones = value % 10;
	value /= 10;
	int tens = value % 10;
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
	int value = LPC_RTC->SEC;
	int ones = value % 10;
	value /= 10;
	int tens = value % 10;
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

bool saveDateValue(int* dateArray)
{
	static int dateInputCounter = 0;
	static int lastSavedValue = -1;
	int keyPressed = -1;
	struct Frame keyFrame = {100, 100+LETTER_WIDTH, 100, 100+LETTER_HEIGHT};
	do
	{
		keyPressed = keyboardScan();
	} while (keyPressed == -1);
	char symbol = KEYBOARD_MAP[keyPressed];
	drawLetter(&keyFrame, symbol);
	clearScreen();
	
	if(lastSavedValue != KEYPAD_VALUES[keyPressed])
	{
		dateArray[dateInputCounter] = KEYPAD_VALUES[keyPressed];
		dateInputCounter += 1;
		lastSavedValue = KEYPAD_VALUES[keyPressed];
	}

	return (dateInputCounter >= 14);
}

void getDate(int* dateArray)
{
	bool dateCollected = false;
	do
	{
		dateCollected = saveDateValue(dateArray);
	} while (dateCollected == false);
}

void saveDate(int* dateArray)
{
	int arrIndex = 0;
	int year = dateArray[arrIndex] * 1000; arrIndex++;
	year += dateArray[arrIndex] * 100; arrIndex++;
	year += dateArray[arrIndex] * 10; arrIndex++;
	year += dateArray[arrIndex]; arrIndex++;
	LPC_RTC->YEAR = year;

	int month = dateArray[arrIndex] * 10; arrIndex++;
	month += dateArray[arrIndex]; arrIndex++;
	LPC_RTC->MONTH = month;

	int day = dateArray[arrIndex] * 10; arrIndex++;
	day += dateArray[arrIndex]; arrIndex++;
	LPC_RTC->DOY = day;

	int hour = dateArray[arrIndex] * 10; arrIndex++;
	hour += dateArray[arrIndex]; arrIndex++;
	LPC_RTC->HOUR = hour;
	
	int min = dateArray[arrIndex] * 10; arrIndex++;
	min += dateArray[arrIndex]; arrIndex++;
	LPC_RTC->MIN = min;
	
	int sec = dateArray[arrIndex] * 10; arrIndex++;
	sec += dateArray[arrIndex]; arrIndex++;
	LPC_RTC->SEC = sec;
}

void setDate()
{
	int dateArray[14];
	getDate(dateArray);
	saveDate(dateArray);
}


void app_main (void *argument) {
	clearScreen();
	//setDate();

	while(1)
	{
		writeKeypadInput();
		writeClockDate();
		osDelay(100);
		clearScreen();
	}
}

int main()
{
	lcdConfiguration();
	init_ILI9325();
	gpioSetup();
	configure_lpc_rtc();
	osKernelInitialize();
	osThreadNew(app_main, NULL, NULL);

	if (osKernelGetState() == osKernelReady){
    osKernelStart();                    								// Start thread execution
  }
		
	//struct Frame frame = {100, 200, 100, 250};
	//draw(&frame, LCDRed);
}

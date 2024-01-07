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

enum lock_state{
	LOCKED,
	UNLOCKED,
	NEW_CODE
};
osTimerId_t timer0;
enum lock_state LOCK_STATE = LOCKED;

#define CODE_LEN 4
int ENTERED_CODE[CODE_LEN] = {-1, -1, -1, -1};
int codeInputCounter = 0;

int PASSCODE[CODE_LEN] = {1,2,3,4};
int passcodeInputCounter = 0;

const int CodeXPos = LCD_MAX_X / 2 - (2 * LETTER_WIDTH);
const int CodeYPos = LCD_MAX_Y / 2 - LETTER_HEIGHT;  // 1 over state

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

static const PIN LED_PIN[] = {
  {0U, 3U },
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

static const int CHAR[] = {'0','1','2','3','4','5','6','7','8','9'};

struct Frame{
	uint16_t xStart;
	uint16_t xEnd;
	uint16_t yStart;
	uint16_t yEnd;
};

struct Date{
	int year;
	int month;
	int day;
	int hour;
	int min;
	int sec;
};

struct Date LAST_STATE_CHANGE = {0, 0, 0, 0, 0, 0};

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
	
	PIN_Configure (LED_PIN[0].Portnum, LED_PIN[0].Pinnum, PIN_FUNC_0, PIN_PINMODE_PULLDOWN, PIN_PINMODE_NORMAL);
	GPIO_SetDir   (LED_PIN[0].Portnum, LED_PIN[0].Pinnum, GPIO_DIR_OUTPUT);
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


bool isCodeOk()
{
	for(int digit = 0; digit < CODE_LEN; digit++)
	{
		if(ENTERED_CODE[digit] != PASSCODE[digit])
		{
			return false;
		}
	}
	return  true;
}

void resetEnteredCode()
{
	ENTERED_CODE[0] = -1;
	ENTERED_CODE[1] = -1;
	ENTERED_CODE[2] = -1;
	ENTERED_CODE[3] = -1;
	codeInputCounter = 0;
}

void resetPasscode()
{
	PASSCODE[0] = -1;
	PASSCODE[1] = -1;
	PASSCODE[2] = -1;
	PASSCODE[3] = -1;
	passcodeInputCounter = 0;
}

void callback(void *param){
	LOCK_STATE = LOCKED;
}

void checkCode()
{
	if(isCodeOk())
	{
		LOCK_STATE = UNLOCKED;
		timer0 = osTimerNew(&callback, osTimerOnce,(void *)0, NULL);
		osTimerStart(timer0, 10000);
	}
	resetEnteredCode();
}

void saveCode(int keyPressed)
{
		if( codeInputCounter == 0)
		{
			ENTERED_CODE[codeInputCounter] = KEYPAD_VALUES[keyPressed];
			codeInputCounter += 1;
		}
		else if(ENTERED_CODE[codeInputCounter-1] != KEYPAD_VALUES[keyPressed])
		{
			ENTERED_CODE[codeInputCounter] = KEYPAD_VALUES[keyPressed];
			codeInputCounter += 1;
		}
}

void saveNewCode(int keyPressed)
{
		if( passcodeInputCounter == 0)
		{
			PASSCODE[passcodeInputCounter] = KEYPAD_VALUES[keyPressed];
			passcodeInputCounter += 1;
		}
		else if(PASSCODE[passcodeInputCounter-1] != KEYPAD_VALUES[keyPressed])
		{
			PASSCODE[passcodeInputCounter] = KEYPAD_VALUES[keyPressed];
			passcodeInputCounter += 1;
		}
}

void writeKeypadInput()
{		
	int keyPressed = keyboardScan();
	struct Frame keyFrame = {LCD_MAX_X / 2, LCD_MAX_X / 2 +LETTER_WIDTH, CodeYPos - (2*LETTER_HEIGHT), CodeYPos - LETTER_HEIGHT};
	if(keyPressed != -1)
	{
		char symbol = KEYBOARD_MAP[keyPressed];
		drawLetter(&keyFrame, symbol);
		
		if(LOCK_STATE == LOCKED)
		{
			saveCode(keyPressed);
			if(codeInputCounter >= CODE_LEN)
			{
				checkCode();
			}
		}
		else if(LOCK_STATE == NEW_CODE)
		{
			saveNewCode(keyPressed);
			if(passcodeInputCounter >= CODE_LEN)
			{
				LOCK_STATE = LOCKED;
			}
		}
	}
	//debugKeypadPrint();
}

void writeLockState()
{
	static char lettersRow[3][8] = {{'L','O','C','K','E','D',' ',' '},
																	{'U','N','L','O','C','K','E','D'},
																	{'N','E','W',' ','C','O','D','E'},};
	static int xPos = LCD_MAX_X / 2 - (4 * LETTER_WIDTH);
	static int yPos = LCD_MAX_Y / 2;
	struct Frame letterFrameCol = {xPos, xPos + LETTER_WIDTH, yPos, yPos + LETTER_HEIGHT};
	writeLetters(lettersRow[LOCK_STATE], &letterFrameCol, 8);
}

void writeEnteredCode()
{
	struct Frame keyFrame = {CodeXPos, CodeXPos+LETTER_WIDTH, CodeYPos - LETTER_HEIGHT, CodeYPos};
	for(int digit = 0; digit < CODE_LEN; digit++)
	{
		if(LOCK_STATE == LOCKED)
		{
			if(ENTERED_CODE[digit] == -1)
			{
				break;
			}
			else
			{
				drawLetter(&keyFrame, CHAR[ENTERED_CODE[digit]]);
				keyFrame.xStart += 10;
				keyFrame.xEnd += 10;
			}
		}
		else if(LOCK_STATE == NEW_CODE)
		{
			if(PASSCODE[digit] == -1)
			{
				break;
			}
			else
			{
				drawLetter(&keyFrame, CHAR[PASSCODE[digit]]);
				keyFrame.xStart += 10;
				keyFrame.xEnd += 10;
			}
		}
	}
}

//real time clock
void configure_lpc_rtc()
{
		LPC_RTC->CCR = 1; // clock control register, wlaczenie zegara
}

void writeYear(struct Frame* dateFrame, int inputYear)
{
	static const int numberOfValues = 5;
	int year = inputYear;
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

void writeMonth(struct Frame* dateFrame, int inputMon)
{
	static const int numberOfValues = 3;
	int value = inputMon;
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

void writeDay(struct Frame* dateFrame, int inputDay)
{
	static const int numberOfValues = 3;
	int value = inputDay;
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

void writeHour(struct Frame* dateFrame, int inputHour)
{
	static const int numberOfValues = 3;
	int value = inputHour;
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

void writeMinute(struct Frame* dateFrame, int inputMin)
{
	static const int numberOfValues = 3;
	int value = inputMin;
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

void writeSec(struct Frame* dateFrame, int inputSec)
{
	static const int numberOfValues = 3;
	int value = inputSec;
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
	struct Frame letterFrame = {10, 10 + LETTER_WIDTH, 280, 280 + LETTER_HEIGHT};
	const char letters[12] = {'C','U','R','R','E','N','T',' ','D','A','T','E'};
	writeLetters(letters, &letterFrame, 12);
	struct Frame dateFrame = {10, 10 + LETTER_WIDTH, 300, 300 + LETTER_HEIGHT};
	writeYear(&dateFrame, LPC_RTC->YEAR);
	writeMonth(&dateFrame, LPC_RTC->MONTH);
	writeDay(&dateFrame, LPC_RTC->DOY);
	writeHour(&dateFrame, LPC_RTC->HOUR);
	writeMinute(&dateFrame, LPC_RTC->MIN);
	writeSec(&dateFrame, LPC_RTC->SEC);
}

void writeDateTypeToSeve(int dateInputCounter)
{
	int dateTypeIndex = dateInputCounter / 2;
	static char lettersRow[7][4] = {{'Y','E','A','R'},
																	{'Y','E','A','R'},
																	{'M','O','N',' '},
																	{'D','A','Y',' '},
																	{'H','O','U','R'},
																	{'M','I','N',' '},
																	{'S','E','C',' '}};
	
	struct Frame letterFrameCol = {70, 70 + LETTER_WIDTH, 70, 70 + LETTER_HEIGHT};
	writeLetters(lettersRow[dateTypeIndex], &letterFrameCol, 4);
}

bool saveDateValue(int* dateArray)
{
	static int dateInputCounter = 0;
	static int lastSavedValue = -1;
	int keyPressed = -1;
	
	writeDateTypeToSeve(dateInputCounter);
	
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
	osDelay(300);
}

void checkDButton()
{
	int keyPressed = keyboardScan();
	if(keyPressed == 15)
	{
		LOCK_STATE = NEW_CODE;
		resetPasscode();
		osDelay(300);
	}
}


void udpdateLastStateChangeDate()
{
	LAST_STATE_CHANGE.year = LPC_RTC->YEAR;
	LAST_STATE_CHANGE.month = LPC_RTC->MONTH;
	LAST_STATE_CHANGE.day = LPC_RTC->DOY;
	LAST_STATE_CHANGE.hour = LPC_RTC->HOUR;
	LAST_STATE_CHANGE.min = LPC_RTC->MIN;
	LAST_STATE_CHANGE.sec = LPC_RTC->SEC;
}

void writeLastStateChangeDate()
{
	struct Frame letterFrame = {10, 10 + LETTER_WIDTH, 230, 230 + LETTER_HEIGHT};
	const char letters[17] = {'L','A','S','T',' ','S','T','A','T','E',' ','C','H','A','N','G','E'};
	writeLetters(letters, &letterFrame, 17);
	struct Frame dateFrame = {10, 10 + LETTER_WIDTH, 250, 250 + LETTER_HEIGHT};
	writeYear(&dateFrame, LAST_STATE_CHANGE.year);
	writeMonth(&dateFrame, LAST_STATE_CHANGE.month);
	writeDay(&dateFrame, LAST_STATE_CHANGE.day);
	writeHour(&dateFrame, LAST_STATE_CHANGE.hour);
	writeMinute(&dateFrame, LAST_STATE_CHANGE.min);
	writeSec(&dateFrame, LAST_STATE_CHANGE.sec);
}

void writeLastStateChange()
{
	static enum lock_state lasteKnownState = NEW_CODE;
	if(LOCK_STATE != lasteKnownState)
	{
		lasteKnownState = LOCK_STATE;
		udpdateLastStateChangeDate();
	}
	
	writeLastStateChangeDate();
}

void lightLed()
{
	if(LOCK_STATE == UNLOCKED)
	{
		GPIO_PinWrite (LED_PIN[0].Portnum, LED_PIN[0].Pinnum, 0U);
	}
	else
	{
		GPIO_PinWrite (LED_PIN[0].Portnum, LED_PIN[0].Pinnum, 1U);
	}
}

void app_main (void *argument) {
	clearScreen();
	setDate();

	while(1)
	{
		checkDButton();
		writeKeypadInput();
		writeEnteredCode();
		writeLockState();
		writeLastStateChange();
		writeClockDate();
		lightLed();
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
		
}

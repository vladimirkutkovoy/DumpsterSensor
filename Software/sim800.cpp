#define _CRT_SECURE_NO_WARNINGS

#include <iostream>
#include <algorithm>
using std::cout;

// BLOCK - блокировка процесса поиска информации
// ERROR - ошибка парсинга
// BEGIN - начало поиска строки
// HEADER - парсинг заголовка
// READ - чтение информации
// MESSAGE - копирование смс в переменную
// RESULT - данные готовы забирай быстрее

enum findStates { BLOCK = -1, ERROR, BEGIN, HEADER, STATUS, READ, MESSAGE, RESULT, ANS };
enum answerStates { NONE, WAIT };
enum parsingStates { DATA, END = 9 };
enum resultStates { UNDEFINED, SMS, AT_ANS_OK, AT_ANS_ERROR, AT_WITHPARAM };

//enum ATStates { AT_BEGIN, AT_HEADER, AT_DATA, AT_OKrn };
//int ATState = 0;

int findState = BEGIN;
bool answerState = NONE;
int parsingState = DATA;
int resultState = UNDEFINED;

int part = 0;

char tmpBuffer[10];
int tmpLength = 0;
char answerStr[20];
int answerLen = 0;
char resultStr[50];

struct messageData
{
	char phoneNumberStr[12]; // Номер телефона
	char dateStr[30]; // Дата и время
	char messageStr[100]; // Сообщение
} myMessageData;

struct resCode
{
	const char* str;
	const short length;
	const short state;
};

const resCode urc[] = { {"CMT", 3, READ}, {"CLIP", 4} };
const int urcSize = sizeof(urc) / sizeof(resCode);

const resCode src[] = { {"AT", 2, ANS}, {"CMGF", 4, ANS}, {"CPOF", 4, ANS} };
const int srcSize = sizeof(src) / sizeof(resCode);

const resCode status[] = { {"OK", 2, RESULT}, {"ERROR", 5, RESULT} };
const int statusSize = sizeof(status) / sizeof(resCode);

char* dataPtr[13] = { myMessageData.phoneNumberStr, NULL, myMessageData.dateStr, NULL, NULL, NULL, NULL, NULL, NULL, tmpBuffer, myMessageData.messageStr };
int end_indexes[11] = { 11, 1, 31, 1, 1, 1, 1, 1, 1, 3, 101 };
bool mark = false; // true - кавычки открылись, false - кавычки закрылись
int messageLen = 0; // Длина получаемого смс
int errorType = 0; // Тип выдаваемой ошибки
void(*callBack)(); // Указатель на функцию пользователя

bool error();
bool readHeader();
bool readStatus();
bool readSMS();
bool message();
bool result();
//bool at();
//bool AT_OK();
//bool AT_CMGF_OK();
//bool AT_CMGF_HEADER();
bool readAT();
int str2int(const char* str, int length, int base);

// DATA, END = 9, MESSAGE, RESULT, ERROR

bool(*parsingPtr[])() = { error, readHeader, readHeader, readStatus, readSMS, message, result, readAT, readHeader };

char* gprsBuffer;
char* gprsBufferEnd;

// +CMTI:<mem>,<index>
// +CMT: "+7926...", , "2018/04/27,13:17:17+03", 145, 17, 0, 0, "+79262909090", 145, 8\r\n
// Privet\r\n
bool reader(char* gprsBuf, int len)
{
	if (findState == BLOCK)
		return false;

	gprsBuffer = gprsBuf;
	gprsBufferEnd = gprsBuffer + len;

	while ((*parsingPtr[findState])());

	return true;
}

bool readHeader()
{
	while (gprsBuffer < gprsBufferEnd)
	{
		char cur = *gprsBuffer++;

		if (cur == '\r' || cur == '\n')
			continue;

		if (cur == '+')
		{
			findState = HEADER;
			continue;
		}

		if (findState == BEGIN)
		{
			findState = STATUS;
			gprsBuffer--;

			if (answerState == NONE)
			{
				gprsBuffer = gprsBufferEnd;
				findState = ERROR;
				errorType = 2;
			}
			return true;
		}

		if (tmpLength > 5)
		{
			gprsBuffer = gprsBufferEnd;
			findState = ERROR;
			errorType = 0;
			return true;
		}

		if (cur != ':')
		{
			tmpBuffer[tmpLength++] = cur;
			continue;
		}

		// URC (незапрашиваемые)
		for (int i = 0; i < urcSize; i++)
			if ((tmpLength == urc[i].length) && strncmp(tmpBuffer, urc[i].str, urc[i].length) == 0)
			{
				tmpLength = 0;
				findState = urc[i].state;
				return true;
			}

		// SRC (запрашиваемые)
		for (int i = 0; i < srcSize; i++)
			if ((tmpLength == src[i].length) && strncmp(tmpBuffer, src[i].str, src[i].length) == 0)
			{
				tmpLength = 0;
				findState = src[i].state;
				return true;
			}
	}

	return (gprsBuffer < gprsBufferEnd);
}

bool readStatus()
{
	while (gprsBuffer < gprsBufferEnd)
	{
		if (tmpLength > 5)
		{
			gprsBuffer = gprsBufferEnd;
			findState = ERROR;
			errorType = 0;
			return true;
		}

		tmpBuffer[tmpLength++] = *gprsBuffer++;

		for (int i = 0; i < statusSize; i++)
			if ((tmpLength == status[i].length) && strncmp(tmpBuffer, status[i].str, status[i].length) == 0)
			{
				tmpLength = 0;
				findState = RESULT;

				if (resultState == UNDEFINED)
					resultState = i + 2;

				answerState = NONE;
				return true;
			}
	}

	return (gprsBuffer < gprsBufferEnd);
}

bool readSMS()
{
	while (gprsBuffer < gprsBufferEnd)
	{
		char cur = *gprsBuffer++;

		if (parsingState == END)
			if (cur == '\n')
			{
				if (tmpBuffer[tmpLength - 1] == '\r')
				{
					messageLen = str2int(tmpBuffer, tmpLength, 10);

					if (messageLen > 100)
					{
						gprsBuffer = gprsBufferEnd;
						findState = ERROR;
						errorType = 1;
						return true;
					}

					parsingState = DATA;
					findState = MESSAGE;
					tmpLength = 0;
					break;
				}
				else
				{
					gprsBuffer = gprsBufferEnd;
					findState = ERROR;
					errorType = 2;
					return true;
				}
			}

		// Открытие кавычек
		if (cur == '"')
			mark = !mark;

		if (tmpLength > end_indexes[parsingState])
		{
			findState = ERROR;
			gprsBuffer = gprsBufferEnd;
			errorType = 0;
			return true;
		}

		if (cur != ',' || mark == true)
		{
			// Копим данные пока нет ',' или открыта кавычка
			if (cur != '\\' && cur != '"')
				if (dataPtr[parsingState])
					dataPtr[parsingState][tmpLength++] = cur;
			continue;
		}

		if (dataPtr[parsingState])
			dataPtr[parsingState][tmpLength++] = '\0';

		parsingState++;
		tmpLength = 0;
	}

	return (gprsBuffer < gprsBufferEnd);
}

bool message()
{
	int mLen = std::min(int(gprsBufferEnd - gprsBuffer), std::min(messageLen, messageLen - tmpLength));
	memcpy(myMessageData.messageStr + tmpLength, gprsBuffer, mLen);
	gprsBuffer += mLen;
	tmpLength += mLen;

	if (tmpLength == messageLen)
	{
		tmpLength = 0;

		if ((myMessageData.messageStr[messageLen - 2] == '\r') && (myMessageData.messageStr[messageLen - 1] == '\n'))
		{
			findState = RESULT;
			resultState = SMS;
		}
		else
			findState = ERROR;

		return true;
	}

	return false;
}

//\r\n
//+ CMGW: ИНДЕКС\r\n
//\r\n
//OK\r\n
bool readAT()
{
	if (answerState == NONE)
	{
		findState = ERROR;
		return true;
	}

	int index = strcspn(gprsBuffer, "\n");
	int len = std::min(int(gprsBufferEnd - gprsBuffer), index + 1);

	if (len > 50)
	{
		gprsBuffer = gprsBufferEnd;
		findState = ERROR;
		errorType = 3;
		return true;
	}

	memcpy(resultStr, gprsBuffer, len);

	gprsBuffer += len;
	tmpLength += len;

	if (index != (gprsBufferEnd - gprsBuffer))
		if ((resultStr[len - 2] == '\r') && (resultStr[len - 1] == '\n'))
		{
			tmpLength = 0;
			findState = RESULT;
			resultState = AT_WITHPARAM;
		}
		else
			findState = ERROR;

	return true;
}

bool error()
{
	findState = BLOCK;

	switch (errorType)
	{
	case 0:
		cout << "Array index out of range" << '\n';
		break;

	case 1:
		cout << "Message error" << '\n';
		break;

	case 2:
		cout << "Protocol error" << '\n';
		break;

	case 3:
		cout << "AT Param error" << '\n';
		break;

	default:
		break;
	}

	return false;
}

bool result()
{
	parsingState = DATA;
	findState = BEGIN;
	callBack();

	resultState = UNDEFINED;
	return true;
}

void getSMSData(messageData& out)
{
	out = myMessageData;
}

int str2int(const char* str, int length, int base)
{
	int ret = 0;

	for (int i = 0; i < length; i++)
	{
		int tst;

		if ((str[i] >= '0') && (str[i] <= '9'))
			tst = (str[i] - '0');

		else if ((str[i] >= 'A') && (str[i] <= 'Z'))
			tst = (str[i] - 'A' + 10);

		else if ((str[i] >= 'a') && (str[i] <= 'z'))
			tst = (str[i] - 'a' + 10);

		else break;

		if (tst >= base) break;

		ret = ret * base + tst;
	}

	return ret;
}

messageData tmp = { "", "", "" };

void userCallBack()
{
	if (resultState == SMS)
		getSMSData(tmp);
	if (resultState == AT_WITHPARAM)
		cout << "OK, Param:" << resultStr << '\n';
	if (resultState == AT_ANS_OK)
		cout << "OK" << '\n';
	if (resultState == AT_ANS_ERROR)
		cout << "ERROR" << '\n';
}

int main()
{
	callBack = userCallBack;

	char str0[] = "+CMT:\"+7926290909\",,\"2018";
	char str1[] = "/04/27,13:17:17+03\",145,17,0";
	char str2[] = ",0,\"+79262909090\",145,8\r";
	char str3[] = "\nPr\"vet\r\n+CMT:\"+1234567890\",,\"";
	char str4[] = "2018/04/27,13:15:17+03\",145,17,0,0,\"+79262909090\",145,8\r\nhel";
	char str5[] = "lo!\r\n";

	bool state = false;

	state = reader(str0, strlen(str0));
	state = reader(str1, strlen(str1));
	state = reader(str2, strlen(str2));
	state = reader(str3, strlen(str3));
	state = reader(str4, strlen(str4));
	state = reader(str5, strlen(str5));

	char str6[] = "\r\n+CMGF:1\r\n+CMT:\"+7926290909\",,\"2018/04/27,13:17:17+03\",145,17,0,0,\"+79262909090\",145,8\r\nPrivet\r\n\r\nERROR\r\n";
	answerState = WAIT;

	state = reader(str6, strlen(str6));

	return 0;
}
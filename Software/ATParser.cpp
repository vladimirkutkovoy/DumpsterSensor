#define _CRT_SECURE_NO_WARNINGS

#include <iostream>
#include "ATParser.h"
using std::cout;

ATParser::ATParser(const ResCode* urcInit, const int* urcSizeInit, const ResCode* srcInit, const int* srcSizeInit,
	const ResCode* statusInit, const int* statusSizeInit, char* tmpBufferInit, const int* tmpBufferSizeInit,
	char* resultStrInit, const int* resultStrSizeInit) // , bool (ATParser::** parsPtrInit)()
{
	ATParser_urc = urcInit;
	ATParser_src = srcInit;
	ATParser_urcSize = urcSizeInit;
	ATParser_srcSize = srcSizeInit;
	ATParser_status = statusInit;
	ATParser_statusSize = statusSizeInit;
	ATParser_tmpBuffer = tmpBufferInit;
	ATParser_tmpBufferSize = tmpBufferSizeInit;
	ATParser_resultStr = resultStrInit;
	ATParser_resultStrSize = resultStrSizeInit;
	//ATParser_parsingUserPtr = parsPtrInit;
}

ATParser::~ATParser()
{
}

bool ATParser::reader(char* gprsBuf, int len)
{
	if (ATParser_findState == BLOCK)
		return false;

	ATParser_gprsBuffer = gprsBuf;
	ATParser_gprsBufferEnd = ATParser_gprsBuffer + len;

	//while (ATParser_findState > ANS ? (this->*ATParser_parsingUserPtr[ATParser_findState - USER])() : (this->*ATParser_parsingPtr[ATParser_findState])());

	while (ATParser_findState > ANS ? (this->*ATParser_parsingUserPtr)() : (this->*ATParser_parsingPtr[ATParser_findState])());

	return true;
}

bool ATParser::readHeader()
{
	while (ATParser_gprsBuffer < ATParser_gprsBufferEnd)
	{
		char cur = *ATParser_gprsBuffer++;

		if (cur == '\r' || cur == '\n')
			continue;

		if (cur == '+')
		{
			ATParser_findState = HEADER;
			continue;
		}

		if (ATParser_findState == BEGIN)
		{
			ATParser_findState = STATUS;
			ATParser_gprsBuffer--;

			if (!ATParser_answerWaitState)
			{
				ATParser_gprsBuffer = ATParser_gprsBufferEnd;
				ATParser_findState = ERROR;
				ATParser_errorType = 2;
			}
			return true;
		}

		if (ATParser_tmpLength > *ATParser_tmpBufferSize + 1)
		{
			ATParser_gprsBuffer = ATParser_gprsBufferEnd;
			ATParser_findState = ERROR;
			ATParser_errorType = 0;
			return true;
		}

		if (cur != ' ')
		{
			ATParser_tmpBuffer[ATParser_tmpLength++] = cur;
			continue;
		}

		if (ATParser_tmpBuffer[ATParser_tmpLength - 1] == ':')
			ATParser_tmpLength--;
		else
		{
			ATParser_gprsBuffer = ATParser_gprsBufferEnd;
			ATParser_findState = ERROR;
			ATParser_errorType = 2;
			return true;
		}

		// URC (незапрашиваемые)
		for (int i = 0; i < *ATParser_urcSize; i++)
			if ((ATParser_tmpLength == ATParser_urc[i].length) && strncmp(ATParser_tmpBuffer, ATParser_urc[i].str, ATParser_urc[i].length) == 0)
			{
				ATParser_tmpLength = 0;
				// ATParser_findState = ATParser_urc[i].state;
				ATParser_parsingUserPtr = ATParser_urc[i].handler;
				ATParser_findState = USER; // функция от пользователя
				return true;
			}

		// SRC (запрашиваемые)
		for (int i = 0; i < *ATParser_srcSize; i++)
			if ((ATParser_tmpLength == ATParser_src[i].length) && strncmp(ATParser_tmpBuffer, ATParser_src[i].str, ATParser_src[i].length) == 0)
			{
				ATParser_tmpLength = 0;
				// ATParser_findState = ATParser_src[i].state;
				// ATParser_parsingUserPtr = ATParser_src[i].handler;
				ATParser_findState = ANS; // readAT
				return true;
			}
	}

	return (ATParser_gprsBuffer < ATParser_gprsBufferEnd);
}

bool ATParser::readStatus()
{
	while (ATParser_gprsBuffer < ATParser_gprsBufferEnd)
	{
		if (ATParser_tmpLength > *ATParser_tmpBufferSize)
		{
			ATParser_gprsBuffer = ATParser_gprsBufferEnd;
			ATParser_findState = ERROR;
			ATParser_errorType = 0;
			return true;
		}

		ATParser_tmpBuffer[ATParser_tmpLength++] = *ATParser_gprsBuffer++;

		for (int i = 0; i < *ATParser_statusSize; i++)
			if ((ATParser_tmpLength == ATParser_status[i].length) && strncmp(ATParser_tmpBuffer, ATParser_status[i].str, ATParser_status[i].length) == 0)
			{
				ATParser_tmpLength = 0;
				ATParser_findState = RESULT;

				if (ATParser_resultState == 0)
					ATParser_resultState = i + 1;

				ATParser_answerWaitState = false;
				return true;
			}
	}

	return (ATParser_gprsBuffer < ATParser_gprsBufferEnd);
}

//\r\n
//+ CMGW: ИНДЕКС\r\n
//\r\n
//OK\r\n
bool ATParser::readAT()
{
	if (!ATParser_answerWaitState)
	{
		ATParser_findState = ERROR;
		return true;
	}

	int index = strcspn(ATParser_gprsBuffer, "\n");
	int len = std::min(int(ATParser_gprsBufferEnd - ATParser_gprsBuffer), index + 1);

	if (len > *ATParser_resultStrSize)
	{
		ATParser_gprsBuffer = ATParser_gprsBufferEnd;
		ATParser_findState = ERROR;
		ATParser_errorType = 3;
		return true;
	}

	memcpy(ATParser_resultStr, ATParser_gprsBuffer, len);

	ATParser_gprsBuffer += len;
	ATParser_tmpLength += len;

	if (index != (ATParser_gprsBufferEnd - ATParser_gprsBuffer))
		if ((ATParser_resultStr[len - 2] == '\r') && (ATParser_resultStr[len - 1] == '\n'))
		{
			ATParser_tmpLength = 0;
			ATParser_findState = RESULT;
			ATParser_resultState = 3;
		}
		else
			ATParser_findState = ERROR;

	return true;
}

bool ATParser::error()
{
	ATParser_findState = BLOCK;

	switch (ATParser_errorType)
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

bool ATParser::result()
{
	ATParser_findState = BEGIN;

	if (ATParser_callBack != NULL)
		ATParser_callBack();

	ATParser_resultState = 0;
	return true;
}

int ATParser::str2int(const char* str, int length, int base)
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
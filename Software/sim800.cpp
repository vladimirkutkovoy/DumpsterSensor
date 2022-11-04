#include "SIM800.h"
#include <string.h>

#include <iostream>
using std::cout;

bool SIM800::readSMS()
{
	while (ATParser_gprsBuffer < ATParser_gprsBufferEnd)
	{
		char cur = *ATParser_gprsBuffer++;

		if ((SIM800_parsingState == END) && (cur == '\r'))
		{
			SIM800_messageDataPtr[SIM800_parsingState][ATParser_tmpLength++] = '\0';
			SIM800_parsingState++;
			continue;
		}

		if (SIM800_parsingState == END + 1)
		{
			SIM800_parsingState = DATA;
			if (cur == '\n')
			{
				SIM800_messageLen = str2int(SIM800_tmpBuffer, ATParser_tmpLength, 10);

				if (SIM800_messageLen > SIM800_messageEndInd[END + 1])
				{
					ATParser_gprsBuffer = ATParser_gprsBufferEnd;
					ATParser_findState = ERROR;
					ATParser_errorType = 1;
					return true;
				}

				ATParser_parsingUserPtr = (bool (ATParser::*)())(&SIM800::message);
				ATParser_tmpLength = 0;

				return true;
			}

			ATParser_gprsBuffer = ATParser_gprsBufferEnd;
			ATParser_findState = ERROR;
			ATParser_errorType = 2;

			return true;
		}

		// Открытие кавычек
		if (cur == '"')
			ATParser_mark = !ATParser_mark;

		if (ATParser_tmpLength >= SIM800_messageEndInd[SIM800_parsingState])
		{
			ATParser_findState = ERROR;
			ATParser_gprsBuffer = ATParser_gprsBufferEnd;
			ATParser_errorType = 0;
			return true;
		}

		if (cur != ',' || ATParser_mark == true)
		{
			// Копим данные пока нет ',' или открыта кавычка
			if (cur != '\\' && cur != '"')
				if (SIM800_messageDataPtr[SIM800_parsingState])
					SIM800_messageDataPtr[SIM800_parsingState][ATParser_tmpLength++] = cur;
			continue;
		}

		if (SIM800_messageDataPtr[SIM800_parsingState])
			SIM800_messageDataPtr[SIM800_parsingState][ATParser_tmpLength] = '\0';

		SIM800_parsingState++;
		ATParser_tmpLength = 0;
	}

	return (ATParser_gprsBuffer < ATParser_gprsBufferEnd);
}

bool SIM800::message()
{
	int mLen = std::min(int(ATParser_gprsBufferEnd - ATParser_gprsBuffer), std::min(SIM800_messageLen, SIM800_messageLen - ATParser_tmpLength));
	memcpy(SIM800_myMessageData.messageStr + ATParser_tmpLength, ATParser_gprsBuffer, mLen);
	ATParser_gprsBuffer += mLen;
	ATParser_tmpLength += mLen;

	if (ATParser_tmpLength == SIM800_messageLen)
	{
		ATParser_tmpLength = 0;

		if ((SIM800_myMessageData.messageStr[SIM800_messageLen - 2] == '\r') && (SIM800_myMessageData.messageStr[SIM800_messageLen - 1] == '\n'))
		{
			ATParser_findState = RESULT;
			ATParser_resultState = (int)SIM800_RESULT_CODES::NEW_SMS;
		}
		else
			ATParser_findState = ERROR;

		return true;
	}

	return false;
}

void SIM800::getSMSData(MessageData& out) const
{
	out = SIM800_myMessageData;
}

void userCallBack();

MessageData tmp;
SIM800 test;

int main()
{
	char str6[] = "\r\n+CMGF: 1,2,3\r\n+CMT: \"+7926290909\",,\"2018/04/27,13:17:17+03\",145,17,0,0,\"+79262909090\",145,8\r\nPrivet\r\n\r\nERROR\r\n";

	test.setAnswerState(true);
	test.setCallBack(userCallBack);

	bool state = test.reader(str6, strlen(str6));

	return 0;
}

void userCallBack()
{
	int resState = test.getResultState();

	if (resState == (int)SIM800::SIM800_RESULT_CODES::NEW_SMS)
		cout << "New message!!!" << '\n';

	if (resState == (int)SIM800::SIM800_RESULT_CODES::AT_WITHPARAM)
	{
		char resStr[256];
		test.getResultStr(resStr);
		cout << "OK, Param:" << resStr << '\n';
	}

	if (resState == (int)SIM800::SIM800_RESULT_CODES::AT_ANS_OK)
		cout << "OK" << '\n';

	if (resState == (int)SIM800::SIM800_RESULT_CODES::AT_ANS_ERROR)
		cout << "ERROR" << '\n';
}
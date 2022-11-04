#pragma once
#include "ATParser.h"

struct MessageData
{
	char phoneNumberStr[12]; // Номер телефона
	char dateStr[30]; // Дата и время
	char messageStr[100]; // Сообщение
};

class SIM800 : public ATParser
{
public:
	SIM800() :ATParser(SIM800_urc, &SIM800_urcSize, SIM800_src, &SIM800_srcSize, SIM800_status, &SIM800_statusSize,
		SIM800_tmpBuffer, &SIM800_tmpBufferSize, SIM800_resultStr, &SIM800_resultStrSize) //,(bool (ATParser::**)())(SIM800_parsingPtr)
	{}
	~SIM800() {};

	enum class SIM800_RESULT_CODES
	{
		UNDEFINED, AT_ANS_OK, AT_ANS_ERROR, AT_WITHPARAM, NEW_SMS, NEW_RING
	};

private:
	enum SIM800_ParsingStates { DATA, END = 9 };
	int SIM800_parsingState = DATA; // Состояние парсинга сообщений

	MessageData SIM800_myMessageData;
	char* SIM800_messageDataPtr[13] = { SIM800_myMessageData.phoneNumberStr, NULL, SIM800_myMessageData.dateStr, NULL, NULL, NULL, NULL, NULL, NULL, SIM800_tmpBuffer, SIM800_myMessageData.messageStr };
	static const int SIM800_messageEndInd[11];
	int SIM800_messageLen = 0; // Длина получаемого СМС

	static const int SIM800_tmpBufferSize = 5; // Ограничение буфера обработки
	char SIM800_tmpBuffer[SIM800_tmpBufferSize];

	static const int SIM800_resultStrSize = 50; // Ограничение результирующего буфера
	char SIM800_resultStr[SIM800_resultStrSize];

	const ResCode SIM800_urc[2] = { {"CMT", 3, (bool (ATParser::*)())(&SIM800::readSMS)}, {"CLIP", 4} };
	const int SIM800_urcSize = sizeof(SIM800_urc) / sizeof(ResCode);

	const ResCode SIM800_src[3] = { {"AT", 2}, {"CMGF", 4}, {"CPOF", 4} };
	const int SIM800_srcSize = sizeof(SIM800_src) / sizeof(ResCode);

	const ResCode SIM800_status[2] = { {"OK", 2}, {"ERROR", 5} };
	const int SIM800_statusSize = sizeof(SIM800_status) / sizeof(ResCode);

	bool readSMS();
	bool message();
	//bool(SIM800::* SIM800_parsingPtr[2])(void) = { &SIM800::readSMS, &SIM800::message };

public:
	void getSMSData(MessageData& out) const;
};

const int SIM800::SIM800_messageEndInd[11] = { 13, 1, 31, 1, 1, 1, 1, 1, 1, 4, 300 }; // макс длина параметра конткретной AT, с учётом \0 в конце
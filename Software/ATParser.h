#define _CRT_SECURE_NO_WARNINGS
#pragma once
#include <iostream>

class ATParser
{
public:
	struct ResCode
	{
		const char* str;
		const short length;
		bool (ATParser::* handler)();
	};

	ATParser(const ResCode* urcInit, const int* urcSizeInit, const ResCode* srcInit, const int* srcSizeInit,
		const ResCode* statusInit, const int* statusSizeInit, char* tmpBufferInit, const int* tmpBufferSizeInit,
		char* resultStrInit, const int* resultStrSizeInit); // , bool (ATParser::** parsPtrInit)()
	~ATParser();

private:
	char* ATParser_tmpBuffer; // Указатель на буфер обработки заголовков и т.д. 
	const int* ATParser_tmpBufferSize; // Указатель на буфер обработки заголовков и т.д. 

	char* ATParser_resultStr; // Указатель на массив для выдачи результатов(параметров) ненужных AT-команд
	const int* ATParser_resultStrSize; // Размер результирующего массива

	const ResCode* ATParser_urc; // Указатель на массив незапрашиваемых кодов результата (URC)
	const int* ATParser_urcSize; // Кол-во URC

	const ResCode* ATParser_src; // Указатель на массив запрашиваемых кодов результата (SRC)
	const int* ATParser_srcSize; // Кол-во SRC

	const ResCode* ATParser_status; // Указатель на массив результатов выполнения AT-команд (OK/ERROR)
	const int* ATParser_statusSize; // Кол-во возможных результатов выполнения AT-команд

	void(*ATParser_callBack)() = nullptr; // Указатель на функцию пользователя для вывода результата

	bool error(); // Функция выдачи ошибки
	bool readHeader(); // Чтение заголовка
	bool readStatus(); // Чтение статуса (OK/ERROR)
	bool result(); // Выдача результата, вызов callback
	bool readAT(); // Чтение параметров произвольной AT-команд
	bool(ATParser::* ATParser_parsingPtr[6])(void) = { &ATParser::error, &ATParser::readHeader, &ATParser::readHeader, &ATParser::readStatus, &ATParser::result, &ATParser::readAT };

protected:
	bool (ATParser::* ATParser_parsingUserPtr)(); // Указатель на функцию потомка
	// BLOCK - блокировка процесса поиска информации
	// ERROR - ошибка парсинга
	// BEGIN - начало поиска строки
	// HEADER - парсинг заголовка
	// READ - чтение информации
	// MESSAGE - копирование смс в переменную
	// RESULT - данные готовы забирай быстрее
	// ANS - чтение ответа в виде AT
	// USER - вызываем дочернию функцию парсинга конкретной AT

	enum FindStates { BLOCK = -1, ERROR, BEGIN, HEADER, STATUS, RESULT, ANS, USER };

	int ATParser_findState = BEGIN; // Состояние текущего поиска
	bool ATParser_answerWaitState = false; // Состояние ждём ли мы ответ на какую-либо AT-команду, false - не ждём, true - ждём
	int ATParser_resultState = 0; // Состояние, какой результат готов

	char* ATParser_gprsBuffer; // Указатель на начало потока
	char* ATParser_gprsBufferEnd; // Указатель на конец потока

	int ATParser_tmpLength = 0;

	int ATParser_errorType = 0; // Тип выдаваемой ошибки
	bool ATParser_mark = false; // true - кавычки открылись, false - кавычки закрылись

	static int str2int(const char* str, int length, int base); // Перевода строки в число, входные параметры указатель на буфер, его длина, базис.

public:
	bool reader(char* gprsBuf, int len); // Основная функция обработки потока данных

	void setCallBack(void (*userCallBack)()) { ATParser_callBack = userCallBack; }
	bool setAnswerState(bool state) { ATParser_answerWaitState = state; return true; }

	void getResultStr(char* output) const { strcpy(output, ATParser_resultStr); };
	int getResultState() { return ATParser_resultState; }
};
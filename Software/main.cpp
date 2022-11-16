#define DEBUG
#define SIM_DEBUG //вывод ответов с SIM800 модуля

#include <Wire.h>
#include <VL53L1X.h>
#include "GyverTimer.h"
GTimer timerTempRequest(MS, 500);

VL53L1X sensor;
bool full = false;
//SIM module **********************************************************************
Stream *sim800 = &Serial1;
Stream *debug = &Serial2;
// BEGIN - начало поиска строки
// HEADER - парсинг заголовка
// MESSAGE - чтение парараметров пришедшего смс
// TEXT - копирование текста смс в переменную
// TIME - копирование времени
// RESULT - выдача смс пользователю
//char startConfig[][15] = {"AT+CMGF=1", "AT+CSCS=\"GSM\"", "AT+CNMI=2,2", "AT+CSDH=1", "AT+CPMS=\"MT\"", "AT+CLIP=1", "AT+CLTS=1"};
const char whiteList[][13] = {"xxxxxxxx", "xxxxxxxx"};
const int whiteListSize = sizeof(whiteList) / 12;

enum ParsStates {ERROR, BEGIN, HEADER, MESSAGE, TEXT, RING, TIME, RESULT};
int parsState = BEGIN;
int errorType = -1;
int resultType = -1;

const int tmpBufferSize = 15; // MAX размер буфера считывателя
char tmpBuffer[tmpBufferSize+1];
int tmpLength = 0;

bool mark = false; // true - кавычки открылись, false - кавычки закрылись

struct MessageData
{
  char phoneNumberStr[13]; // Номер телефона
  char dateStr[30]; // Дата и время
  char messageStr[300]; // Сообщение
} myMessageData;

char* messageDataPtr[11] = { myMessageData.phoneNumberStr, NULL, myMessageData.dateStr, NULL, NULL, NULL, NULL, NULL, NULL, tmpBuffer, myMessageData.messageStr };
const int messageEndIndexes[11] = { 13, 5, 31, 5, 5, 5, 5, 13, 5, 15, 300 };
int messageLen = 0; // Длина получаемого смс

enum MessageStates { DATA, END = 9 };
int messageState = DATA;

//****************************************************************
char phoneNumberStr[13]; // Номер телефона звонка
char* ringDataPtr[11] = { phoneNumberStr, NULL, NULL, NULL, tmpBuffer , tmpBuffer, tmpBuffer, tmpBuffer};
const int ringEndIndexes[6] = { 13, 15, 15, 15, 15, 15 };
int ringState = 0;
int endRingState = 5;

bool error();
bool readHeader();
bool readSMS();
bool message();
bool readTime();
bool readRing();
bool result();
//void newMessage(MessageData& out);
void newTime();
int str2int(const char* str, int length, int base);
void UCS2ToString(const char* input, char* res);
bool(*parsingPtr[])() = { error, readHeader, readHeader, readSMS, message, readRing, readTime, result };

const int minf = 0, maxf = 600;
int dist = 0;

void setup()
{
  Serial1.begin(115200); // sim800
  Serial2.begin(115200); // debug
  // Ждём все порты
  while (!Serial1) {}
  while (!Serial2) {}

  pinMode(PC13, OUTPUT);
  Wire.begin();
  Wire.setClock(400000); // use 400 kHz I2C
  Serial2.println("Start prog...");

  sensor.setTimeout(500);
  if (!sensor.init())
  {
    Serial2.println("Failed to detect and initialize sensor!");
    while (1);
  }

  Serial2.println("OK 2");
  sensor.setDistanceMode(VL53L1X::Long);
  sensor.setMeasurementTimingBudget(15000);
  sensor.startContinuous(15);

  digitalWrite(PC13, 1);
  delay(1000);
  digitalWrite(PC13, 0);
}
bool fl = false;

void loop()
{
  if (timerTempRequest.isReady())
  {
    int dist = sensor.read();
    int procent = ( ( maxf - dist ) * 100 ) / ( maxf - minf );
    Serial2.println(String(dist));
    Serial2.println(String(procent) + "%");
	
    if (procent > 90 || !full)
    {
      sendNotificSMS("xxxxxxx");
      full = true;
    }

    if (procent < 80)
      full = false;
      
    fl = !fl;
    digitalWrite(PC13, fl);
  }

  reader(); // Функция обработки SIM800

  if (Serial2.available())
    Serial1.write(Serial2.read());

  /*  Serial2.println(String(sensor.read()));
    digitalWrite(PC13, 0);
    // Serial2.println("OK");
    delay(100);
    digitalWrite(PC13, 1);
    delay(100);
  */
  /*if (Serial1.available())
    Serial2.write(Serial1.read());*/
}

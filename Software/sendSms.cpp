void sendStateSMS(const char* num)
{
  sim800->print("AT+CMGS=\"");
  sim800->print(num);
  sim800->print("\"\r\n");
  delay(2000);
  int dist = sensor.read();
  int procent = ( ( maxf - dist ) * 100 ) / ( maxf - minf );

  if (procent > 100 || procent < 0)
  {
    sim800->print("I broke down, help me pls! :-/");
    sim800->write(0x1A); // end of SMS
    return;
  }

  sim800->print("The level of fullness of the dumpster is ");

  sim800->print(String(procent) + "%, ");
  sim800->print(String(dist) + " cm\n");

  sim800->print("Status: ");
  if ( procent < 50)
    sim800->print("good :-)");
  if ( procent >= 50 &&  procent < 80)
    sim800->print("bad :-|");
  if ( procent >= 80 &&  procent <= 100)
    sim800->print("terrible :-(");

  sim800->write(0x1A); // end of SMS
}

void sendNotificSMS(const char* num)
{
  sim800->print("AT+CMGS=\"");
  sim800->print(num);
  sim800->print("\"\r\n");
  delay(2000);

  sim800->print("Man, it's time to throw out the trash!!!");

  sim800->write(0x1A); // end of SMS
}

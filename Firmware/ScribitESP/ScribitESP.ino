#include "ScribIt.hpp"
#include "SITestBancoESP.hpp"

ScribIt scribit;

void checkTestAndRun()
{
  uint8_t countPress = 0;

  pinMode(SW, INPUT);

  int startTimer = millis();
  while (millis() - startTimer < TESTMODE)
  {
    if (digitalRead(SW) == 0) //TESTMODE)
    {
      while (digitalRead(SW) == 0)
      {
      }
      countPress++;
    }
  }

  if (countPress > 2)
  {
    SITestBancoESP testMode;
    testMode.run();
  }
}

void setup()
{
  checkTestAndRun();
  scribit.begin();
}

void loop()
{
  //Main control loop
  scribit.loop();
}

#define DELAY 1000
#define SW 8
#define TESTMODE 1500

class SITestBancoESP
{
  RGBLEDs ledTest; //RGBLed

public:
  void run()
  {
    for (;;)
    {
      ledTest.setColor(0, 0, 255);
      delay(DELAY);
      ledTest.setColor(0, 255, 0);
      delay(DELAY);
      ledTest.setColor(255, 0, 0);
      delay(DELAY);
    }
  }
};

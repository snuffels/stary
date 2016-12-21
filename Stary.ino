#include <EEPROMex.h>
#include <EEPROMVar.h>

#include <Adafruit_NeoPixel.h>

/* Create starlight fadeout or morning colors fade in pattern in WS2812B string leds
   UI Key1, Key2, pot
   Key1 short press: change pattern then fadeout starlight, pot gives fadeout time
   Key2 short press: Change pattern then fade in wake up, pot gives fadein time
   key1 long press: change starlight colors, use pot to set point 2
   key2 long press: change morning colors (by color temp) use pot for point 2
   key1+key2 long: change current brightness, fade out fast ... return to max ... fade out fast
   key1+key2 short: store values in EEPROM and turn off leds 

   using pot:
   pot 0....1024
   time minimal 1 minute.
   time maximal 4 hours
*/

#ifdef __AVR__
#include <avr/power.h>
#endif

#define PIN 8               //connect the led string here

#define KEY1 3              //push button to ground
#define KEY2 6              //push button to ground
#define KEYTIMESHORT  150   //a short button push should be at least this long to detect
#define KEYTIMELONG   1000  //a long button push schould be at least this long to detect
#define LEDS  20            //the amount of leds connected
#define POT A0              //a potmeter 47K between +5 and GND with the runner to A0. Also connect a 4.7u cap from the runner to GND.
#define POTHYST 5           //hysteresis for the POT. So that only a value change of more than 5 is detected.


// Parameter 1 = number of pixels in strip
// Parameter 2 = Arduino pin number (most are valid)
// Parameter 3 = pixel type flags, add together as needed:
//   NEO_KHZ800  800 KHz bitstream (most NeoPixel products w/WS2812 LEDs)
//   NEO_KHZ400  400 KHz (classic 'v1' (not v2) FLORA pixels, WS2811 drivers)
//   NEO_GRB     Pixels are wired for GRB bitstream (most NeoPixel products)
//   NEO_RGB     Pixels are wired for RGB bitstream (v1 FLORA pixels, not v2)
Adafruit_NeoPixel strip = Adafruit_NeoPixel(LEDS, PIN, NEO_GRB + NEO_KHZ400);

// IMPORTANT: To reduce NeoPixel burnout risk, add 1000 uF capacitor across
// pixel power leads, add 300 - 500 Ohm resistor on first pixel's data input
// and minimize distance between Arduino and first pixel.  Avoid connecting
// on a live circuit...if you must, connect GND first.


int starField[LEDS][3];         //RGB value for every LED
uint8_t starStart[3];           //Start color for star field (composed of all the low values)
uint8_t starStop[3];            //Stop color for star field (composed of al the high values) Start[n]<Stop[n]
int potVal;                     //value of the POT
byte potChanged;                //POT value has changed since last line =1 else 0
byte mode;                      //What is the current mode: 1 Starfield 2 SunRise 0 off
byte brightness;                //Brightness of the starfield In star mode an true color brightness is calculated. In sunmode Adafruit brightness 
                                //is used. at low values the leds turn redder and that is good at a sunrise.
long startTime;                 //To test the length of intervals
byte iLum;                      //A seccondary brightness for starmode. The ilumminance for all leds can be set so the stars are not too bright
int dayStart;                   //Start value in Kelvin of daylight colors
int dayStop;                    //Stop value of daylight colors in Kelvin
int adresEE[5]={0,0,0,0,0};     //The addresses of the values stored in EEPROM set to 0 because of strange bug that sometimes executes setup twice.


void setup() {
  /*   EEPROM memory map
      starrStart[]
      starStop[]
      iLum
      int dayStart
      int dayStop
  */
  if(adresEE[4]==0)
  {// there is a strange bug that sometimes executes setup twice
    adresEE[0] = EEPROM.getAddress(sizeof(starStart));
    adresEE[1] = EEPROM.getAddress(sizeof(starStop));
    adresEE[2] = EEPROM.getAddress(sizeof(iLum));
    adresEE[3] = EEPROM.getAddress(sizeof(dayStart));
    adresEE[4] = EEPROM.getAddress(sizeof(dayStop));
  }

  //mode=1 stary night mode=2 daybreak
  mode = 0;
  Serial.begin(9600);
  Serial.println("init");
  randomSeed(analogRead(POT));
  strip.begin();
  strip.show(); // Initialize all pixels to 'off'

  pinMode(KEY1, INPUT_PULLUP);
  pinMode(KEY2, INPUT_PULLUP);
  getValues(); //Get EEPROM values
//  When de EEPROM is empty you first need to fill it with something usefull: 
//  starStart[0] = 220;
//  starStart[1] = 152;
//  starStart[2] = 0;
//  starStop[0] = 253;
//  starStop[1] = 213;
//  starStop[2] = 152;
//  iLum = 255; //helderheid manuele instelling
//  dayStart = 1500;
//  dayStop = 10000;

  brightness = 255; //helderheid voor aftellen starry of optellen sunup

DumpSettings();
potVal=analogRead(POT); //Not to start with potChanged =1
}

void loop() {

  Pot();
  Key1();
  Key2();

  if (mode == 1)
    DoStary();
  if (mode == 2)
    DoDaybreak();
}

void Pot()
{
  if (potVal - POTHYST > analogRead(POT) || potVal + POTHYST < analogRead(POT))
  {
    potChanged = 1;
    potVal = analogRead(POT);
    Serial.println(potVal);

  }

}

void Key1()
{
  long tKey;
  byte tempKey = 0;
  // Serial.println("Do key 1");
  if (digitalRead(KEY1) == LOW)
  {
    delay(20); //want anders moet je toets 1 en 2 wel heel erg tegelijk indrukken
    tKey = millis();
    if (digitalRead(KEY2) == LOW)
      tempKey = 2;
    Serial.println("key1 low");
    Serial.print(mode);
    while (digitalRead(KEY1) == LOW)
    {
      if (millis() - tKey > KEYTIMELONG)
      {
        if (digitalRead(KEY2) == LOW)
        { //Key 1 + 2 long press
          DoKey12Long();
          tKey = millis();
          return; //exit routine to prevent flase key detection
        }
        else
        {
          //longkey press detected on key1
          //Pot();
          ChangeColor(1);
          tempKey = 10; //want longpress is al uitgevoerd
          tKey = millis();
        }
      }
    }
    //key had been pressed but is not pressed at the moment
    if (millis() - tKey > KEYTIMESHORT)
    {
      Serial.println("do Key1 short");
      if (tempKey == 2)
      { //key 2 also pressed
        DoKey12Short();
      }
      else
      {
        //tKey=millis();
        ChangePatternStar();
        mode = 1;
      }
    }

    if (tempKey == 10)
      delay(1000); // wait 5seconds to allow user to set time with pot

  }
  tempKey = 0;
}

void Key2()
{
  long tKey;
  byte tempKey = 0;
  // Serial.println("Do key 2");
  if (digitalRead(KEY2) == LOW)
  {
    delay(20);
    tKey = millis();
    if (digitalRead(KEY1) == LOW)
      tempKey = 1;
    Serial.println("key2 low");
    while (digitalRead(KEY2) == LOW)
    {
      if (millis() - tKey > KEYTIMELONG)
      {
        //longkey press detected on key2
        if (digitalRead(KEY1) == LOW)
        { //key 1 also pressed
          DoKey12Long();
          tKey = millis();
          return; //exit routine to prevent false key detection
        }
        else
        {
          Pot();
          ChangeColor(2);
          tempKey = 10; //longpress has been done
          tKey = millis();
        }
      }
    }
    //key had been pressed but is not pressed at the moment
    if (millis() - tKey > KEYTIMESHORT)
    {
      Serial.println("do Key2 short");
      if (tempKey == 1)
      { //key 1 also pressed
        DoKey12Short();
      }
      else
      {
        ChangePatternDay();
        mode = 2;
      }
    }
    if (tempKey == 10)
      delay(1000); //allow user to set time delay
  }
  tempKey = 0;
}

/*******************************************************************************
                    DoKey12Long
   Change iLum value when keys 1 + 2 are pressed fore more than KEYTIMELONG
   decrese from 255 to 95 in steps of -10
Called from Key1() Key2()
 ******************************************************************************/

void DoKey12Long()
{
  int t;
  Serial.println("DoKey12Long()");
  if (iLum > 90 && mode == 1)
  {
    iLum -= 10;
    Serial.println(iLum);
    ShowField();
  }
  if(iLum <=105)
  { //when iLum is <= 105 flash leds
    t=iLum;
    ShowField();
    delay(100);
    iLum=20;
    ShowField();
    delay(100);
    iLum=t;
    ShowField();
    delay(100);
    iLum=20;
    ShowField();
    delay(100);
    iLum=t;
    ShowField();

  }
  if (iLum <= 90 && mode == 1)
  {
    iLum = 255;
    ShowField();

  }
 delay(500); //prevent false positive on short key press

}

/*******************************************************************************
                  DoKey12Short
   Do leds OFF. Set mode to 0
   Save changed values in EEPROM


 ******************************************************************************/

void DoKey12Short()
{
  Serial.println("DoKey12Short()");
  mode = 0;
  for (int i = 0; i < LEDS; i++)
  {
    strip.setPixelColor(i, 0, 0, 0);
  }

  strip.show(); // Initialize all pixels to 'off'
  updateValues(); //update values in EEprom
}

/*******************************************************************************
                        ChangeColor(mode)
   Change color scheme according to day or night
   Called from Key1() Key2() after long press
   uses Pot to set secondary color range
 ******************************************************************************/
void ChangeColor(int mod)
{
  long morning, noon;
  // strip.setBrightness(brightness);
  Serial.print("Mode=");
  Serial.println(mod, DEC);

  Serial.println("ChangeColor()");
  if (mod == 1)
  {
    brightness = 255;
    //change starlight color
    //key= change green
    //pot= change blue
    Pot();
    if (potChanged == 1)
    {
      potChanged = 0;
      Serial.print("Analog value changed to :");
      Serial.println(potVal, DEC);
      starStop[2] = potVal / 4;
    }
    else if (starStop[1] < 250 && starStart[1] < 250)
    {
      starStart[1] += 5;
      starStop[1] += 5;
    }
    else
    {
      starStop[1] = starStop[1] - starStart[1];
      starStart[1] = 0;
    }

    for (int i = 0; i < 20; i++)
    {
      strip.setPixelColor(i, Fade(RGB2Long(starStart[0], starStart[1], starStart[2]) , RGB2Long(starStop[0], starStop[1], starStop[2]), LEDS, i));
    }
    strip.show();

  }
  if (mod == 2)
  {
    brightness = 50; //Als deze gebruikt in het donker is hoge helderheid mogelijk helemaal niet fijn
    Pot();
    if (potChanged == 1)
    {
      potChanged = 0;
      Serial.print("Analog value changed to :");

      dayStop = 4000 + (potVal * 11);
      Serial.println(dayStop, DEC);
    }
    else if (dayStart > 100) //optimale waarde nog testen
      dayStart -= 100;
    else
      dayStart = 2700;
    Serial.println(dayStart, DEC);
    morning = colorTemperatureToRGB(dayStart);
    noon = colorTemperatureToRGB(dayStop);
    for (int i = 0; i < 20; i++)
    {
      strip.setPixelColor(i, Fade(morning , noon , LEDS, i));
    }
    strip.show();

  }

}

/*******************************************************************************
                    ChangePatternStar()
   Change the pattern according to the current color range for the stars



 ******************************************************************************/
void ChangePatternStar()
{
  Serial.println("ChangePattern");

  startTime = millis();
  brightness = 255;
  for (int i = 0; i < LEDS; i++)
  {
    for (int j = 0; j < 3; j++)
    {
      starField[i][j] = random(starStart[j], starStop[j]);
    }
    /*
       Serial.print(starField[i][0]);
       Serial.print(' ');
       Serial.print(starField[i][1]);
       Serial.print(' ');
       Serial.print(starField[i][2]);
       Serial.println(' ');
    */
  }
  mode = 1;
  ShowField();

}

/*******************************************************************************
                    ChangePatternDay()
   Change the pattern according to the current color range for the suns



 ******************************************************************************/

void ChangePatternDay()
{
  long sun;
  startTime = millis();
  Serial.println("ChangePatternDay");
  brightness = 2;
  for (int i = 0; i < LEDS; i++)
  {
    sun = colorTemperatureToRGB(random(dayStart, dayStop));
    starField[i][0] = i2Red(sun);
    starField[i][1] = i2Green(sun);
    starField[i][2] = i2Blue(sun);

  }
  /*  if (potChanged == 1)
    {
      potChanged = 0;
      Serial.print("Analog value :");
      Serial.println(potVal, DEC);
    } */
  mode = 2;
  ShowField();
}

/*******************************************************************************
                    ShowField()
   Set the leds to the proper value according to mode



 ******************************************************************************/
void ShowField()
{
  float r, g, b;
  for (int i = 0; i < LEDS; i++)
  {
    if (brightness > -1 && mode == 1)
    { //bereken helderheid met behoud van kleur
      r = ((float)starField[i][0] - ((float)starField[i][0] / 255) * (255 - brightness)) * ((float)iLum / 255);
      g = ((float)starField[i][1] - ((float)starField[i][1] / 255) * (255 - brightness)) * ((float)iLum / 255);
      b = ((float)starField[i][2] - ((float)starField[i][2] / 255) * (255 - brightness)) * ((float)iLum / 255);
    }
    else
    {
      r = starField[i][0];
      g = starField[i][1];
      b = starField[i][2];

    }
    strip.setPixelColor(i, (int)r, (int)g, (int)b);
  }
  if (mode == 2)
    strip.setBrightness(brightness); // De adafruit brightness doet geen kleur behoud en daardoor is bij lage brightness de kleur roder
  strip.show();
}

/*******************************************************************************
          void DoStary()
 Decrease starField brightness at set interval.
Interval set bij analog value from POT
tottal interval= (14076*(1+POT))/255 =4 hours in 255 steps

Called from main()
 ******************************************************************************/
void DoStary()
{
  if ((millis() - startTime > ((14076 * ((long)potVal + 1)) / 255)) && brightness > 0) //curTime-lastTime> 14sec * potVal (=total time)/ number of brightess steps
  {
    brightness--;
    startTime = millis();
    ShowField();
    //    strip.setBrightness(brightness);

    //    strip.show();
    Serial.print(iLum);
    Serial.print("  ");
    Serial.println(brightness);
  }
}



/*******************************************************************************
                  DoDaybreak()
   Calculate the current brightness and showField



 ******************************************************************************/
void DoDaybreak()
{
  // Serial.print('-');
  if ((millis() - startTime > ((14076 * ((long)potVal + 1)) / 255)) && brightness < 255) //curTime-lastTime> 14sec * potVal (=total time)/ number of brightess steps
  {
    brightness++;
    startTime = millis();
    ShowField();
    Serial.println(brightness);
  }

}



/*******************************************************************************
                  updateValues()
   Store changed values to EEPROM

   /*   EEPROM memory map
    starrStart[]
    starStop[]
    iLum
    int dayStart
    int dayStop

   adresEE[0]=EEPROM.getAddress(sizeof(starStart));
   adresEE[1]=EEPROM.getAddress(sizeof(starStop));
   adresEE[2]=EEPROM.getAddress(sizeof(iLum));
   adresEE[3]=EEPROM.getAddress(sizeof(dayStart));
   adresEE[4]=EEPROM.getAddress(sizeof(dayStop));

Called from DoKey12Short()
 ******************************************************************************/
void updateValues()
{
  EEPROM.updateBlock(adresEE[0], starStart);
  EEPROM.updateBlock(adresEE[1], starStop);
  EEPROM.updateBlock(adresEE[2], iLum);
  EEPROM.updateBlock(adresEE[3], dayStart);
  EEPROM.updateBlock(adresEE[4], dayStop);

}

/*******************************************************************************
*             getValues()
*
*Get global variables from EEPROM
*Called from Setup
*
*******************************************************************************/

void getValues()
{

  EEPROM.readBlock(adresEE[0], starStart);
  EEPROM.readBlock(adresEE[1], starStop);
  EEPROM.readBlock(adresEE[2], iLum);
  EEPROM.readBlock(adresEE[3], dayStart);
  EEPROM.readBlock(adresEE[4], dayStop);


}


/*******************************************************************************
          RGB2Long(byte red, byte green, byte blue)

Return LONG GRB value from red green blue bytes. 

Called from ChangeColor()
 ******************************************************************************/
long RGB2Long(byte red, byte green, byte blue)
{

  return blue + (green * 255) + (red * 65535);


}




/*******************************************************************************
            long colorTemperatureToRGB(int kelvin)

// From http://www.tannerhelland.com/4435/convert-temperature-rgb-algorithm-code/

// Start with a temperature, in Kelvin, somewhere between 1000 and 40000.  (Other values may work,
//  but I can't make any promises about the quality of the algorithm's estimates above 40000 K.)

Calculate long GRB value from color temerature in Kelvin.

Called from ChangeColor()
 ******************************************************************************/


long colorTemperatureToRGB(int kelvin) {


  float red;
  float green;
  float blue;
  float temp;

  temp = kelvin / 100;


  if ( temp <= 66 ) {

    red = 255;

    green = temp;
    green = 99.4708025861 * log(green) - 161.1195681661;


    if ( temp <= 19) {

      blue = 0;

    } else {

      blue = temp - 10;
      blue = 138.5177312231 * log(blue) - 305.0447927307;

    }

  } else {

    red = temp - 60;
    red = 329.698727446 * pow(red, -0.1332047592);

    green = temp - 60;
    green = 288.1221695283 * pow(green, -0.0755148492 );

    blue = 255;

  }
  constrain(red, 0, 255);
  constrain(green, 0, 255);
  constrain(blue, 0, 255);
  return iColor(red, green, blue);


}


/*******************************************************************************
Hee deze doet hetzelfde als rgb2long. Deze is efficientenr




 ******************************************************************************/


uint32_t iColor(uint32_t red, uint32_t green, uint32_t blue)
{
  uint32_t c = 0;
  c = (red << 16) + (green << 8) + blue;
  return c;
}

/*******************************************************************************
uint32_t Fade(uint32_t startColor, uint32_t stopColor, uint32_t Steps, uint32_t Index)
Calculate the color between two colors.
startColor - color to start from GRB
stopColor   -Color to stop at GRB
Steps - total amount of steps, like the amount of leds in the string
Index - the step to return the color value from

 ******************************************************************************/

uint32_t Fade(uint32_t startColor, uint32_t stopColor, uint32_t Steps, uint32_t Index)
{
  uint32_t red, green, blue;
  red =  ((i2Red  (startColor) * (Steps - Index)) + (i2Red  (stopColor) * Index)) / Steps;
  green = ((i2Green(startColor) * (Steps - Index)) + (i2Green(stopColor) * Index)) / Steps;
  blue = ((i2Blue (startColor) * (Steps - Index)) + (i2Blue (stopColor) * Index)) / Steps;

  return iColor(red, green, blue);
}


/*******************************************************************************
    i2Red i2Green i2Blue
    Return the 8 bit color value from a combinded GRB (not rgb) long value.


  Called from:
  ChangePatternDay()
  Fade()
 ******************************************************************************/


uint8_t i2Red(uint32_t ired)
{
  return ((ired >> 16) & 255);
}

uint8_t i2Green(uint32_t igreen)
{
  return ((igreen >> 8) & 255);
}

uint8_t i2Blue(uint32_t iblue)
{
  return (iblue & 255);
}


void DumpSettings()
{
  Serial.print("starStart[0]  ");
  Serial.println(starStart[0]);
  Serial.print("starStart[1]  ");
  Serial.println(starStart[1]);
  Serial.print("starStart[2]  ");
  Serial.println(starStart[2]);
  Serial.print("starStop[0]  ");
  Serial.println(starStop[0]);
  Serial.print("starStop[1]  ");
  Serial.println(starStop[1]);
  Serial.print("starStop[2]  ");
  Serial.println(starStop[2]);
  Serial.print("iLum  ");
  Serial.println(iLum);
  Serial.print("dayStart  ");
  Serial.println(dayStart);
  Serial.print("dayStop  ");
  Serial.println(dayStop);
}



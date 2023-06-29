// C++ code
//
#include <EasyBuzzer.h>
#include <EEPROM.h>
#include <EasyButton.h>
#define BUTTON_01_PIN 3
#define BUTTON_02_PIN 4
#define BAUDRATE 115200
EasyButton button_01(BUTTON_01_PIN);
EasyButton button_02(BUTTON_02_PIN);

char *main_osmose_status[] = { "Bereit", "Osmose Btn 1", "Osmose Btn 2", "Spülung", "Desinfektion",
                               "Reinwasser", "Auto_Stehwasserspülung"
                                             "Desinfektion" };
char *sub_osmose_status[] = { "Alles aus", "Membranspülung", "Stehwasserspülung",
                              "Reinwasser", "Int_Stehwasserspülung"
                                            "Desinfektion" };


int main_status_osmose_no = 0;
int sub_status_osmose_no = 0;
//int taster02_pin = 3;
//int taster03_pin = 4;
//int taster04_pin = 5;
int m1_ventil = 8;   // ausgang membranspülung;
int m2_ventil =11;   // wasserzufuhr
int m3_ventil = 10;  // ausgang reinwasser
int m4_ventil = 9;  // ausgang stehwasser;


bool taster01_state = HIGH;
bool taster02_state = HIGH;
bool taster03_state = HIGH;
bool taster04_state = HIGH;
bool taster_off = HIGH;
bool taster_on = LOW;
bool ventil_auf = HIGH;
bool ventil_zu = LOW;
float durchfluss_liter_sec = 0.017;       // defaulit 1 l/min = 1/60 = 0.017
float verhaeltnis_rein_spuel = 0.333333;  //default 1:3 = 0.3333333
float membranspuelung_ms = 5000;
float stehwasserspuelung_ms = 0;
float init_stehwasserspuelung_ms = 120000; // 2 min _ 120 sec.
//long intervall_stehwasserspuelung = (6L * 60L * 60L * 1000L);  // vor 6 Stunden))
long intervall_stehwasserspuelung = 21600000L;  //  6 Stunden))
long next_stehwasserspuelung;

float auto_stehwasserspuelung_ms = 5000;
long reinwasser_01_ms;
long reinwasser_02_ms;
long eeprom_reinwasser_01_ms = 10000;
long eeprom_reinwasser_02_ms = 10000;
float reinwasser_ms = 20000;
float tmp_reinwasser_ms = 3600000L;

float start_reinwasser_ms = 0;
float end_membranspuelung_ms = 4000;
float start_ms, process_ms, phasen_ms, start_phasen_ms, set_ms;
int zeitmessung_status = 0;
float zeitmessung_ms = 0;
bool set_01 = 0;
bool set_02 = 0;

String Z01 = "";


void setup() {

  EasyBuzzer.setPin(12);
  Serial.begin(9600);

  pinMode(BUTTON_01_PIN, 1);
  pinMode(BUTTON_02_PIN, 1);
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(m1_ventil, OUTPUT);  // Wasserzufuht
  pinMode(m2_ventil, OUTPUT);
  pinMode(m3_ventil, OUTPUT);
  pinMode(m4_ventil, OUTPUT);

  digitalWrite(m1_ventil, ventil_zu);
  digitalWrite(m2_ventil, ventil_zu);
  digitalWrite(m3_ventil, ventil_zu);
  digitalWrite(m4_ventil, ventil_zu);

  eeprom_reinwasser_01_ms = EEPROMReadlong(1);
  if (eeprom_reinwasser_01_ms < 10000) EEPROMWritelong(1, 10000);
  eeprom_reinwasser_02_ms = EEPROMReadlong(5);  //
  if (eeprom_reinwasser_02_ms < 10000) EEPROMWritelong(5, 10000);
  reinwasser_01_ms = EEPROMReadlong(1);
  reinwasser_02_ms = EEPROMReadlong(5);

  // Initialize the button.
  button_01.begin();
  button_02.begin();
  // Add the callback function to be called when the button is pressed for at least the given time.
  button_01.onPressedFor(2000, b01_onPressedForDuration);
  button_01.onPressed(b01_onPressed);

  button_02.onPressedFor(2000, b02_onPressedForDuration);
  button_02.onPressed(b02_onPressed);

  ausgabe("INIT", "", "", "", "","","");

  next_stehwasserspuelung =  millis();



}

void loop() {
  EasyBuzzer.update();

  button_01.read();
  button_02.read();
  if (millis() % 1000 == 0) {
    //Serial.write(27);Serial.write(91);Serial.write("3A"); // VT100 3x Cursor UP
    ausgabe(main_osmose_status[main_status_osmose_no],
            sub_osmose_status[sub_status_osmose_no],
            "Phasenzeit: " + String(start_phasen_ms) + " > " + String(start_phasen_ms - phasen_ms),
            "Rein01: " + String(reinwasser_01_ms),
            "Rein02: " + String(reinwasser_02_ms),
            "Next Spülung: "+ String(next_stehwasserspuelung),
            "Millis:" + String(millis()));
  }

  if (millis() >= next_stehwasserspuelung && main_status_osmose_no == 0)  // Auto_Spülung
  {
    reinwasser_ms=0;
    main_status_osmose_no =6;

  }



  if (main_status_osmose_no == 1 || main_status_osmose_no == 2 || main_status_osmose_no == 6)  // Osmose
  {
    process_ms = millis() - start_ms;
    if (set_01 == 1) process_ms = process_ms - (millis() - set_ms);
    if (process_ms >= 0 && process_ms <= membranspuelung_ms)  //Membranspülung
    {
      membranspuelung();
      sub_status_osmose_no = 1;
      start_phasen_ms = membranspuelung_ms / 1000;
      phasen_ms = round((membranspuelung_ms - process_ms) / 1000);
    }
    if (process_ms >= membranspuelung_ms && process_ms < membranspuelung_ms + stehwasserspuelung_ms)  //Stehwasserspülung
    {
      stehwasserspuelung();
      sub_status_osmose_no = 2;
      start_phasen_ms = stehwasserspuelung_ms / 1000;
      phasen_ms = round((membranspuelung_ms + stehwasserspuelung_ms - process_ms) / 1000);
      if (process_ms >= membranspuelung_ms  + stehwasserspuelung_ms-1000) next_stehwasserspuelung = millis()+intervall_stehwasserspuelung;
    }
    if (process_ms >= membranspuelung_ms + stehwasserspuelung_ms && process_ms <= membranspuelung_ms + stehwasserspuelung_ms + reinwasser_ms)  //Reinwasser
    {
      reinwasser();
      sub_status_osmose_no = 3;
      phasen_ms = (membranspuelung_ms + stehwasserspuelung_ms + reinwasser_ms - process_ms) / 1000;
      start_phasen_ms = reinwasser_ms / 1000;
    }
    if (process_ms >= membranspuelung_ms + stehwasserspuelung_ms + reinwasser_ms && process_ms <= membranspuelung_ms + stehwasserspuelung_ms + reinwasser_ms + end_membranspuelung_ms)  //Reinwasser
    {
      membranspuelung();
      sub_status_osmose_no = 1;
      start_phasen_ms = end_membranspuelung_ms / 1000;
      phasen_ms = round((membranspuelung_ms + stehwasserspuelung_ms + reinwasser_ms + end_membranspuelung_ms - process_ms) / 1000);
    }
    if (process_ms >= membranspuelung_ms + stehwasserspuelung_ms + reinwasser_ms + end_membranspuelung_ms)  //Alles aus
    {
      alles_aus();
      sub_status_osmose_no = 0;
      main_status_osmose_no = 0;
    }

  }
  else if  (main_status_osmose_no == 0) alles_aus();

  //membranspuelung(5); // 5sec.
  //stehwasserspuelung(0.1); // 0.1 liter
  //reinwasser(1); // 1 literffff
  // } else {
  // turn LED off
  //digitalWrite(LED_BUILTIN, LOW);
  //}
  //delay(10); // Delay a little bit to improve simulation performance
}
void membranspuelung()  //M1 + M2 + M4
{
  //display_output(1,  "Membransp.");
  digitalWrite(m1_ventil, ventil_auf);
  digitalWrite(m2_ventil, ventil_auf);
  digitalWrite(m3_ventil, ventil_zu);
  digitalWrite(m4_ventil, ventil_auf);

  return;
}
void stehwasserspuelung()  //M2 + M4
{
  digitalWrite(m1_ventil, ventil_zu);
  digitalWrite(m2_ventil, ventil_auf);
  digitalWrite(m3_ventil, ventil_zu);
  digitalWrite(m4_ventil, ventil_auf);

  return;
}

void reinwasser() {    //M2 + M3
  digitalWrite(m1_ventil, ventil_zu);
  digitalWrite(m2_ventil, ventil_auf);
  digitalWrite(m3_ventil, ventil_auf);
  digitalWrite(m4_ventil, ventil_zu);
  return;
}

void alles_aus()  
{
  digitalWrite(m1_ventil, ventil_zu);
  digitalWrite(m2_ventil, ventil_zu);
  digitalWrite(m3_ventil, ventil_zu);
  digitalWrite(m4_ventil, ventil_zu);
  return;
}

void b01_onPressedForDuration() {
  Serial.println("Button_01 Long pressed");
  if (sub_status_osmose_no == 3 && main_status_osmose_no == 1 && zeitmessung_status == 0) {
    zeitmessung_status = 1;
    reinwasser_ms = 3600000;
    EasyBuzzer.beep(
      1500,  // Frequency in hertz(HZ).
      500,   // On Duration in milliseconds(ms).
      500,   // Off Duration in milliseconds(ms).
      1,     // The number of beeps per cycle.
      1000,  // Pause duration.
      1);    // The number of cycle.

  } else if (sub_status_osmose_no == 3 && main_status_osmose_no == 1 && zeitmessung_status == 1) {
    zeitmessung_status = 0;
    reinwasser_ms = (start_phasen_ms - phasen_ms) * 1000L;
    reinwasser_01_ms = reinwasser_ms;
    EEPROMWritelong(1, reinwasser_01_ms);
    EasyBuzzer.beep(
      1500,  // Frequency in hertz(HZ).
      500,   // On Duration in milliseconds(ms).
      100,   // Off Duration in milliseconds(ms).
      1,     // The number of beeps per cycle.
      500,   // Pause duration.
      2);    // The number of cycle.
  }
}

void b02_onPressedForDuration() {
  Serial.println("Button_02 Long pressed");
  if (sub_status_osmose_no == 3 && main_status_osmose_no == 2 && zeitmessung_status == 0) {
    zeitmessung_status = 1;
    reinwasser_ms = 3600000;
    EasyBuzzer.beep(
      1500,  // Frequency in hertz(HZ).
      500,   // On Duration in milliseconds(ms).
      500,   // Off Duration in milliseconds(ms).
      1,     // The number of beeps per cycle.
      1000,  // Pause duration.
      1);    // The number of cycle.

  } else if (sub_status_osmose_no == 3 && main_status_osmose_no == 2 && zeitmessung_status == 1) {
    zeitmessung_status = 0;
    reinwasser_ms = (start_phasen_ms - phasen_ms) * 1000L;
    reinwasser_02_ms = reinwasser_ms;
    EEPROMWritelong(5, reinwasser_02_ms);
    EasyBuzzer.beep(
      1500,  // Frequency in hertz(HZ).
      500,   // On Duration in milliseconds(ms).
      100,   // Off Duration in milliseconds(ms).
      1,     // The number of beeps per cycle.
      500,   // Pause duration.
      2);    // The number of cycle.
  }
}

void b01_onPressed() {


  Serial.println("Button_01 short pressed");
  if (sub_status_osmose_no == 0) {
    // Alles aus

    sub_status_osmose_no = 1;
    main_status_osmose_no = 1;
    reinwasser_ms = reinwasser_01_ms;
    if (millis() >= next_stehwasserspuelung) stehwasserspuelung_ms= init_stehwasserspuelung_ms;
    else stehwasserspuelung_ms= 0;
    start_ms = millis();
    EasyBuzzer.beep(
      3600,  // Frequency in hertz(HZ).
      150,   // On Duration in milliseconds(ms).
      150,   // Off Duration in milliseconds(ms).
      1,     // The number of beeps per cycle.
      1000,  // Pause duration.
      1);    // The number of cycle.



  } else if (main_status_osmose_no == 1) {
    sub_status_osmose_no = 0;
    main_status_osmose_no = 0;
    EasyBuzzer.beep(
      3600,  // Frequency in hertz(HZ).
      150,   // On Duration in milliseconds(ms).
      150,   // Off Duration in milliseconds(ms).
      2,     // The number of beeps per cycle.
      1000,  // Pause duration.
      1);    // The number of cycle.
  }
}

void b02_onPressed() {


  Serial.println("Button_02 short pressed");
  if (sub_status_osmose_no == 0) {
    // Alles aus

    sub_status_osmose_no = 1;
    main_status_osmose_no = 2;
    reinwasser_ms = reinwasser_02_ms;
    if (millis() >= next_stehwasserspuelung) stehwasserspuelung_ms= init_stehwasserspuelung_ms;
    else stehwasserspuelung_ms= 0;
    start_ms = millis();
    EasyBuzzer.beep(
      3600,  // Frequency in hertz(HZ).
      150,   // On Duration in milliseconds(ms).
      150,   // Off Duration in milliseconds(ms).
      1,     // The number of beeps per cycle.
      1000,  // Pause duration.
      1);    // The number of cycle.



  } else if (main_status_osmose_no == 2) {
    sub_status_osmose_no = 0;
    main_status_osmose_no = 0;
    EasyBuzzer.beep(
      3600,  // Frequency in hertz(HZ).
      150,   // On Duration in milliseconds(ms).
      150,   // Off Duration in milliseconds(ms).
      2,     // The number of beeps per cycle.
      1000,  // Pause duration.
      1);    // The number of cycle.
  }
}
void ausgabe(String Z01, String Z02, String Z03, String Z04, String Z05,String Z06,String Z07) {
  Serial.write(27);
  Serial.write(91);
  Serial.write("10A");  // VT100 5x Cursor UP
  Serial.print(Z01);
  Serial.println("                         ");
  Serial.print(Z02);
  Serial.println("                         ");
  Serial.print(Z03);
  Serial.println("                         ");
  Serial.print(Z04);
  Serial.println("                         ");
  Serial.print(Z05);
  Serial.println("                         ");
  Serial.print(Z06);
  Serial.println("                         ");
  Serial.print(Z07);
  Serial.println("                         ");
}

void EEPROMWritelong(int address, long value) {
  byte four = (value & 0xFF);
  byte three = ((value >> 8) & 0xFF);
  byte two = ((value >> 16) & 0xFF);
  byte one = ((value >> 24) & 0xFF);

  EEPROM.update(address, four);
  EEPROM.update(address + 1, three);
  EEPROM.update(address + 2, two);
  EEPROM.update(address + 3, one);
}

long EEPROMReadlong(long address) {
  long four = EEPROM.read(address);
  long three = EEPROM.read(address + 1);
  long two = EEPROM.read(address + 2);
  long one = EEPROM.read(address + 3);

  return ((four << 0) & 0xFF) + ((three << 8) & 0xFFFF) + ((two << 16) & 0xFFFFFF) + ((one << 24) & 0xFFFFFFFF);
}



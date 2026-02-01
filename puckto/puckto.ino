#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "TLC5947.h"

#include <string> 
#include <sstream>  

#include <Arduino.h>
#include "hardware/flash.h"
#include "hardware/sync.h"
#include <EEPROM.h>

#define FLASH_TARGET_OFFSET 0x100000  // 1 MB mark, must be sector aligned
#define DATA_COUNT 30

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1   
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

//Storage and settings
const uint8_t *flash_ptr = (const uint8_t *)(XIP_BASE + FLASH_TARGET_OFFSET);
int save[DATA_COUNT] = {0,50,50,10,100,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
int storedData[DATA_COUNT];
bool flashLoaded = false;
int setings = 1;
int options = 0;
int options_menu = 0;



//pins
const int PIN_ENC_CLK = 15;  // CLK
const int PIN_ENC_DT  = 14;  // DT
const int PIN_ENC_SW  = 13;  // knapp (SW)

const int PIN_I2C_SDA = 16;  // OLED SDA
const int PIN_I2C_SCL = 17;  // OLED SCL

const int CLOCK = 3;
const int DATA  = 2;
const int LATCH = 4;
const int BLANK = 5;

int DEVICES = 1;                          //  set Amount of TLC5947 Boards
int led = 36 * DEVICES;                   //  this set Amount of LEDs per Board
int wait = 100; 

TLC5947 tlc(DEVICES, CLOCK, DATA, LATCH, BLANK);


const int ZERO_B = 0;                     //  zero brightness
const int MAX_B = 4095;                   //  100% brightness 4095
int a = 4000;                            //  current brightness

enum MenuMode {
  MENU_MAIN,
  MENU_SUB
};

const char* mainItems[] = {
  "Blink speed",
  "Intensity",
  "Color",
  "run",
  "pre saves"
};
const int MAIN_ITEM_COUNT = 5;

const char* colors[] = {
  "BLue",
  "Yellow",
};
const int COLOR_COUNT = 2;



MenuMode menuMode = MENU_MAIN;
int selectedIndex = 0;     
int activeSubmenu = -1;   


int blinkSpeed[] = {50, 50, 10}; // [on, off, program längd sek] 
int blink_select=0;
      
int brightness = 5;        
int colorIndex = 0;     

bool needRedraw = true;

//potentiometer/knapp
int lastClkState;
bool lastButtonState = HIGH;
unsigned long lastButtonTime = 0;
const unsigned long BUTTON_DEBOUNCE_MS = 200;

//Hjälpfunktioner
int clampInt(int v, int lo, int hi) {
  if (v < lo) return lo;
  if (v > hi) return hi;
  return v;
}

int readEncoderStep() {
  static uint8_t lastState = 0;
  static int8_t stepSum = 0;
  uint8_t state = (digitalRead(PIN_ENC_CLK) << 1) |
                   digitalRead(PIN_ENC_DT);
  int8_t delta = 0;

  if      (lastState == 0b00 && state == 0b01) delta = +1;
  else if (lastState == 0b01 && state == 0b11) delta = +1;
  else if (lastState == 0b11 && state == 0b10) delta = +1;
  else if (lastState == 0b10 && state == 0b00) delta = +1;

  else if (lastState == 0b00 && state == 0b10) delta = -1;
  else if (lastState == 0b10 && state == 0b11) delta = -1;
  else if (lastState == 0b11 && state == 0b01) delta = -1;
  else if (lastState == 0b01 && state == 0b00) delta = -1;

  lastState = state;

  if (delta != 0) {
    stepSum += delta;
    if (stepSum >= 4) {
      stepSum = 0;
      return -1;
    }
    else if (stepSum <= -4) {
      stepSum = 0;
      return +1;
    }
  }

  return 0;
}

bool buttonPressed() {
  bool pressed = false;
  bool current = digitalRead(PIN_ENC_SW);
  unsigned long now = millis();

  if (current != lastButtonState) {
    if (now - lastButtonTime > BUTTON_DEBOUNCE_MS) {
      lastButtonTime = now;
      if (current == LOW && lastButtonState == HIGH) {
        pressed = true;
      }
    }
    lastButtonState = current;
  }

  return pressed;
}

void drawMainMenu() {    // Ritar menyer
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  

  for (int i = 0; i < MAIN_ITEM_COUNT; i++) {
    int y = 2+ i * 12;
    display.setCursor(0, y);
    if (i == selectedIndex) {
      display.print("> ");
    } else {
      display.print("  ");
    }
    display.println(mainItems[i]);
  }

  display.display();
}

void drawSubmenu() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  if (activeSubmenu == 0) { // Blinkhastighet
    display.setCursor(0, 0);
    display.println("Blink speed");
    display.setCursor(0, 14);
    if (blink_select==0) display.print("> ");
    display.print("on: ");
    display.print(blinkSpeed[0]);
    display.println(" ms");
    display.setCursor(0, 28);
    if (blink_select==1) display.print("> ");
    display.print("off: ");
    display.print(blinkSpeed[1]);
    display.println(" ms");
    display.setCursor(0, 42);
    if (blink_select==2) display.print("> ");
    display.print("runtime:");
    display.print(blinkSpeed[2]);
    display.println(" S");
  } else if (activeSubmenu == 1) { // Ljusstyrka
    display.setCursor(0, 0);
    display.println("Intensity");
    display.setCursor(0, 16);
    display.print("Value: ");
    display.println(a);
  } else if (activeSubmenu == 2) { // Farg
    display.setCursor(0, 0);
    display.println("Color");
    display.setCursor(0, 16);
    display.print("Value: ");
    display.println(colors[colorIndex]);
    display.println(colorIndex);

  } else if (activeSubmenu == 3) { // run
   //Lampa
    int start = 0;
    float runs = blinkSpeed[2]*1000/(blinkSpeed[0]+blinkSpeed[1]+18.0198019802); //+ 18 då det tar tid att tända/släcka lamporna

    unsigned long startTime;
    unsigned long endTime;

    // ! viktikgt ! 
    // Då det tar tid att stänga av/ på lamporna måste jag koreskera för det. 
    // Lista ut hur snabt det tar att stänga av / på 
    // ! viktikgt !

    if (colorIndex==1) start=1;
    startTime = millis();
    for (float z=0.0; z<runs; z++){
      for (int i = start; i < led; i+=2){
        tlc.setPWM(i, a);
        tlc.write();
      }
      delay(blinkSpeed[0]);
      for (int i = start; i < led; i+=2){
        tlc.setPWM(i, 0);
        tlc.write();
      }
      delay(blinkSpeed[1]);  
    }
    endTime = millis();
    display.setCursor(0, 48);
    display.print(endTime-startTime);

    display.print("ms | ");
    display.println(runs);

  } else if (activeSubmenu == 4 && options_menu==0) {
    //display.setCursor(0, 32);
    //display.println("testtttt");
    Read(storedData);
    String name = " ";
    int shit_variabel=0;
    for (int i=5; i<30; i++){
      name+= String(save[i]) + ".";
      if((i + 1) % 5 == 0){
        int y = 2 + shit_variabel*12;
        display.setCursor(0, y);
        if (i+1==setings*5 + 5){
          display.print("> ");
        }else {
          display.print("  ");
        }
        display.println(name);
        name = " ";
        shit_variabel++;
      }

    }

  } else if (activeSubmenu == 4 && options_menu==1) {
    const char * options_list[]={"Use", "Save", "Cancel"};
        display.setCursor(10, 26);
        for (int i=0; i<3; i++) {
          if (options==i){
            display.print("[");
            display.print(options_list[i]);
            display.print("]");
          } else{ 
            display.print(" ");
            display.print(options_list[i]);
            display.print(" ");
          }
        }
        display.println();
  }



  //display.setCursor(0, 48);
  //display.println("Press to go back");

  display.display();
}

void redraw() {
  if (!needRedraw) return;

  if (menuMode == MENU_MAIN) {
    drawMainMenu();
  } else {
    drawSubmenu();
  }

  needRedraw = false;
}
// save
void Save(int start) {
  save[start*5+0] = colorIndex;
  save[start*5+1] = blinkSpeed[0];
  save[start*5+2] = blinkSpeed[1];
  save[start*5+3] = blinkSpeed[2];
  save[start*5+4] = a;

  // Write all 30 integers to EEPROM
  for (int i = 0; i < DATA_COUNT; i++) {
    int addr = i * sizeof(int);  // each int takes 4 bytes
    EEPROM.put(addr, save[i]);
  }

  EEPROM.commit(); // push changes to flash
}



// --- Read settings from EEPROM ---
void Read(int *buffer) {
  for (int i = 0; i < DATA_COUNT; i++) {
    int addr = i * sizeof(int);
    EEPROM.get(addr, buffer[i]);
  }
}

// --- Load a specific preset ---
void Load(int start) {
  Read(storedData);
  // basic sanity check
  //if (storedData[0] < 0 || storedData[0] > 5000) return;

  colorIndex    = storedData[start*5+0];
  blinkSpeed[0] = storedData[start*5+1];
  blinkSpeed[1] = storedData[start*5+2];
  blinkSpeed[2] = storedData[start*5+3];
  a             = storedData[start*5+4];

  flashLoaded = true;
}



//setup & main loop
void setup() {
  Serial.begin(115200);
  // Initialize EEPROM with enough size for 30 ints (4 bytes each = 120 bytes)
  EEPROM.begin(DATA_COUNT * sizeof(int));
  // other init code...
  Load(0);  // load first preset at startup

  Serial.begin(115200);                   //  initialize Serial Output
  Serial.println(__FILE__);
  Serial.print("TLC5947_LIB_VERSION: \t");
  Serial.println(TLC5947_LIB_VERSION);

  tlc.begin();                            //  initialize TLC5947 library
  tlc.enable();

  pinMode(PIN_ENC_CLK, INPUT_PULLUP);
  pinMode(PIN_ENC_DT,  INPUT_PULLUP);
  pinMode(PIN_ENC_SW,  INPUT_PULLUP);

  lastClkState = digitalRead(PIN_ENC_CLK);
  lastButtonState = digitalRead(PIN_ENC_SW);


  Wire.setSDA(PIN_I2C_SDA);
  Wire.setSCL(PIN_I2C_SCL);
  Wire.begin();
  for (int i = 0; i < led; i++){
      tlc.setPWM(i, 0);
      tlc.write();
      Serial.print(i);
      Serial.print(" = ");
      Serial.println(a);
    }

  
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { 
    for (;;);
  }
  display.clearDisplay();
  display.display();
  needRedraw = true;
}

void loop() {
  // --- read inputs ---
  int step = readEncoderStep();        // encoder delta (may be 0)
  bool btnNow = buttonPressed();      // current raw button state (true while pressed)
  static bool btnPrev = false;        // remember last loop
  bool btnPressedEdge = (!btnPrev && btnNow); // true only on the rising edge
  btnPrev = btnNow;

  bool localNeedRedraw = false;

  // --- ROTARY handling (always processed) ---
  if (step != 0) {
    if (menuMode == MENU_MAIN) {
      selectedIndex += step;
      if (selectedIndex < 0) selectedIndex = MAIN_ITEM_COUNT - 1;
      if (selectedIndex >= MAIN_ITEM_COUNT) selectedIndex = 0;
      localNeedRedraw = true;
    } else { // MENU_SUB
      if (activeSubmenu == 0) {
        int mod = 10;
        if (blink_select==2) mod=1;
        blinkSpeed[blink_select] = step * mod + blinkSpeed[blink_select];
        if (blinkSpeed[blink_select] <= 0) blinkSpeed[blink_select] = 0;
        localNeedRedraw = true;

      } else if (activeSubmenu == 1) {
        if (a>500) a = step * 100 + a;
        if (a<=500) a = step * 10 + a;
        if (a <= 0) a = 0;
        if (a >4000) a=4000;
        localNeedRedraw = true;

      } else if (activeSubmenu == 2) {
        colorIndex += step;
        if (colorIndex < 0) colorIndex = COLOR_COUNT - 1;
        if (colorIndex >= COLOR_COUNT) colorIndex = 0;
        localNeedRedraw = true;
      
      } else if (activeSubmenu == 4 && options_menu==0){
        setings+= step;
        if (setings>5) setings=1;
        if (setings<1) setings=5;
        localNeedRedraw = true;
      } else if (activeSubmenu == 4 && options_menu==1){
        options+= step;
        if (options>2) options=0;
        if (options<0) options=2;
        localNeedRedraw = true;
      }
    }
  }

  // --- BUTTON handling (edge-detected) ---
  if (btnPressedEdge) {
    if (menuMode == MENU_MAIN) {
      // Enter selected submenu
      activeSubmenu = selectedIndex;
      menuMode = MENU_SUB;
      localNeedRedraw = true;
    } else if (activeSubmenu == 0){ // we are in MENU_SUB
      // Submenu 0 needs its own button semantics (toggle/select/exit)
        if (blink_select == 0) {
          // first press: switch to second blink selector
          blink_select = 1;
        }
        else if (blink_select == 1) {
          // first press: switch to second blink selector
          blink_select = 2;
        } else {
          // next press: return to main menu
          blink_select = 0;
          menuMode = MENU_MAIN;
          activeSubmenu = -1;
        }
        localNeedRedraw = true;
    } else if (activeSubmenu == 4) {
      if (options_menu==1){
        if (options==0){
          Load(setings);
          options_menu=0;
          menuMode = MENU_MAIN;
          activeSubmenu = -1;
        } else if (options==1){
          Save(setings);
          options_menu=0;
          menuMode = MENU_MAIN;
          activeSubmenu = -1;
        } else if (options==2){
          options_menu=0;
          menuMode = MENU_MAIN;
          activeSubmenu = -1;
        } 
      } else {
        options_menu=1;
      }
      localNeedRedraw = true;
        
    } else {
        // Other submenus: single button press -> back to main
        menuMode = MENU_MAIN;
        activeSubmenu = -1;
        Save(0);
        localNeedRedraw = true;
      }
    }
  // Apply redraw flag
  if (localNeedRedraw) needRedraw = true;

  // Finally redraw
  redraw();
}

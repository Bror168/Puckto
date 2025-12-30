#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "TLC5947.h"

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1   
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

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
int a = 4;                            //  current brightness

enum MenuMode {
  MENU_MAIN,
  MENU_SUB
};

const char* mainItems[] = {
  "Blink speed",
  "Intensity",
  "Color",
  "run"
};
const int MAIN_ITEM_COUNT = 4;

const char* colors[] = {
  "BLue",
  "Yellow",
};
const int COLOR_COUNT = 2;



MenuMode menuMode = MENU_MAIN;
int selectedIndex = 0;     
int activeSubmenu = -1;   


int blinkSpeed[] = {5,5}; // [on, off] 
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
  int step = 0;
  int clkState = digitalRead(PIN_ENC_CLK);

  if (clkState != lastClkState) {
    if (clkState == HIGH) {
      if (digitalRead(PIN_ENC_DT) == LOW) {
        step = +1;
      } else {
        step = -1;
      }
    }
    lastClkState = clkState;
  }

  return step;
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

  display.setCursor(0, 0);
  display.println("MENU");

  for (int i = 0; i < MAIN_ITEM_COUNT; i++) {
    int y = 16 + i * 12;
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
    display.setCursor(0, 16);
    display.print("on: ");
    display.println(blinkSpeed[0]);
    display.println(" ms");
    display.setCursor(0, 32);
    display.print("off: ");
    display.println(blinkSpeed[1]);
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
    if (colorIndex==1) start=1;
    for (int i = start; i < led; i+=2){
      tlc.setPWM(i, a);
      tlc.write();
      Serial.print(i);
      Serial.print(" = ");
      Serial.println(a);
    }
    delay(wait);
    for (int i = 0; i < led; i++){
      tlc.setPWM(i, 0);
      tlc.write();
      Serial.print(i);
      Serial.print(" = ");
      Serial.println(a);
    }
    
  }

  display.setCursor(0, 48);
  display.println("Press to go back");

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

//setup & main loop
void setup() {
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
        // encoder adjusts blinkSpeed for the currently selected blink index
        blinkSpeed[blink_select] = step * 10 + blinkSpeed[blink_select];
        if (blinkSpeed[blink_select] <= 0) blinkSpeed[blink_select] = 1;
        localNeedRedraw = true;

      } else if (activeSubmenu == 1) {
        a = step * 10 + a;
        if (a <= 0) a = 1;
        localNeedRedraw = true;

      } else if (activeSubmenu == 2) {
        colorIndex += step;
        if (colorIndex < 0) colorIndex = COLOR_COUNT - 1;
        if (colorIndex >= COLOR_COUNT) colorIndex = 0;
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
    } else { // we are in MENU_SUB
      // Submenu 0 needs its own button semantics (toggle/select/exit)
      if (activeSubmenu == 0) {
        if (blink_select == 0) {
          // first press: switch to second blink selector
          blink_select = 1;
        } else {
          // next press: return to main menu
          blink_select = 0;
          menuMode = MENU_MAIN;
          activeSubmenu = -1;
        }
        localNeedRedraw = true;
      } else {
        // Other submenus: single button press -> back to main
        menuMode = MENU_MAIN;
        activeSubmenu = -1;
        localNeedRedraw = true;
      }
    }
  }

  // Apply redraw flag
  if (localNeedRedraw) needRedraw = true;

  // Finally redraw
  redraw();
}

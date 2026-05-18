#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "TLC5947.h"

#include <string> 
#include <sstream>  

#include <Arduino.h>
#include "hardware/flash.h"
#include "hardware/sync.h"

// --- KONFIGURATION & MINNE ---
#define FLASH_TARGET_OFFSET (256 * 1024) 
#define DATA_COUNT 30

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1   
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

const uint8_t *flash_ptr = (const uint8_t *)(XIP_BASE + FLASH_TARGET_OFFSET);
int save[DATA_COUNT] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
int storedData[DATA_COUNT];
bool flashLoaded = false;
int setings = 1;
int options = 0;
int options_menu = 0;

// --- PIN-DEFINITIONER ---
const int PIN_ENC_CLK = 15;  
const int PIN_ENC_DT  = 14;  
const int PIN_ENC_SW  = 13;  

const int PIN_I2C_SDA = 16;  
const int PIN_I2C_SCL = 17;  

const int CLOCK = 3;
const int DATA  = 2;
const int LATCH = 4;
const int BLANK = 5;

int DEVICES = 1;                          
int led = 36 * DEVICES;                   
int wait = 100; 

TLC5947 tlc(DEVICES, CLOCK, DATA, LATCH, BLANK);

const int ZERO_B = 0;                     
const int MAX_B = 4095;                   
int a = 4000;                             

// --- MENY-STRUKTUR ---
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

int blinkSpeed[] = {1, 2, 10}; //Sekunder prefix här
int blink_select=0;
int mod=60;
      
int brightness = 5;        
int colorIndex[] = {0, 0, 0, 0, 0};
int curent_color= 0;   

bool needRedraw = true;

int lastClkState;
bool lastButtonState = HIGH;
unsigned long lastButtonTime = 0;
const unsigned long BUTTON_DEBOUNCE_MS = 200;

// --- HJÄLPFUNKTIONER ---

// Håller värden inom ett säkert intervall
int clampInt(int v, int lo, int hi) {
  if (v < lo) return lo;
  if (v > hi) return hi;
  return v;
}

// Läser av encoderns steg och riktning
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

// Kollar om knappen tryckts ned (med avstudsning)
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

// --- GRAFIK & LOGIK ---

// Ritar upp huvudmenyns val
void drawMainMenu() {    
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

// Hanterar alla undermenyer och deras funktioner
void drawSubmenu() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  if (activeSubmenu == 0) { // BLINK SPEED: Adjusts On/Off times and duration
    display.setCursor(0, 0);
    display.println("Blink speed");
    display.setCursor(0, 14);
    if (blink_select == 0) display.print("> ");
    display.print("on: ");
    display.print(blinkSpeed[0]);
    display.println(" S");
    display.setCursor(0, 28);
    if (blink_select == 1) display.print("> ");
    display.print("off: ");
    display.print(blinkSpeed[1]);
    display.println(" S");
    display.setCursor(0, 42);
    if (blink_select == 2) display.print("> ");
    display.print("runtime: ");
    display.print((int)(blinkSpeed[2] / 60));
    display.print("|m ");
    if (blink_select == 2 && mod == 1) display.print("*");
    display.print(blinkSpeed[2] % 60);
    display.print("|s");
    
  } else if (activeSubmenu == 1) { // INTENSITY: Changes brightness
    display.setCursor(0, 0);
    display.println("Intensity");
    display.setCursor(0, 16);
    display.print("Value: ");
    display.println(a);
    
  } else if (activeSubmenu == 2) { // COLOR: Selects color
    display.setCursor(0, 0);
    display.println("Color");
    display.setCursor(0, 16);
    for (int i=0; i<5; i++){
      if (curent_color == i) {
        display.print("[");
        display.print(colors[colorIndex[i]][0]);
        display.print("], ");
      } else{
        display.print(colors[colorIndex[i]][0]);
        display.print(", ");
      }
    }
    display.println();

  } else if (activeSubmenu == 3) { // RUN: Runs the program with time correction

    float runs = blinkSpeed[2]/(blinkSpeed[0]+blinkSpeed[1]); // Seconds prefix here
    bool stop = false;

    unsigned long startTime;
    unsigned long endTime;

    startTime = millis();
    
    for (float z=0.0; z<runs; z++){
      display.clearDisplay();
      display.setCursor(10, 28); 
      display.println("- press to cancel -");
      display.setCursor(38, 50); 
      display.print((int)(z/runs*100));
      display.println("% done");
      display.display();
      
      for (int i = colorIndex[curent_color]; i < led; i+=2){
        tlc.setPWM(i, a);
        tlc.write();
      }
      stop = vanta(blinkSpeed[0]*1000-23, stop);
      
      for (int i = colorIndex[curent_color]; i < led; i+=2){
        tlc.setPWM(i, 0);
        tlc.write();
      }
      stop = vanta(blinkSpeed[1]*1000-23, stop); 
    
      if (stop) { // Pause menu on button press
        for (int nedrakning = 10; nedrakning > 0; nedrakning--) {
          display.clearDisplay();
          display.setCursor(22, 14);
          display.println("- Cancelling -");
          display.setCursor(25, 28);
          display.println("Are you sure?");
          display.setCursor(16, 42);
          display.print("Continuing in ");
          display.print(nedrakning);
          display.println("-");
          display.display();
          bool avbruten = false;
          while(buttonPressed());
          for (int j = 0; j < 20; j++) {
            if (buttonPressed()) {
              z += runs;      
              avbruten = true; 
              break;          
            }
            delay(50); 
          } 
          if (avbruten) {
            break; 
          }
        }
        display.clearDisplay();
        display.display();
        stop=false;
      }
      display.clearDisplay();
      endTime = millis();
      display.setCursor(0, 48);
      display.print(endTime-startTime);

      display.print("ms | ");
      display.println(runs);
      curent_color++;
      if (curent_color>4) curent_color=0;
    }
    curent_color=0;
    
  } else if (activeSubmenu == 4 && options_menu==0) { // PRE-SAVES: Shows saved slots
    for (int slot = 1; slot <= 5; slot++) {
      int y = 2 + (slot - 1) * 12;
      display.setCursor(0, y);

      if (setings == slot) {
        display.print("> ");
      } else {
        display.print("  ");
      }

      int mem_color = save[slot*5 + 0];
      int mem_a = save[slot*5 + 4];

      if (mem_color !=0 && mem_color !=1) {
        display.print("Save ");
        display.print(slot);
        display.println(": Empty");
      } else {
        display.print("Save ");
        display.print(slot);
        display.print(":");
        display.println(colors[mem_color]);
      }
    } 
    
  } else if (activeSubmenu == 4 && options_menu==1) { // PRE-SAVES: Selection menu (Use/Save/Cancel)
    const char * options_list[]={"Use", "Save", "Cancel"};
    display.clearDisplay();
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
    display.display();
  }

  display.display();
}

// Uppdaterar skärmen vid behov
void redraw() {
  if (!needRedraw) return;

  if (menuMode == MENU_MAIN) {
    drawMainMenu();
  } else {
    drawSubmenu();
  }

  needRedraw = false;
}

bool vanta(int ms, bool stop) { // stödfunktion för att avryta blinkloopen
  if (stop) return true;
  unsigned long start = millis();
  while (millis() - start < ms) {
    if (buttonPressed()) return true; 
  }
  return false;
}

// --- MINNES-FUNKTIONER (FLASH) ---

// Sparar aktuella värden till valt minnesställe
void Save(int start) {
  save[start*5+0] = colorIndex;
  save[start*5+1] = blinkSpeed[0];
  save[start*5+2] = blinkSpeed[1];
  save[start*5+3] = blinkSpeed[2]; 
  save[start*5+4] = a;
  
  uint32_t ints = save_and_disable_interrupts();
  flash_range_erase(FLASH_TARGET_OFFSET, FLASH_SECTOR_SIZE);
  flash_range_program(
    FLASH_TARGET_OFFSET,
    (uint8_t *)save,
    sizeof(save)
  );
  restore_interrupts(ints);
}

// Läser rådata från flash-minnet
void Read(int *buffer) {
  memcpy(buffer, flash_ptr, sizeof(save));
}

// Laddar sparade värden till variablerna
void Load(int start) {
  Read(storedData);

  if (storedData[start*5+4] < 0 || storedData[start*5+4] > 4095) return;
  
  colorIndex    = storedData[start*5+0];
  blinkSpeed[0] = storedData[start*5+1];
  blinkSpeed[1] = storedData[start*5+2];
  blinkSpeed[2] = storedData[start*5+3];
  a             = storedData[start*5+4];
  
  flashLoaded = true;
}

// --- START & LOOP ---

// Startar hårdvara och laddar inställningar
void setup() {
  Serial.begin(115200);                                  
  Serial.println(__FILE__);
  Serial.print("TLC5947_LIB_VERSION: \t");
  Serial.println(TLC5947_LIB_VERSION);

  tlc.begin();                                            
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

  Read(save);

  display.clearDisplay();
  display.display();
  Load(0);
  needRedraw = true;
}


// Laddar sparade värden till variablerna
void loop() {
  int step = readEncoderStep();         
  bool btnNow = buttonPressed();      
  static bool btnPrev = false;        
  bool btnPressedEdge = (!btnPrev && btnNow); 
  btnPrev = btnNow;

  bool localNeedRedraw = false;

  // --- ENCODER-LOGIK ---
  if (step != 0) {
    if (menuMode == MENU_MAIN) {
      selectedIndex += step;
      if (selectedIndex < 0) selectedIndex = MAIN_ITEM_COUNT - 1;
      if (selectedIndex >= MAIN_ITEM_COUNT) selectedIndex = 0;
      localNeedRedraw = true;
    } else { 
      if (activeSubmenu == 0) {
        blinkSpeed[blink_select] = (step * mod) + blinkSpeed[blink_select];
        if (blinkSpeed[blink_select] < 1) blinkSpeed[blink_select] = 1;
        localNeedRedraw = true;

      } else if (activeSubmenu == 1) {
        if (a > 500) a = step * 100 + a;
        else a = step * 10 + a;
        if (a < 0) a = 0;
        if (a > 4000) a = 4000;
        localNeedRedraw = true;

      } else if (activeSubmenu == 2) {
        colorIndex[curent_color] += step;
        if (colorIndex[curent_color] < 0) colorIndex[curent_color] = COLOR_COUNT - 1;
        if (colorIndex[curent_color] >= COLOR_COUNT) colorIndex[curent_color] = 0;
        localNeedRedraw = true;
      
      } else if (activeSubmenu == 4 && options_menu == 0){
        setings += step;
        if (setings > 5) setings = 1;
        if (setings < 1) setings = 5;
        localNeedRedraw = true;

      } else if (activeSubmenu == 4 && options_menu == 1){
        options += step;
        if (options > 2) options = 0;
        if (options < 0) options = 2;
        localNeedRedraw = true;
      }
    }
  }

  // --- KNAPP-LOGIK ---
  if (btnPressedEdge) {
    if (menuMode == MENU_MAIN) {
      activeSubmenu = selectedIndex;
      menuMode = MENU_SUB;
      blink_select = 0;
      mod = 1;
      localNeedRedraw = true;

    } else if (activeSubmenu == 0) { 
      if (blink_select == 0) {
        blink_select = 1;
      } else if (blink_select == 1) {
        blink_select = 2;
        mod = 1;
      } else if (blink_select == 2) {
        if (mod == 1) {
          mod = 60;
        } else { 
          mod = 1;
          blink_select = 0;
          menuMode = MENU_MAIN;
          activeSubmenu = -1;
        }
      }
      localNeedRedraw = true;

    } else if (activeSubmenu == 2) { 
      if (curent_color == 0) {
        curent_color = 1;
      } else if (curent_color == 1) {
        curent_color = 2;
      } else if (curent_color == 2) {
        curent_color = 3;
      } else if (curent_color == 3) {
        curent_color = 4;
      } else if (curent_color == 4) {
        blink_select = 0;
        menuMode = MENU_MAIN;
        activeSubmenu = -1;
      }
      localNeedRedraw = true;

    }else if (activeSubmenu == 4) {
      if (options_menu == 1) {
        if (options == 0) Load(setings);
        else if (options == 1) Save(setings);
        
        options_menu = 0;
        menuMode = MENU_MAIN;
        activeSubmenu = -1;
        localNeedRedraw = true;
      } else {
        options_menu = 1;
        localNeedRedraw = true;
      }
    
    } else {
      menuMode = MENU_MAIN;
      activeSubmenu = -1;
      Save(0);
      localNeedRedraw = true;
    }
  }

  if (localNeedRedraw) needRedraw = true;
  redraw();
}
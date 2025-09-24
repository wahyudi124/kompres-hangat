#include <LiquidCrystal_I2C.h>

// Pin definitions
#define BTN_UP 10
#define BTN_DOWN 6
#define BTN_SELECT 3

// LCD I2C
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Variables
int currentTemp = 25; // Suhu saat ini
int tempOffset = 0;   // Offset suhu
int targetTemp = 40;  // Suhu target (40, 50, 60)
int therapyTime = 10; // Waktu terapi (10, 15, 20 menit)
int state = 0;        // State mesin
unsigned long lastButtonPress = 0;
unsigned long countdownStart = 0;
unsigned long therapyStart = 0;
unsigned long welcomeStart = 0;
unsigned long lastUpdate = 0;
bool screenNeedsUpdate = true;

// Button debouncing
bool lastBtnUp = HIGH, lastBtnDown = HIGH, lastBtnSelect = HIGH;
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50;

// Long press detection
unsigned long selectPressStart = 0;
bool selectPressed = false;
const unsigned long longPressTime = 2000; // 2 detik

void setup() {
  lcd.init();
  lcd.backlight();
  pinMode(BTN_UP, INPUT_PULLUP);
  pinMode(BTN_DOWN, INPUT_PULLUP);
  pinMode(BTN_SELECT, INPUT_PULLUP);
  
  // Welcome screen
  lcd.print("SISTEM TERAPI");
  lcd.setCursor(0, 1);
  lcd.print("KOMPRES PANAS");
  welcomeStart = millis();
  state = 0;
}

void loop() {
  switch(state) {
    case 0: handleWelcome(); break;
    case 1: handlePressAnyKey(); break;
    case 2: handleTempAdjust(); break;
    case 3: handleTargetTemp(); break;
    case 4: handleTherapyTime(); break;
    case 5: handleCountdown(); break;
    case 6: handleTherapy(); break;
  }
}

void handleWelcome() {
  if (millis() - welcomeStart >= 2000) {
    state = 1;
    showPressAnyKey();
  }
}

void showPressAnyKey() {
  lcd.clear();
  lcd.print("Tekan tombol");
  lcd.setCursor(0, 1);
  lcd.print("untuk mulai");
}

void handlePressAnyKey() {
  if (buttonPressed() && millis() - lastButtonPress > 300) {
    state = 2;
    showTempAdjust();
    lastButtonPress = millis();
  }
}

void showTempAdjust() {
  lcd.clear();
  lcd.print("Atur Suhu Saat:");
  updateTempDisplay();
}

void handleTempAdjust() {
  if (millis() - lastButtonPress > 200) {
    if (digitalRead(BTN_UP) == LOW) {
      tempOffset++;
      updateTempDisplay();
      lastButtonPress = millis();
    }
    else if (digitalRead(BTN_DOWN) == LOW) {
      tempOffset--;
      updateTempDisplay();
      lastButtonPress = millis();
    }
    else if (digitalRead(BTN_SELECT) == LOW) {
      state = 3;
      showTargetTemp();
      lastButtonPress = millis();
    }
  }
}

void updateTempDisplay() {
  lcd.setCursor(0, 1);
  lcd.print("                ");
  lcd.setCursor(0, 1);
  lcd.print(currentTemp + tempOffset);
  lcd.print("C (");
  lcd.print(tempOffset >= 0 ? "+" : "");
  lcd.print(tempOffset);
  lcd.print(")");
}

void showTargetTemp() {
  lcd.clear();
  lcd.print("Suhu Target:");
  updateTargetDisplay();
}

void handleTargetTemp() {
  if (millis() - lastButtonPress > 300) {
    if (digitalRead(BTN_UP) == LOW) {
      if (targetTemp == 40) targetTemp = 50;
      else if (targetTemp == 50) targetTemp = 60;
      else targetTemp = 40;
      updateTargetDisplay();
      lastButtonPress = millis();
    }
    else if (digitalRead(BTN_DOWN) == LOW) {
      if (targetTemp == 60) targetTemp = 50;
      else if (targetTemp == 50) targetTemp = 40;
      else targetTemp = 60;
      updateTargetDisplay();
      lastButtonPress = millis();
    }
    else if (digitalRead(BTN_SELECT) == LOW) {
      state = 4;
      showTherapyTime();
      lastButtonPress = millis();
    }
  }
}

void updateTargetDisplay() {
  lcd.setCursor(0, 1);
  lcd.print("                ");
  lcd.setCursor(0, 1);
  lcd.print(targetTemp);
  lcd.print("C");
}

void showTherapyTime() {
  lcd.clear();
  lcd.print("Waktu Terapi:");
  updateTimeDisplay();
}

void handleTherapyTime() {
  if (millis() - lastButtonPress > 300) {
    if (digitalRead(BTN_UP) == LOW) {
      if (therapyTime == 10) therapyTime = 15;
      else if (therapyTime == 15) therapyTime = 20;
      else therapyTime = 10;
      updateTimeDisplay();
      lastButtonPress = millis();
    }
    else if (digitalRead(BTN_DOWN) == LOW) {
      if (therapyTime == 20) therapyTime = 15;
      else if (therapyTime == 15) therapyTime = 10;
      else therapyTime = 20;
      updateTimeDisplay();
      lastButtonPress = millis();
    }
    else if (digitalRead(BTN_SELECT) == LOW) {
      state = 5;
      countdownStart = millis();
      showCountdown();
      lastButtonPress = millis();
    }
  }
}

void updateTimeDisplay() {
  lcd.setCursor(0, 1);
  lcd.print("                ");
  lcd.setCursor(0, 1);
  lcd.print(therapyTime);
  lcd.print(" menit");
}

void showCountdown() {
  lcd.clear();
  lcd.print("Bersiap...");
}

void handleCountdown() {
  // Check for long press to cancel
  if (checkLongPress()) {
    state = 1;
    showPressAnyKey();
    return;
  }
  
  unsigned long elapsed = millis() - countdownStart;
  int remaining = 3 - (elapsed / 1000);
  
  if (remaining > 0) {
    if (millis() - lastUpdate >= 1000) {
      lcd.setCursor(0, 1);
      lcd.print("                ");
      lcd.setCursor(0, 1);
      lcd.print("Mulai dalam: ");
      lcd.print(remaining);
      lastUpdate = millis();
    }
  } else {
    state = 6;
    therapyStart = millis();
    showTherapy();
  }
}

void showTherapy() {
  lcd.clear();
  lcd.print("TERAPI AKTIF");
}

void handleTherapy() {
  // Check for long press to stop therapy
  if (checkLongPress()) {
    lcd.clear();
    lcd.print("TERAPI DIHENTIKAN");
    lcd.setCursor(0, 1);
    lcd.print("Tekan tombol");
    while (!buttonPressed()) {} // Wait for button
    state = 1;
    tempOffset = 0;
    targetTemp = 40;
    therapyTime = 10;
    showPressAnyKey();
    return;
  }
  
  unsigned long elapsed = millis() - therapyStart;
  unsigned long totalTime = therapyTime * 60000UL;
  unsigned long remaining = totalTime - elapsed;
  
  if (remaining > 0) {
    if (millis() - lastUpdate >= 1000) {
      int minutes = remaining / 60000;
      int seconds = (remaining % 60000) / 1000;
      
      lcd.setCursor(0, 1);
      lcd.print("                ");
      lcd.setCursor(0, 1);
      lcd.print("Sisa: ");
      if (minutes < 10) lcd.print("0");
      lcd.print(minutes);
      lcd.print(":");
      if (seconds < 10) lcd.print("0");
      lcd.print(seconds);
      lastUpdate = millis();
    }
  } else {
    if (screenNeedsUpdate) {
      lcd.clear();
      lcd.print("TERAPI SELESAI");
      lcd.setCursor(0, 1);
      lcd.print("Tekan tombol");
      screenNeedsUpdate = false;
    }
    if (buttonPressed() && millis() - lastButtonPress > 300) {
      state = 1;
      tempOffset = 0;
      targetTemp = 40;
      therapyTime = 10;
      screenNeedsUpdate = true;
      showPressAnyKey();
      lastButtonPress = millis();
    }
  }
}

bool buttonPressed() {
  return (digitalRead(BTN_UP) == LOW || 
          digitalRead(BTN_DOWN) == LOW || 
          digitalRead(BTN_SELECT) == LOW);
}

bool readButton(int pin, bool &lastState) {
  bool reading = digitalRead(pin);
  if (reading != lastState) {
    lastDebounceTime = millis();
  }
  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (reading != lastState) {
      lastState = reading;
      return (reading == LOW); // Return true on press
    }
  }
  return false;
}

bool checkLongPress() {
  if (digitalRead(BTN_SELECT) == LOW) {
    if (!selectPressed) {
      selectPressStart = millis();
      selectPressed = true;
    }
    if (millis() - selectPressStart >= longPressTime) {
      selectPressed = false;
      return true;
    }
  } else {
    selectPressed = false;
  }
  return false;
}
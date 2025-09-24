#include <LiquidCrystal_I2C.h>

// Pin definitions
#define BTN_UP 10
#define BTN_DOWN 6
#define BTN_SELECT 3
#define LM35_PIN A2
#define RELAY_PUMP 13
#define RELAY_HEATER 12
#define FLOW_SENSOR_PIN 7

// LCD I2C
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Variables
int currentTemp = 25; // Suhu saat ini
unsigned long lastTempRead = 0;
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

// Cancel message
unsigned long cancelStart = 0;

// Flow sensor variables
volatile int flowPulseCount = 0;
float flowRate = 0;
float initialFlowRate = 0;
unsigned long flowCheckStart = 0;
bool flowStable = false;
bool therapyStarted = false;
const unsigned long flowStabilizeTime = 5000; // 5 detik
const unsigned long flowTimeoutTime = 10000; // 10 detik timeout

void setup() {
  lcd.init();
  lcd.backlight();
  pinMode(BTN_UP, INPUT_PULLUP);
  pinMode(BTN_DOWN, INPUT_PULLUP);
  pinMode(BTN_SELECT, INPUT_PULLUP);
  pinMode(RELAY_PUMP, OUTPUT);
  pinMode(RELAY_HEATER, OUTPUT);
  pinMode(FLOW_SENSOR_PIN, INPUT_PULLUP);
  
  // Attach interrupt for flow sensor
  attachInterrupt(digitalPinToInterrupt(FLOW_SENSOR_PIN), flowPulseCounter, FALLING);
  
  // Turn off relays initially
  digitalWrite(RELAY_PUMP, LOW);
  digitalWrite(RELAY_HEATER, LOW);
  
  // Welcome screen
  lcd.print("SISTEM TERAPI");
  lcd.setCursor(0, 1);
  lcd.print("KOMPRES PANAS");
  welcomeStart = millis();
  state = 0;
}

void loop() {
  // Update temperature every 500ms
  if (millis() - lastTempRead >= 500) {
    readTemperature();
    lastTempRead = millis();
  }
  
  // Control heater during therapy
  if (state == 6) {
    controlHeater();
    checkFlowRate();
  }
  
  // Calculate flow rate every second
  static unsigned long lastFlowCalc = 0;
  if (millis() - lastFlowCalc >= 1000) {
    calculateFlowRate();
    lastFlowCalc = millis();
  }
  
  switch(state) {
    case 0: handleWelcome(); break;
    case 1: handlePressAnyKey(); break;
    case 2: handleTempAdjust(); break;
    case 3: handleTargetTemp(); break;
    case 4: handleTherapyTime(); break;
    case 5: handleCountdown(); break;
    case 6: handleTherapy(); break;
    case 7: handleCancelMessage(); break;
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
  if (digitalRead(BTN_SELECT) == HIGH && digitalRead(BTN_UP) == HIGH && digitalRead(BTN_DOWN) == HIGH) {
    if (buttonPressed() && millis() - lastButtonPress > 300) {
      state = 2;
      showTempAdjust();
      lastButtonPress = millis();
    }
  }
}

void showTempAdjust() {
  lcd.clear();
  lcd.print("Atur Suhu Saat:");
  updateTempDisplay();
}

void handleTempAdjust() {
  // Update display every 500ms for realtime temperature
  static unsigned long lastTempUpdate = 0;
  if (millis() - lastTempUpdate >= 500) {
    updateTempDisplay();
    lastTempUpdate = millis();
  }
  
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
  // Debug: show raw ADC value
  int rawADC = analogRead(LM35_PIN);
  
  lcd.setCursor(0, 1);
  lcd.print("                ");
  lcd.setCursor(0, 1);
  lcd.print(currentTemp + tempOffset);
  lcd.print("C ADC:");
  lcd.print(rawADC);
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
  // Turn on pump during countdown
  digitalWrite(RELAY_PUMP, HIGH);
  // Reset flow monitoring
  flowStable = false;
  therapyStarted = false;
  flowCheckStart = millis();
  flowPulseCount = 0;
  initialFlowRate = 0;
}

void handleCountdown() {
  // Check for long press to cancel
  if (checkLongPress()) {
    // Wait until all buttons released
    while (digitalRead(BTN_SELECT) == LOW || digitalRead(BTN_UP) == LOW || digitalRead(BTN_DOWN) == LOW) {}
    // Turn off pump when cancelled
    digitalWrite(RELAY_PUMP, LOW);
    showCancelMessage();
    return;
  }
  
  // Check flow sensor timeout (10 detik)
  if (millis() - flowCheckStart > flowTimeoutTime && flowRate == 0) {
    digitalWrite(RELAY_PUMP, LOW);
    lcd.clear();
    lcd.print("Tidak ada aliran");
    lcd.setCursor(0, 1);
    lcd.print("air terdeteksi");
    delay(3000);
    showCancelMessage();
    return;
  }
  
  // Check if flow is stable for 5 seconds
  if (flowRate > 0 && !flowStable) {
    if (initialFlowRate == 0) {
      initialFlowRate = flowRate;
      flowCheckStart = millis();
    }
    if (millis() - flowCheckStart >= flowStabilizeTime) {
      flowStable = true;
      therapyStart = millis();
      state = 6;
      showTherapy();
      return;
    }
  }
  
  // Display flow status
  if (millis() - lastUpdate >= 1000) {
    lcd.setCursor(0, 1);
    lcd.print("                ");
    lcd.setCursor(0, 1);
    if (flowRate == 0) {
      lcd.print("Menunggu aliran");
    } else if (!flowStable) {
      int remaining = (flowStabilizeTime - (millis() - flowCheckStart)) / 1000;
      lcd.print("Stabilisasi: ");
      lcd.print(remaining + 1);
    }
    lastUpdate = millis();
  }
}

void showTherapy() {
  lcd.clear();
  lcd.print("TERAPI AKTIF");
}

void handleTherapy() {
  // Check for long press to stop therapy
  if (checkLongPress()) {
    // Wait until all buttons released
    while (digitalRead(BTN_SELECT) == LOW || digitalRead(BTN_UP) == LOW || digitalRead(BTN_DOWN) == LOW) {}
    
    // Turn off all relays
    digitalWrite(RELAY_PUMP, LOW);
    digitalWrite(RELAY_HEATER, LOW);
    
    tempOffset = 0;
    targetTemp = 40;
    therapyTime = 10;
    showCancelMessage();
    return;
  }
  
  // Check for flow rate drop (leak detection)
  if (flowRate < initialFlowRate / 2) {
    digitalWrite(RELAY_PUMP, LOW);
    digitalWrite(RELAY_HEATER, LOW);
    lcd.clear();
    lcd.print("SISTEM BOCOR!");
    lcd.setCursor(0, 1);
    lcd.print("Aliran menurun");
    delay(3000);
    tempOffset = 0;
    targetTemp = 40;
    therapyTime = 10;
    showCancelMessage();
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
      // Turn off all relays when therapy finished
      digitalWrite(RELAY_PUMP, LOW);
      digitalWrite(RELAY_HEATER, LOW);
      
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

void showCancelMessage() {
  lcd.clear();
  lcd.print("Sistem berhasil");
  lcd.setCursor(0, 1);
  lcd.print("dibatalkan");
  cancelStart = millis();
  state = 7;
}

void handleCancelMessage() {
  if (millis() - cancelStart >= 2000) {
    state = 1;
    lastButtonPress = millis() + 500;
    showPressAnyKey();
  }
}

void readTemperature() {
  // Single read - no delay for non-blocking
  int sensorValue = analogRead(LM35_PIN);
  
  // 1°C = 10mV (sesuai datasheet)
  // 5V / 1023 = 4.883 mV (5V = tegangan referensi, 1023 = resolusi 10 bit)
  float tempp =  sensorValue / 2.0479;
  
  // Jika sensor memberikan Fahrenheit, konversi ke Celsius
  // currentTemp = (int)((tempCelsius - 32.0) * 5.0 / 9.0);
  
  // LM35 sudah dalam Celsius
  currentTemp = (int)tempp;
}

void controlHeater() {
  int actualTemp = currentTemp + tempOffset;
  
  if (actualTemp < targetTemp) {
    digitalWrite(RELAY_HEATER, HIGH);
  }
  else if (actualTemp >= targetTemp + 2) {
    digitalWrite(RELAY_HEATER, LOW);
  }
}

void flowPulseCounter() {
  flowPulseCount++;
}

void calculateFlowRate() {
  // Calculate flow rate (L/min)
  // YF-S401 calibration factor: 5.5 pulses per liter
  flowRate = (flowPulseCount / 5.5);
  flowPulseCount = 0;
}

void checkFlowRate() {
  // Monitor flow rate during therapy
  if (initialFlowRate > 0) {
    // Check if flow dropped below 50% of initial rate
    if (flowRate < (initialFlowRate * 0.5)) {
      // Flow rate too low - possible leak
      digitalWrite(RELAY_PUMP, LOW);
      digitalWrite(RELAY_HEATER, LOW);
      lcd.clear();
      lcd.print("SISTEM BOCOR!");
      lcd.setCursor(0, 1);
      lcd.print("Flow: ");
      lcd.print(flowRate, 1);
      lcd.print(" L/min");
      delay(3000);
      
      // Reset and go to cancel message
      tempOffset = 0;
      targetTemp = 40;
      therapyTime = 10;
      showCancelMessage();
    }
  }
}
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <PinChangeInterrupt.h>  // Install dari Library Manager

// Pin definitions
#define BTN_UP 3
#define BTN_DOWN 6
#define BTN_SELECT 10
#define LM35_PIN A2
#define RELAY_PUMP 13
#define RELAY_HEATER 12
#define FLOW_SENSOR_PIN 7

// LCD I2C
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Variables
int currentTemp = 25; // Suhu saat ini
unsigned long lastTempRead = 0;

// Temperature averaging filter
const int TEMP_SAMPLES = 10;
int tempReadings[TEMP_SAMPLES];
int tempIndex = 0;
long tempTotal = 0;
bool tempArrayFilled = false;
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

// Flow sensor variables (YF-S401)
volatile unsigned long pulseCount = 0;
float flowRate = 0;
float initialFlowRate = 0;
unsigned long flowCheckStart = 0;
bool flowStable = false;
bool therapyStarted = false;
const unsigned long flowStabilizeTime = 5000; // 5 detik
const unsigned long flowTimeoutTime = 10000; // 10 detik timeout
const float PULSES_PER_LITER = 450.0; // YF-S401 kalibrasi
const unsigned long WINDOW_MS = 1000; // 1 detik window
unsigned long lastCalcMs = 0;
unsigned long lastCount = 0;

void setup() {

  pinMode(RELAY_PUMP, OUTPUT);
  pinMode(RELAY_HEATER, OUTPUT);
  digitalWrite(RELAY_PUMP, HIGH);
  digitalWrite(RELAY_HEATER, HIGH);
  lcd.init();
  lcd.backlight();
  pinMode(BTN_UP, INPUT_PULLUP);
  pinMode(BTN_DOWN, INPUT_PULLUP);
  pinMode(BTN_SELECT, INPUT_PULLUP);

  pinMode(FLOW_SENSOR_PIN, INPUT_PULLUP);
  
  // Initialize temperature filter array
  for (int i = 0; i < TEMP_SAMPLES; i++) {
    tempReadings[i] = 25; // Initialize with room temperature
  }
  tempTotal = 25 * TEMP_SAMPLES;
  
  // Attach Pin Change Interrupt for flow sensor (YF-S401) - pin 7
  attachPCINT(digitalPinToPCINT(FLOW_SENSOR_PIN), flowISR, FALLING);
  lastCalcMs = millis();
  
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
  unsigned long now = millis();
  if (now - lastCalcMs >= WINDOW_MS) {
    calculateFlowRate(now);
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
  digitalWrite(RELAY_PUMP, LOW);
  // Reset flow monitoring
  flowStable = false;
  therapyStarted = false;
  flowCheckStart = millis();
  pulseCount = 0;
  lastCount = 0;
  initialFlowRate = 0;
  lastCalcMs = millis();
}

void handleCountdown() {
  // Check for long press to cancel
  if (checkLongPress()) {
    // Wait until all buttons released
    while (digitalRead(BTN_SELECT) == LOW || digitalRead(BTN_UP) == LOW || digitalRead(BTN_DOWN) == LOW) {}
    // Turn off pump when cancelled
    digitalWrite(RELAY_PUMP, HIGH);
    showCancelMessage();
    return;
  }
  
  // Check flow sensor timeout (10 detik) - hanya jika benar-benar tidak ada aliran
  if (millis() - flowCheckStart > flowTimeoutTime && flowRate == 0) {
    digitalWrite(RELAY_PUMP, HIGH);
    lcd.clear();
    lcd.print("Pompa bermasalah");
    lcd.setCursor(0, 1);
    lcd.print("Tidak ada aliran");
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
  
  // Display flow status with flow rate
  if (millis() - lastUpdate >= 1000) {
    lcd.setCursor(0, 1);
    lcd.print("                ");
    lcd.setCursor(0, 1);
    if (flowRate == 0) {
      lcd.print("Flow: 0.0 L/min");
    } else if (!flowStable) {
      int remaining = (flowStabilizeTime - (millis() - flowCheckStart)) / 1000;
      lcd.print("Flow:");
      lcd.print(flowRate, 1);
      lcd.print(" S:");
      lcd.print(remaining + 1);
    }
    lastUpdate = millis();
  }
}

void showTherapy() {
  lcd.clear();
  lcd.print("TERAPI AKTIF");
  updateTherapyDisplay();
}

void updateTherapyDisplay() {
  lcd.setCursor(0, 1);
  lcd.print("                ");
  lcd.setCursor(0, 1);
  // Show temperature and flow
  lcd.print("T:");
  lcd.print(currentTemp + tempOffset);
  lcd.print("C F:");
  lcd.print(flowRate, 1);
}

void handleTherapy() {
  // Check for long press to stop therapy
  if (checkLongPress()) {
    // Wait until all buttons released
    while (digitalRead(BTN_SELECT) == LOW || digitalRead(BTN_UP) == LOW || digitalRead(BTN_DOWN) == LOW) {}
    
    // Turn off all relays
    digitalWrite(RELAY_PUMP, HIGH);
    digitalWrite(RELAY_HEATER, HIGH);
    
    tempOffset = 0;
    targetTemp = 40;
    therapyTime = 10;
    showCancelMessage();
    return;
  }
  
  // Check for flow rate drop (leak detection)
  if (flowRate < initialFlowRate / 2) {
    digitalWrite(RELAY_PUMP, HIGH);
    digitalWrite(RELAY_HEATER, HIGH);
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
      
      // Alternate display every 3 seconds
      static bool showTimer = true;
      static unsigned long displayToggle = 0;
      
      if (millis() - displayToggle >= 3000) {
        showTimer = !showTimer;
        displayToggle = millis();
      }
      
      lcd.setCursor(0, 1);
      lcd.print("                ");
      lcd.setCursor(0, 1);
      
      if (showTimer) {
        // Show remaining time
        lcd.print("Sisa: ");
        if (minutes < 10) lcd.print("0");
        lcd.print(minutes);
        lcd.print(":");
        if (seconds < 10) lcd.print("0");
        lcd.print(seconds);
      } else {
        // Show temperature and flow
        lcd.print("T:");
        lcd.print(currentTemp + tempOffset);
        lcd.print("C F:");
        lcd.print(flowRate, 1);
      }
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
      digitalWrite(RELAY_PUMP, HIGH);
      digitalWrite(RELAY_HEATER, HIGH);
      
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
  float temp_val = (sensorValue * 4.88);      /* Convert adc value to equivalent voltage */
  temp_val = (temp_val/10);
  
  int rawTemp = (int)temp_val;
  
  // Apply moving average filter
  tempTotal = tempTotal - tempReadings[tempIndex];
  tempReadings[tempIndex] = rawTemp;
  tempTotal = tempTotal + tempReadings[tempIndex];
  tempIndex = (tempIndex + 1) % TEMP_SAMPLES;
  
  // Check if array is filled for the first time
  if (tempIndex == 0 && !tempArrayFilled) {
    tempArrayFilled = true;
  }
  
  // Calculate average only if we have enough samples
  if (tempArrayFilled) {
    currentTemp = tempTotal / TEMP_SAMPLES;
  } else {
    // Use simple average for initial readings
    int validSamples = (tempIndex == 0) ? TEMP_SAMPLES : tempIndex;
    currentTemp = tempTotal / validSamples;
  }
}

void controlHeater() {
  int actualTemp = currentTemp + tempOffset;
  
  if (actualTemp < targetTemp) {
    digitalWrite(RELAY_HEATER, LOW); // Turn on heater
  }
  else if (actualTemp >= targetTemp + 2) {
    digitalWrite(RELAY_HEATER, HIGH); // Turn off heater
  }
}

void flowISR() {
  pulseCount++;
}

void calculateFlowRate(unsigned long now) {
  // Ambil delta pulsa dalam jendela waktu
  unsigned long countNow = pulseCount;
  unsigned long delta = countNow - lastCount;
  lastCount = countNow;
  
  // Hitung volume pada jendela (dalam liter)
  float windowSeconds = (now - lastCalcMs) / 1000.0;
  float litersWindow = delta / PULSES_PER_LITER;
  
  // L/min
  if (windowSeconds > 0) {
    flowRate = litersWindow * (60.0 / windowSeconds);
  }
  
  // Geser jendela
  lastCalcMs = now;
}

void checkFlowRate() {
  // Monitor flow rate during therapy
  if (initialFlowRate > 0) {
    // Check if flow dropped below 50% of initial rate
    if (flowRate < (initialFlowRate * 0.5)) {
      // Flow rate too low - ada masalah (bocor, sumbatan, dll)
      digitalWrite(RELAY_PUMP, HIGH);
      digitalWrite(RELAY_HEATER, HIGH);
      lcd.clear();
      lcd.print("ALIRAN BERMASALAH");
      lcd.setCursor(0, 1);
      lcd.print("Awal:");
      lcd.print(initialFlowRate, 1);
      lcd.print(" Kini:");
      lcd.print(flowRate, 1);
      delay(4000);
      
      // Reset and go to cancel message
      tempOffset = 0;
      targetTemp = 40;
      therapyTime = 10;
      showCancelMessage();
    }
  }
}
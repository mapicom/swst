#define G433_FAST
#define G433_SPEED 2000
#define EB_CLICK 2000

#include <Gyver433.h>
#include <LiquidCrystal.h>
#include <EEPROM.h>
#include <EncButton.h>

#define FIRMWARE_VERSION "1.1"
#define SIGNAL_TIMEOUT 10000
#define TONE_PIN 9
#define BLINK_INTERVAL 250
#define MENUS 6

// Addresses
#define SAVED_TZ_HOUR_OFFSET 0x1 // 4
#define SAVED_TZ_MIN_OFFSET SAVED_TZ_HOUR_OFFSET+4 // 4
#define SAVED_STATE_OFFSET SAVED_TZ_MIN_OFFSET+4 // 1
#define SAVED_SPEAKER_STATE_OFFSET SAVED_STATE_OFFSET+1 // 1
#define SAVED_TONE_FREQ_OFFSET SAVED_SPEAKER_STATE_OFFSET+1 // 2
#define SAVED_USB_STATE_OFFSET SAVED_TONE_FREQ_OFFSET+2 // 1

Gyver433_RX<2, 13> rx;
LiquidCrystal lcd(A4, A5, A7, A6, A1, A0);
EncButton<EB_TICK, 6, 5, 4> enc;

const char *months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

unsigned long timeoutTimer = 0;
unsigned long ledTimer = 0;
unsigned long menuTimer = 0;
bool justPrintedSignalLost = false;
uint8_t usingTXID = 0;
bool rxStarted = false;
uint8_t changeTZState = 0;
int8_t selectedInt = 0;
uint8_t speakerState = 1;
uint16_t toneFreq = 800;
uint8_t menu = 0;
bool isMenuChange = false;
bool menuSomethingChanged = false;
uint8_t usbState = 0;

long timezoneHours = 7;
long timezoneMinutes = 0;
long totalOffsetMinutes = timezoneHours * 60 + timezoneMinutes;

void renderMenu(uint8_t menuId, bool changeState = false) {
  if(menuId == 0) {
    if(menu != 0) {
      rxStarted = true;
      attachInterrupt(0, isr, CHANGE);
    }
    menu = 0;
    menuSomethingChanged = false;
  } else {
    if(changeState) menuTimer = 0;
    else menuTimer = millis();
    if(menu == 0) {
      detachInterrupt(0);
      rxStarted = false;
      timeoutTimer = 0;
    }
    menu = menuId;
    switch (menuId) {
      case 1:
        lcd.clear();
        lcd.setCursor(0, 0);
        if(!changeState) lcd.print("> SOUND");
        else lcd.print("  SOUND");
        lcd.setCursor(0, 1);
        if(!changeState) lcd.print("  " + String(speakerState));
        else lcd.print("> " + String(speakerState));
        break;
      case 2:
        lcd.clear();
        lcd.setCursor(0, 0);
        if(!changeState) lcd.print("> FREQ");
        else lcd.print("  FREQ");
        lcd.setCursor(0, 1);
        if(!changeState) lcd.print("  " + String(toneFreq));
        else lcd.print("> " + String(toneFreq));
        break;
      case 3:
        lcd.clear();
        lcd.setCursor(0, 0);
        if(!changeState) lcd.print("> USB");
        else lcd.print("  USB");
        lcd.setCursor(0, 1);
        if(!changeState) lcd.print("  " + String(usbState));
        else lcd.print("> " + String(usbState));
        break;
      case 4:
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("> INFO");
        break;
      case 5:
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("> SET TZ");
        break;
      case 6:
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("> RESET");
    }
  }
}

bool enterTZHour() {
  changeTZState = 1;
  selectedInt = 0;
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("HOUR:");
  lcd.setCursor(0, 1);
  lcd.print("+00:00");
  return true;
}

bool enterTZMin() {
  changeTZState = 2;
  selectedInt = 0;
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("MINUTE:");
  lcd.setCursor(0, 1);
  int aHour = timezoneHours;
  char formattedStr[8];
  if(timezoneHours < 0) aHour = -timezoneHours;
  else aHour = timezoneHours;
  sprintf(formattedStr, " %02d:%02d", aHour, selectedInt);
  if(timezoneHours < 0) formattedStr[0] = '-';
  else formattedStr[0] = '+';
  lcd.print(formattedStr);
  return true;
}

// Function to calculate the number of days in a given month and year
int daysInMonth(int month, int year) {
    static const int daysInMonth[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    int days = daysInMonth[month - 1];
    if (month == 2 && isLeapYear(year)) {
        days++;
    }
    return days;
}

// Function to check if a year is a leap year
bool isLeapYear(int year) {
    return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

void setup() {
  Serial.begin(115200);
  while(!Serial) delay(10);
  enc.setHoldTimeout(2000);
  pinMode(LED_BUILTIN, OUTPUT);
  lcd.begin(8, 2);
  uint8_t savedState;
  EEPROM.get(SAVED_STATE_OFFSET, savedState);
  if(savedState == 255) {
    timezoneHours = 0;
    timezoneMinutes = 0;
    toneFreq = 800;
    speakerState = 1;
    usbState = 0;
    EEPROM.put(SAVED_TZ_HOUR_OFFSET, timezoneHours);
    EEPROM.put(SAVED_TZ_MIN_OFFSET, timezoneMinutes);
    EEPROM.put(SAVED_SPEAKER_STATE_OFFSET, speakerState);
    EEPROM.put(SAVED_TONE_FREQ_OFFSET, toneFreq);
    EEPROM.put(SAVED_USB_STATE_OFFSET, usbState);
    rxStarted = false;
    timeoutTimer = 0;
    usingTXID = 0;
    lcd.clear();
    delay(1000);
    enterTZHour();
  } else {
    EEPROM.get(SAVED_SPEAKER_STATE_OFFSET, speakerState);
    EEPROM.get(SAVED_TONE_FREQ_OFFSET, toneFreq);
    if(toneFreq == 65535) {
      toneFreq = 800;
      EEPROM.put(SAVED_TONE_FREQ_OFFSET, toneFreq);
    }
    EEPROM.get(SAVED_USB_STATE_OFFSET, usbState);
    if(usbState == 255) {
      usbState = 0;
      EEPROM.put(SAVED_USB_STATE_OFFSET, usbState);
    }
    EEPROM.get(SAVED_TZ_HOUR_OFFSET, timezoneHours);
    EEPROM.get(SAVED_TZ_MIN_OFFSET, timezoneMinutes);
    totalOffsetMinutes = timezoneHours * 60 + timezoneMinutes;
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("USE UTC");
    lcd.setCursor(0, 1);
    char formattedStr[8];
    int aHour = timezoneHours;
    if(timezoneHours < 0) aHour = -timezoneHours;
    else aHour = timezoneHours;
    sprintf(formattedStr, " %02d:%02d", aHour, timezoneMinutes);
    if(timezoneHours < 0) formattedStr[0] = '-';
    else formattedStr[0] = '+';
    rxStarted = true;
    lcd.print(formattedStr);
    delay(2000);
    attachInterrupt(0, isr, CHANGE);
    lcd.clear();
    lcd.setCursor(0, 1);
    lcd.print("AWAITING");
    lcd.setCursor(0, 0);
    lcd.print("SWST "FIRMWARE_VERSION);
  }
}

void isr() {
  rx.tickISR();
}

void loop() {
  if(rxStarted && rx.gotData()) {
    if(rx.buffer[0] == 'S' && rx.buffer[1] == 'w' && rx.buffer[2] == 'S' && rx.buffer[3] == 't') {
      uint8_t txid = rx.buffer[12];
      if(usingTXID == 0) usingTXID = txid;
      else {
        if(usingTXID != txid) return;
      }
      if(usbState == 1) {
        Serial.write(rx.buffer, rx.size);
      }
      uint8_t sec = rx.buffer[4];
      uint8_t min = rx.buffer[5];
      int8_t hour = static_cast<int8_t>(rx.buffer[6]);
      uint8_t day = rx.buffer[7];
      uint8_t month = rx.buffer[8];
      uint16_t year = (rx.buffer[10] << 8) | rx.buffer[9];
      uint8_t dayWeek = rx.buffer[11];
      int16_t totalMinutes = hour * 60 + min + totalOffsetMinutes;
      if (totalMinutes < 0) {
        totalMinutes += 24 * 60;
        if (day > 1) {
            day--;
        } else {
            if (month > 1) {
                month--;
                day = daysInMonth(month, year);
            } else {
                month = 12;
                day = 31;
                year--;
            }
        }
    } else if (totalMinutes >= 24 * 60) {
        totalMinutes -= 24 * 60;
        if (day < daysInMonth(month, year)) {
            day++;
        } else {
            if (month < 12) {
                month++;
                day = 1;
            } else {
                month = 1;
                day = 1;
                year++;
            }
        }
      }
      hour = (totalMinutes / 60) % 24;
      min = totalMinutes % 60;
      char formatted[8] = {};
      sprintf(formatted, "%02d:%02u:%02u", hour, min, sec);
      lcd.setCursor(0, 0);
      lcd.print(formatted);
      sprintf(formatted, " %02u %s ", day, months[month-1]);
      lcd.setCursor(0, 1);
      lcd.print(formatted);
      timeoutTimer = millis();
      justPrintedSignalLost = false;
      if(min == 59) {
        if(sec == 55 || sec == 56 || sec == 57 || sec == 58 || sec == 59) {
          if(speakerState > 0 && toneFreq != 65535) tone(TONE_PIN, toneFreq, 100);
        }
      } else if(min == 30 && sec == 0) {
        if(speakerState > 0 && toneFreq != 65535) tone(TONE_PIN, toneFreq, 100);
        delay(200);
        if(speakerState > 0 && toneFreq != 65535) tone(TONE_PIN, toneFreq, 100);
      } else if(min == 0 && sec == 0) {
        if(speakerState > 0 && toneFreq != 65535) tone(TONE_PIN, toneFreq, 1000);
      }
    }
    if(speakerState > 0) digitalWrite(LED_BUILTIN, HIGH);
    ledTimer = millis();
  }

  if(rxStarted && timeoutTimer != 0 && millis()-timeoutTimer > SIGNAL_TIMEOUT) {
    if(!justPrintedSignalLost) {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print(" SIGNAL");
      lcd.setCursor(0, 1);
      lcd.print("  LOST");
      usingTXID = 0;
      if(speakerState > 0) tone(TONE_PIN, 50, 100);
      justPrintedSignalLost = true;
    }
  }

  if(ledTimer != 0 && millis()-ledTimer > 100) {
    if(speakerState > 0) digitalWrite(LED_BUILTIN, LOW);
    ledTimer = 0;
  }

  if(!rxStarted && menu != 0) {
    if(menuTimer > 0) {
      if(millis() - menuTimer > 5000) {
        menuTimer = 0;
        if(menuSomethingChanged) { // We have a limited resource of entries in the EEPROM, so we use this small optimization
          EEPROM.put(SAVED_SPEAKER_STATE_OFFSET, speakerState);
          EEPROM.put(SAVED_TONE_FREQ_OFFSET, toneFreq);
          EEPROM.put(SAVED_USB_STATE_OFFSET, usbState);
        }
        renderMenu(0);
        lcd.clear();
        lcd.setCursor(0, 1);
        lcd.print("AWAITING");
        lcd.setCursor(0, 0);
        lcd.print("SWST "FIRMWARE_VERSION);
        enc.resetState();
        return;
      }
    }
  }


  if(enc.tick()) {
    if(enc.held(1)) {
      detachInterrupt(0);
      rxStarted = false;
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("TXID:");
      lcd.setCursor(0, 1);
      lcd.print(String(usingTXID));
      delay(2000);
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("MUTED?");
      lcd.setCursor(0, 1);
      lcd.print(speakerState ? "No" : "Yes");
      delay(2000);
      rxStarted = true;
      attachInterrupt(0, isr, CHANGE);
    }
    if(enc.held(0)) {
      renderMenu(1);
    }

    if(!rxStarted && menu != 0) {
      if(!isMenuChange) {
        if(enc.click()) {
          if(menu == 4) {
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("TXID:");
            lcd.setCursor(0, 1);
            lcd.print(String(usingTXID));
            delay(2000);
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("MUTED?");
            lcd.setCursor(0, 1);
            lcd.print(speakerState ? "No" : "Yes");
            delay(2000);
            menuTimer = 0;
            renderMenu(menu, false);
          } else if(menu == 5) {
            menu = 0;
            enterTZHour();
          } else if(menu == 6) {
            menu = 0;
            usingTXID = 0;
            uint8_t resetState = 255;
            toneFreq = 65535;
            EEPROM.put(SAVED_STATE_OFFSET, resetState);
            EEPROM.put(SAVED_SPEAKER_STATE_OFFSET, resetState);
            EEPROM.put(SAVED_TONE_FREQ_OFFSET, toneFreq);
            EEPROM.put(SAVED_USB_STATE_OFFSET, resetState);
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print(" RESET");
            lcd.setCursor(0, 1);
            lcd.print(" DEVICE");
            delay(100);
            while(true) delay(100);
          } else {
            renderMenu(menu, true);
            isMenuChange = true;
          }
        }
        if(enc.left()) {
          if(menu+1 > MENUS) menu = 1;
          else menu++;
          renderMenu(menu);
        } else if(enc.right()) {
          if(menu-1 < 1) menu = MENUS;
          else menu--;
          renderMenu(menu);
        }
      } else {
        if(enc.click()) {
          isMenuChange = false;
          renderMenu(menu, false);
        }
        if(menu == 1) {
          if(enc.right() || enc.left()) {
            if(speakerState == 0) speakerState = 1;
            else if(speakerState == 1) speakerState = 0;
            menuSomethingChanged = true;
            renderMenu(menu, true);
          }
        } else if(menu == 2) {
          if(enc.left()) {
            if(toneFreq+25 > 8000) toneFreq = 50;
            else toneFreq += 25;
            menuSomethingChanged = true;
            renderMenu(menu, true);
          } else if(enc.right()) {
            if(toneFreq-25 < 50) toneFreq = 8000;
            else toneFreq -= 25;
            menuSomethingChanged = true;
            renderMenu(menu, true);
          }
        } else if(menu == 3) {
          if(enc.right() || enc.left()) {
            if(usbState == 0) usbState = 1;
            else if(usbState == 1) usbState = 0;
            menuSomethingChanged = true;
            renderMenu(menu, true);
          }
        }
      }
    }

    if(!rxStarted && changeTZState == 1 || !rxStarted && changeTZState == 2) {
      bool success = false;
      if(enc.right()) {
        if(changeTZState == 1 && selectedInt-1 < -12) selectedInt = 14;
        else if(changeTZState == 2 && selectedInt-15 < 0) selectedInt = 45;
        else {
          if(changeTZState == 1) selectedInt -= 1;
          else if(changeTZState == 2) {
            if(selectedInt-15 < 0) selectedInt = 45;
            else selectedInt -= 15;
          }
        }
        success = true;
      }
      else if(enc.left()) {
        if(changeTZState == 1 && selectedInt+1 > 14) selectedInt = -12;
        else if(changeTZState == 2 && selectedInt+15 > 45) selectedInt = 0;
        else {
          if(changeTZState == 1) selectedInt += 1;
          else if(changeTZState == 2) {
            if(selectedInt+15 >= 60) selectedInt = 0;
            else selectedInt += 15;
          }
        }
        success = true;
      }
      else if(enc.click()) {
        if(changeTZState == 1) {
          timezoneHours = selectedInt;
          enterTZMin();
        } else if(changeTZState == 2) {
          timezoneMinutes = selectedInt;
          EEPROM.put(SAVED_TZ_HOUR_OFFSET, timezoneHours);
          EEPROM.put(SAVED_TZ_MIN_OFFSET, timezoneMinutes);
          totalOffsetMinutes = timezoneHours * 60 + timezoneMinutes;
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("Timezone");
          lcd.setCursor(0, 1);
          lcd.print("changed.");
          delay(2000);
          EEPROM.put(SAVED_STATE_OFFSET, 1);
          EEPROM.put(SAVED_SPEAKER_STATE_OFFSET, speakerState); // attempt to fix
          rxStarted = true;
          attachInterrupt(0, isr, CHANGE);
          lcd.clear();
          lcd.setCursor(0, 1);
          lcd.print("AWAITING");
          lcd.setCursor(0, 0);
          lcd.print("SWST "FIRMWARE_VERSION);
        }
      }
      if(success) {
        lcd.clear();
        lcd.setCursor(0, 0);
        char formattedStr[8];
        int aHour = selectedInt;
        if(changeTZState == 1) {
          lcd.print("HOUR:");
          if(selectedInt < 0) aHour = -selectedInt;
          else aHour = selectedInt;
          sprintf(formattedStr, " %02d:%02d", aHour, 0);
          if(selectedInt < 0) formattedStr[0] = '-';
          else formattedStr[0] = '+';
        } else if(changeTZState == 2) {
          lcd.print("MINUTE:");
          if(timezoneHours < 0) aHour = -timezoneHours;
          else aHour = timezoneHours;
          sprintf(formattedStr, " %02d:%02d", aHour, selectedInt);
          if(timezoneHours < 0) formattedStr[0] = '-';
          else formattedStr[0] = '+';
        }
        lcd.setCursor(0, 1);
        lcd.print(formattedStr);
      }
    }
    enc.resetState();
  }
}

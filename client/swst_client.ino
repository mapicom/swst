#include <Gyver433.h>
#include <LiquidCrystal.h>

#define G433_FAST
#define G433_SPEED 2000

#define FIRMWARE_VERSION "0.1"
#define SIGNAL_TIMEOUT 10000
#define TIME_ZONE 7

Gyver433_RX<2, 12> rx;
LiquidCrystal lcd(A4, A5, A7, A6, A1, A0);

const char *months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

unsigned long timeoutTimer = 0;
unsigned long ledTimer = 0;
bool justPrintedSignalLost = false;

void setup() {
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);
  lcd.begin(8, 2);
  attachInterrupt(0, isr, CHANGE);
  lcd.clear();
  lcd.setCursor(0, 1);
  lcd.print("AWAITING");
  lcd.setCursor(0, 0);
  lcd.print("SWST "FIRMWARE_VERSION);
}

void isr() {
  rx.tickISR();
}

void loop() {
  if(rx.gotData()) {
    if(rx.buffer[0] == 'S' && rx.buffer[1] == 'w' && rx.buffer[2] == 'S' && rx.buffer[3] == 't') {
      uint8_t sec = rx.buffer[4];
      uint8_t min = rx.buffer[5];
      int8_t hour = static_cast<int8_t>(rx.buffer[6]);
      uint8_t day = rx.buffer[7];
      uint8_t month = rx.buffer[8];
      uint16_t year = (rx.buffer[10] << 8) | rx.buffer[9];
      uint8_t dayWeek = rx.buffer[11];
      char formatted[8] = {};
      sprintf(formatted, "%02d:%02u:%02u", (hour+TIME_ZONE) % 24, min, sec);
      lcd.setCursor(0, 0);
      lcd.print(formatted);
      sprintf(formatted, "%u %s %u", day, months[month-1], dayWeek);
      lcd.setCursor(0, 1);
      lcd.print(formatted);
      timeoutTimer = millis();
      justPrintedSignalLost = false;
      if(min == 59) {
        if(sec == 55 || sec == 56 || sec == 57 || sec == 58 || sec == 59) {
          tone(9, 800, 100);
        }
      } else if(min == 30 && sec == 0) {
        tone(9, 800, 100);
        delay(200);
        tone(9, 800, 100);
      } else if(min == 0 && sec == 0) {
        tone(9, 800, 1000);
      }
    }
    digitalWrite(LED_BUILTIN, HIGH);
    ledTimer = millis();
  }

  if(timeoutTimer != 0 && millis()-timeoutTimer > SIGNAL_TIMEOUT) {
    if(!justPrintedSignalLost) {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print(" SIGNAL");
      lcd.setCursor(0, 1);
      lcd.print("  LOST");
      tone(9, 50, 2000);
      justPrintedSignalLost = true;
    }
  }

  if(ledTimer != 0 && millis()-ledTimer > 100) {
    digitalWrite(LED_BUILTIN, LOW);
    ledTimer = 0;
  }
  delay(50);
}

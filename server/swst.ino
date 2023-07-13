//#include <ESP8266WiFi.h>
#include <Gyver433.h>
#include <GyverNTP.h>

#define G433_FAST
#define G433_SPEED 8000

#define WIFI_TIMEOUT 8000
#define BLINK_INTERVAL 500
#define DEFAULT_NTP "pool.ntp.org"
#define FIRMWARE_VERSION "0.1"

String userSSID = "";
String userPASS = "";
String userNTP = "";

unsigned long timeoutTimer = millis();
int prevState = LOW;
uint8_t ntpState = 0;

GyverNTP ntp(0, 600);
Gyver433_TX<D5, G433_XOR> tx;

bool enterSSID() {
  while(!Serial.available()) {
    digitalWrite(LED_BUILTIN, HIGH);
    delay(BLINK_INTERVAL);
    digitalWrite(LED_BUILTIN, LOW);
    delay(BLINK_INTERVAL);
  }
  String enteredSSID = Serial.readString();
  if(enteredSSID == "") return false;
  enteredSSID.trim();
  userSSID = enteredSSID;
  char buf[userSSID.length()+1];
  userSSID.toCharArray(buf, sizeof(buf));
  File file = SPIFFS.open("ssid.txt", "w+");
  file.write(buf);
  file.close();
  Serial.println("\r\nYou entered " + userSSID + ". Okay.");
  return true;
}

bool enterPass() {
  while(!Serial.available()) {
    digitalWrite(LED_BUILTIN, HIGH);
    delay(BLINK_INTERVAL);
    digitalWrite(LED_BUILTIN, LOW);
    delay(BLINK_INTERVAL);
  }
  String enteredPASS = Serial.readString();
  enteredPASS.trim();
  if(enteredPASS == "") return false;
  userPASS = enteredPASS;
  char buf[userPASS.length()+1];
  userPASS.toCharArray(buf, sizeof(buf));
  File file = SPIFFS.open("pass.txt", "w+");
  file.write(buf);
  file.close();
  Serial.println("\nYou entered " + userPASS + ". Okay.");
  return true;
}

bool enterNTP() {
  while(!Serial.available()) {
    digitalWrite(LED_BUILTIN, HIGH);
    delay(BLINK_INTERVAL);
    digitalWrite(LED_BUILTIN, LOW);
    delay(BLINK_INTERVAL);
  }
  String enteredNTP = Serial.readString();
  enteredNTP.trim();
  if(enteredNTP == "") {
    enteredNTP = DEFAULT_NTP;
  }
  userNTP = enteredNTP;
  char buf[userNTP.length()+1];
  userNTP.toCharArray(buf, sizeof(buf));
  File file = SPIFFS.open("ntp.txt", "w+");
  file.write(buf);
  file.close();
  Serial.println("\nYou entered " + userNTP + ". Okay.");
  return true;
}

void setup() {
  // put your setup code here, to run once:
  pinMode(LED_BUILTIN, OUTPUT);
  Serial.begin(115200);
  while(!Serial) {
    digitalWrite(LED_BUILTIN, HIGH);
    delay(BLINK_INTERVAL);
    digitalWrite(LED_BUILTIN, LOW);
    delay(BLINK_INTERVAL);
  }
  delay(100);
  Serial.printf("\nSWST Server %s (C) 2023 Mapicom. All rights reserved.\n", FIRMWARE_VERSION);
  Serial.print("Enabling file system... ");
  SPIFFS.begin();
  FSInfo fs_info;
  if(SPIFFS.info(fs_info)) {
    Serial.println("ok");
  } else {
    Serial.println("Error!\nFile system is not available. Try to buy new microcontroller.");
    while(true) {
      digitalWrite(LED_BUILTIN, HIGH);
      delay(BLINK_INTERVAL);
      digitalWrite(LED_BUILTIN, LOW);
      delay(BLINK_INTERVAL);
    }
  }
  if(!SPIFFS.exists("ssid.txt")) {
   Serial.println("Configuration file is not exists. Let's create it!");
   Serial.print("Please, enter SSID: ");
   while(!enterSSID()) {
    Serial.println("Please, enter a valid SSID.");
   }
  } else {
    File ssidFile = SPIFFS.open("ssid.txt", "r");
    userSSID = ssidFile.readString();
    ssidFile.close();
    Serial.println("SSID file loaded.");
  }
  if(!SPIFFS.exists("pass.txt")) {
   Serial.print("Please, enter password from Wi-Fi network: ");
   while(!enterPass()) {
    Serial.println("Please, enter a valid password.");
   }
  } else {
    File passFile = SPIFFS.open("pass.txt", "r");
    userPASS = passFile.readString();
    passFile.close();
    Serial.println("Password file loaded.");
  }
  if(!SPIFFS.exists("ntp.txt")) {
   Serial.print(String("Please, enter NTP server or just press Enter for default [" + String(DEFAULT_NTP) + "]: "));
   while(!enterNTP()) {
    Serial.println("Please, enter a valid NTP server.");
   }
  } else {
    File ntpFile = SPIFFS.open("ntp.txt", "r");
    userNTP = ntpFile.readString();
    ntpFile.close();
    Serial.println("NTP file loaded.");
  }
  Serial.print("Checking Wi-Fi status... ");
  Serial.println(WiFi.status());
  Serial.print("Connecting to " + userSSID + "... ");
  bool isSuccess = true;
  if(WiFi.status() == WL_CONNECTED) {
    Serial.println("already connected");
  } else {
    timeoutTimer = millis();
    WiFi.begin(userSSID, userPASS);
    while(WiFi.status() != WL_CONNECTED) {
      delay(100);
      if(millis()-timeoutTimer > WIFI_TIMEOUT) {
        isSuccess = false;
        SPIFFS.remove("ssid.txt");
        SPIFFS.remove("pass.txt");
        Serial.println("timeout.\nConnection timeouted, please reboot your device and enter new credentials.");
        break;
      }
    }
  }
  if(!isSuccess) {
    while(true) {
        digitalWrite(LED_BUILTIN, HIGH);
        delay(BLINK_INTERVAL);
        digitalWrite(LED_BUILTIN, LOW);
        delay(BLINK_INTERVAL);
     }
  }
  Serial.println("ok");
  timeoutTimer = millis();
  Serial.printf("Using NTP server: %s\n", userNTP.c_str());
  ntp.begin();
  ntp.setHost(userNTP.c_str());
  ntp.asyncMode(false);
  ntp.updateNow();
  Serial.println("All done. Running radio.");
  digitalWrite(LED_BUILTIN, HIGH);
  Serial.print("Entering shell-mode.\nWARNING: Entering command to shell makes long system's freeze.\n# ");
}

void loop() {
  // put your main code here, to run repeatedly:
  ntp.tick();
  if(ntp.synced()) {
    uint16_t curMS = ntp.ms();
    if(curMS >= 0 && curMS <= 5) {
      char str[25];
      sprintf(str, "SwSt;%02d;%02d;%02d;%02d;%02d;%02d;%d;", ntp.second(), ntp.minute(), ntp.hour(), ntp.day(), ntp.month(), ntp.year(), ntp.dayWeek());
      tx.sendData(str);
      digitalWrite(LED_BUILTIN, prevState);
      if(prevState == LOW) prevState = HIGH;
      else if(prevState == HIGH) prevState = LOW;
    }
  }

  uint8_t stat = ntp.status();
  if(stat != 0 && stat != ntpState) {
    Serial.printf("\nWARNING: NTP status is not 0, it's %d\n# ", stat);
    ntpState = stat;
  } else if(stat == 0 && ntpState != 0) {
    Serial.print("\nNTP status returned to 0\n# ");
  }

  if(Serial.available() > 0) {
    String cmd = Serial.readString();
    cmd.trim();
    if(cmd == "reset_ntp") {
      SPIFFS.remove("ntp.txt");
      Serial.println("\nNTP configuration removed. Rebooting...");
      ntp.end();
      WiFi.disconnect(false);
      ESP.restart();
    } else if(cmd == "reset_wifi") {
      SPIFFS.remove("ssid.txt");
      SPIFFS.remove("pass.txt");
      Serial.println("\nWi-Fi configuration removed. Rebooting...");
      ntp.end();
      WiFi.disconnect(false);
      ESP.restart();
    } else if(cmd == "date") {
      Serial.printf("\n%s %s\n", ntp.dateString(), ntp.timeString());
    } else if(cmd == "reboot") {
      ntp.end();
      WiFi.disconnect(false);
      Serial.println("Rebooting...");
      ESP.restart();
    } else if(cmd == "mem") {
      Serial.printf("\nFree heap size: %d\n", ESP.getFreeHeap());
    } else if(cmd == "stop_ntp") {
      Serial.print("\nStopping NTP daemon... ");
      ntp.end();
      Serial.println("ok");
    } else if(cmd == "start_ntp") {
      Serial.print("\nStarting NTP daemon... ");
      ntp.begin();
      ntp.updateNow();
      Serial.println("ok");
    } else if(cmd == "update_ntp") {
      Serial.printf("Update status: %d\n", ntp.updateNow());
    } else {
      Serial.println("\nUnknown command.");
    }
    Serial.print("# ");
  }
}

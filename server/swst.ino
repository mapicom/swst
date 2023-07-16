//#include <ESP8266WiFi.h>
#include <Gyver433.h>
#include <GyverNTP.h>

#define G433_FAST
#define G433_SPEED 2000

#define WIFI_TIMEOUT 8000
#define BLINK_INTERVAL 250
#define DEFAULT_NTP "pool.ntp.org"
#define FIRMWARE_VERSION "0.1"

String userSSID = "";
String userPASS = "";
String userNTP = "";

unsigned long timeoutTimer = millis();
int prevState = LOW;
uint8_t ntpState = 0;

GyverNTP ntp(0, 600);
Gyver433_TX<D1> tx;

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
  Serial.printf("ESP8266 SDK Version: %s\n", ESP.getSdkVersion());
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
        Serial.println("timeout.\nConnection timeouted! Please reboot for try again or press Enter for reset credentials and reboot.");
        WiFi.disconnect(false);
        break;
      }
    }
  }
  if(!isSuccess) {
    while(!Serial.available()) {
      digitalWrite(LED_BUILTIN, HIGH);
      delay(100);
      digitalWrite(LED_BUILTIN, LOW);
      delay(100);
    }
    while(true) {
      String answer = Serial.readString();
      answer.trim();
      if(answer == "") {
        SPIFFS.remove("ssid.txt");
        SPIFFS.remove("pass.txt");
        Serial.println("Wi-Fi credentials have been reset. Rebooting...");
        WiFi.disconnect(false);
        ESP.restart();
        return;
      }
    }
    
  }
  Serial.println("ok");
  timeoutTimer = millis();
  Serial.printf("Using NTP server: %s\n", userNTP.c_str());
  ntp.begin();
  ntp.setHost(userNTP.c_str());
  ntp.asyncMode(false);
  Serial.println("All done. Running radio.");
  digitalWrite(LED_BUILTIN, HIGH);
  Serial.println("Entering shell-mode.\nWARNING: Entering command to shell makes long system's freeze.");
}

void loop() {
  // put your main code here, to run repeatedly:
  ntp.tick();
  if(ntp.synced()) {
    uint16_t curMS = ntp.ms();
    if(curMS >= 0 && curMS <= 5) {
      uint8_t signalData[12] = {};
      // ======= PROTOCOL FORMAT =======
      // Packet ID
      signalData[0] = 'S';
      signalData[1] = 'w';
      signalData[2] = 'S';
      signalData[3] = 't';
      // Time
      signalData[4] = ntp.second(); // Second
      signalData[5] = ntp.minute(); // Minute
      signalData[6] = ntp.hour(); // Hour
      // Date
      signalData[7] = ntp.day(); // Day
      signalData[8] = ntp.month(); // Month
      signalData[9] = ntp.year() & 0xFF; // Year 1
      signalData[10] = (ntp.year() >> 8) & 0xFF; // Year 2
      signalData[11] = ntp.dayWeek(); // Day of week
      // ===============================
      tx.sendData(signalData);
      digitalWrite(LED_BUILTIN, prevState);
      if(prevState == LOW) prevState = HIGH;
      else if(prevState == HIGH) prevState = LOW;
    }
  }

  uint8_t stat = ntp.status();
  if(stat != 0 && stat != ntpState) {
    Serial.printf("WARNING: NTP status is not 0, it's %d\n# ", stat);
    ntpState = stat;
  } else if(stat == 0 && ntpState != 0) {
    Serial.print("NTP status returned to 0\n# ");
  }

  if(Serial.available() > 0) {
    String cmd = Serial.readString();
    cmd.trim();
    if(cmd == "reset_ntp") {
      SPIFFS.remove("ntp.txt");
      Serial.println("NTP configuration removed. Rebooting...");
      ntp.end();
      WiFi.disconnect(false);
      ESP.restart();
    } else if(cmd == "reset_wifi") {
      SPIFFS.remove("ssid.txt");
      SPIFFS.remove("pass.txt");
      Serial.println("Wi-Fi configuration removed. Rebooting...");
      ntp.end();
      WiFi.disconnect(false);
      ESP.restart();
    } else if(cmd == "date") {
      Serial.printf("%s %s\n", ntp.dateString(), ntp.timeString());
    } else if(cmd == "reboot") {
      ntp.end();
      WiFi.disconnect(false);
      Serial.println("Rebooting...");
      ESP.restart();
    } else if(cmd == "mem") {
      Serial.printf("Free heap size: %d\n", ESP.getFreeHeap());
    } else if(cmd == "stop_ntp") {
      Serial.print("Stopping NTP daemon... ");
      ntp.end();
      Serial.println("ok");
    } else if(cmd == "start_ntp") {
      Serial.print("Starting NTP daemon... ");
      ntp.begin();
      ntp.updateNow();
      Serial.println("ok");
    } else if(cmd == "update_ntp") {
      Serial.printf("Update status: %d\n", ntp.updateNow());
    } else {
      Serial.println("Unknown command.");
    }
  }
}

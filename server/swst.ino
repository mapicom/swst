#include <ESP8266WiFi.h>
#include <Gyver433.h>
#include <GyverNTP.h>

#define WIFI_TIMEOUT 8000

String userSSID = "";
String userPASS = "";

unsigned long timeoutTimer = millis();

GyverNTP ntp(0, 600);
Gyver433_TX<D5, G433_XOR> tx;

bool enterSSID() {
  while(!Serial.available()) {
    digitalWrite(LED_BUILTIN, HIGH);
    delay(50);
    digitalWrite(LED_BUILTIN, LOW);
    delay(50);
  }
  String enteredSSID = Serial.readString();
  if(enteredSSID == "") return false;
  enteredSSID.trim();
  Serial.println("\r\nYou entered " + enteredSSID + ". Okay.");
  userSSID = enteredSSID;
  char buf[userSSID.length()+1];
  userSSID.toCharArray(buf, sizeof(buf));
  File file = SPIFFS.open("ssid.txt", "w+");
  file.write(buf);
  file.close();
  return true;
}

bool enterPass() {
  while(!Serial.available()) {
    digitalWrite(LED_BUILTIN, HIGH);
    delay(50);
    digitalWrite(LED_BUILTIN, LOW);
    delay(50);
  }
  String enteredPASS = Serial.readString();
  enteredPASS.trim();
  if(enteredPASS == "") return false;
  Serial.println("\nYou entered " + enteredPASS + ". Okay.");
  userPASS = enteredPASS;
  char buf[userPASS.length()+1];
  userPASS.toCharArray(buf, sizeof(buf));
  File file = SPIFFS.open("pass.txt", "w+");
  file.write(buf);
  file.close();
  return true;
}

void setup() {
  // put your setup code here, to run once:
  pinMode(LED_BUILTIN, OUTPUT);
  Serial.begin(115200);
  while(!Serial) {
    digitalWrite(LED_BUILTIN, HIGH);
    delay(50);
    digitalWrite(LED_BUILTIN, LOW);
    delay(50);
  }
  delay(100);
  Serial.println("\nSWST Server (C) 2023 Mapicom. All rights reserved.");
  Serial.print("Enabling file system... ");
  SPIFFS.begin();
  FSInfo fs_info;
  if(SPIFFS.info(fs_info)) {
    Serial.println("ok");
  } else {
    Serial.println("Error!\nFile system is not available. Try to buy new microcontroller.");
    while(true) {
      digitalWrite(LED_BUILTIN, HIGH);
      delay(50);
      digitalWrite(LED_BUILTIN, LOW);
      delay(50);
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
  Serial.print("Checking Wi-Fi status... ");
  Serial.println(WiFi.status());
  Serial.print("Connecting to " + userSSID + "... ");
  timeoutTimer = millis();
  bool isSuccess = true;
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
  if(!isSuccess) {
    while(true) {
        digitalWrite(LED_BUILTIN, HIGH);
        delay(50);
        digitalWrite(LED_BUILTIN, LOW);
        delay(50);
     }
  }
  Serial.println("ok");
  Serial.print("Starting NTP daemon... ");
  ntp.begin();
  ntp.updateNow();
  Serial.println("ok");
  Serial.println("All done. Running radio.");
  digitalWrite(LED_BUILTIN, HIGH);
}

void loop() {
  // put your main code here, to run repeatedly:
  uint16_t curMS = ntp.ms();
  if(curMS >= 0 && curMS <= 5) {
    char str[25];
    sprintf(str, "SwSt;%02d;%02d;%02d;%02d;%02d;%02d;", ntp.second(), ntp.minute(), ntp.hour(), ntp.day(), ntp.month(), ntp.year());
    tx.sendData(str);
  }
}

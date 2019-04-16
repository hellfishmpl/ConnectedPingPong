#include <PubSubClient.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>
#include "SSD1306Brzo.h"
#include "images.h"

const char* ssid = "a wifi";
const char* password = "a password";
const bool wifi = true; // set to FALSE to diseable all network ( for localtesting ).
const bool debug = true; // set to true to serial output debug info.
const bool screenIsPresent = false;  // set to true if you will use a screen ( SSD1306 )
//
//TODO : make some magic about WIFI connetion.
//TODO : make authentication within MQTT

const char* mqtt_server = "mqtt broker";
WiFiClient espClient;
PubSubClient client(espClient);
long lastMsg = 0;
char msg[50];
char p [16];   //OMG what is this!
char aa [50];  //Not better
int score [4];
int value = 0;


// Const for Button state
// TODO: Prototype use only 2 button Plus, So need to Add Minus button in future.
const int OnePlusButton = 0;  //0 this is D3
const int OneMinusButton = 1; // 14; this is D5
const int TwoPlusButton = 2;  // 2;  this is D4
const int TwoMinusButton = 3; // 13; this is D7
const int ButtonPin[4] = {0, 14, 2, 13};  // Set your pinout here for the button.

//variable button state
int ButtonPushCnt[4] ;
int ButtonState[4];
int LastState[4] ;
long TimePressed[4];  // how long the button are pressed
long DebounceTime[4]; //, time allows for Debounce

// MQTT setting
String myName = String(ESP.getChipId());
String clientId = "i77PPSB" + myName;
String P1Queue = "/PP/games/" + clientId + "/current/p1/currentscore";
String P2Queue = "/PP/games/" + clientId + "/current/p2/currentscore";

// Screen setting
#define LED D0;
#define ONE_WIRE_BUS 2;
typedef  void (*fDisplayTemp)(void);
SSD1306Brzo display(0x3c, D1, D2);


// MQTT callback.  This is where you
void mqttCallback(char* topic, byte* payload, unsigned int length) {

  String payloadSTR;
  for (int i = 0; i < length; i++) {
    payloadSTR += (char)payload[i];
  }
  // Here to put receive score from subscribe queue and put
  // it on a global CurrentScoreP1 and CurrentScoreP2 variables.

  if (String(topic).indexOf("/p1/currentscore") > 1) {
    score[OnePlusButton] = payloadSTR.toInt();
    if (debug) Serial.print("Score P1:");
    if (debug) Serial.println(score[OnePlusButton]);
    if (screenIsPresent) drawTxt(70, 14, "  ");
    if (screenIsPresent) drawTxt(70, 14, String(score[OnePlusButton]));
  }

  if (String(topic).indexOf("/p2/currentscore") > 1) {
    score[TwoPlusButton] = payloadSTR.toInt();
    if (screenIsPresent) drawTxt(70, 26, "   ");
    if (screenIsPresent) drawTxt(70, 26, String(score[TwoPlusButton]));
    if (debug) Serial.print("Score P2:");
    if (debug) Serial.println(score[TwoPlusButton]);
  }
};

void reconnect() {
  if (wifi) {
    while (!client.connected()) {
      Serial.print(ESP.getChipId());
      if (client.connect(clientId.c_str())) {
        if (debug) Serial.println("connected");
        String subQueue = "/PP/SB/client/" + clientId + "/status";
        if (debug) Serial.print(subQueue);
        client.publish(subQueue.c_str() , "Connected" );
        // ... and resubscribe
        client.subscribe(P1Queue.c_str());
        client.subscribe(P2Queue.c_str());
      } else {
        if (debug) Serial.print("failed, rc=");
        if (debug) Serial.print(client.state());
        if (debug) Serial.println(" try again in 5 seconds");
        // Wait 5 seconds before retrying
        delay(5000);
      }
    }
  }
}

void handleButtons(int PressedButton) {

  int score2pub = 0;
  long PressedTime = 0;
  // First we read the button state.
  ButtonState[PressedButton] = digitalRead(ButtonPin[PressedButton]);
  // if button not same state , then something append.
  if ( ButtonState[PressedButton] != LastState[PressedButton]) {
    LastState[PressedButton] = ButtonState[PressedButton];
    if (debug) Serial.println ( "Bouton touch");
    if (debug) Serial.println (ButtonState[PressedButton]);
    // Last state was LOW, so someOne push the button
    if ( ButtonState[PressedButton] == LOW ) {
      DebounceTime[PressedButton] = millis();
    } else {
      // Last state was LOW, so someone release it!
      // A press less than 2 seconds is a short press.
      ButtonPushCnt[PressedButton]++;
      PressedTime = millis() - DebounceTime[PressedButton];
      if (debug) Serial.println(PressedTime);
      if (debug) Serial.println(ButtonPushCnt[PressedButton]);
      // +1
      if (PressedTime > 150 && PressedTime < 1000) {
        score2pub = score[PressedButton] + 1;
        if (wifi && PressedButton == OnePlusButton ) client.publish( P1Queue.c_str(), String(score2pub).c_str() );
        if (wifi && PressedButton == TwoPlusButton ) client.publish( P2Queue.c_str(), String(score2pub).c_str() );
      }
      // Reset the games if button is hold for 5 seconds
      if (PressedTime > 4000 ) {
        for (int i = 0; i < 4; i++) {
          score[i] = 0;
          ButtonPushCnt[i] = 0;
        }
        client.publish( P1Queue.c_str(), String(score2pub).c_str());
        client.publish( P2Queue.c_str(), String(score2pub).c_str());
      }
    }
  }
}



void drawImage() {

  display.clear();
  display.drawXbm(34, 16, WiFi_Logo_width, WiFi_Logo_height, WiFi_Logo_bits);
  display.display();

}

void drawProgressBar(int progress) {

  display.clear();
  display.drawProgressBar(0, 32, 120, 10, progress);
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.drawString(64, 15, String(progress) + "%");
  display.display();

}

void drawTxt(int x, int y, String text) {

  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setColor(BLACK);
  display.fillRect( x, y, text.length() * 6, 10);
  display.setColor(WHITE);
  display.drawString(x, y, text);
  display.display();

}

void setup(void) {
  // Buttons pins setup, as we use two wire arcade button, we have to use INPUT_PULLUP ,
  for (int i = 0; i < 4; i++) {
    pinMode(ButtonPin[i], INPUT_PULLUP);
  }

  Serial.begin(115200);
  // Display init
  if (screenIsPresent) {
    display.init();
    display.flipScreenVertically();
    display.setFont(ArialMT_Plain_10);
    drawImage();
    delay(2000);
    drawProgressBar(5);
  }
  // Wifi Init
  if (wifi) {
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    if (screenIsPresent) drawProgressBar(25);
    // Wait for connection
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      if (debug) Serial.print(".");
    }
    if (debug) {
      Serial.println("");
      Serial.print("Connected to ");
      Serial.println(ssid);
      Serial.print("IP address: ");
      Serial.println(WiFi.localIP());
    }
    if (screenIsPresent) drawProgressBar(55);
    if (MDNS.begin("test-device")) {
      Serial.println("MDNS responder started");
    }

    //OTA STUFF
    ArduinoOTA.setHostname(clientId.c_str());
    ArduinoOTA.setPassword(password);

    ArduinoOTA.onStart([]() {
      Serial.println("Start updating ");
    });
    ArduinoOTA.onEnd([]() {
      Serial.println("\nEnd");
    });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
      snprintf (p, 16, "Progress: %u%%\r", (progress / (total / 100)));
      Serial.printf(p);

    });
    ArduinoOTA.onError([](ota_error_t error) {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
      else if (error == OTA_END_ERROR) Serial.println("End Failed");
    });
    ArduinoOTA.begin();
    Serial.println("Ready to update...");
    Serial.print("on IP address: ");
    Serial.println(WiFi.localIP());

    //start Sensors reading
    //sensors.begin();
    if (screenIsPresent) drawProgressBar(100);
    if (screenIsPresent) display.clear();
    String IP = WiFi.localIP().toString();
    if (screenIsPresent) drawTxt(10, 2, "IP:");
    if (screenIsPresent) drawTxt(40, 2, IP);
    WiFi.printDiag(Serial);
    // initalise mqtt
    client.setServer(mqtt_server, 1883);
    client.setCallback(mqttCallback);
  }
  if (screenIsPresent) {
    if (!wifi) display.clear();
    drawTxt(10, 14, "Player 1:");
    drawTxt(10, 26, "Player 2:");
  }
}

void loop(void) {
  if (wifi) {
    ArduinoOTA.handle();
    if (!client.connected()) {
      reconnect();
    }
    client.loop();
  }
  for (int i = 0; i < 4; i++) {
    handleButtons(i);
  }
}

#include <Wire.h>
#include "MAX30100_PulseOximeter.h"
#include <ESP32Servo.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <FS.h>
#include <SPIFFS.h>

#define REPORTING_PERIOD_MS 1000

#define LED_PIN 19
#define SV1PIN 5
#define SV2PIN 14

WiFiMulti wifiMulti;
PulseOximeter pox;
Servo sv1, sv2;
WebServer server(80);

bool p;
float hr;
int spo2;

uint32_t tsLastReport = 0;

void onBeatDetected()
{
    Serial.println("Beat!");
    Serial.print("p = ");
    Serial.println(p);
    sv1.write(p ? 180 : 0);
    sv2.write(p ? 0 : 180);
    p = !p;
    digitalWrite(LED_PIN, HIGH);
    delay(100);
    digitalWrite(LED_PIN, LOW);
}

void handleRoot() {
    String html = "";
    File f = SPIFFS.open("/index.html", FILE_READ);
    if (f) {
        while(f.available()) {
            char ch = f.read();
            html.concat(ch);
        }
        f.close();
    }
    Serial.println("GET /");
    server.send(200, "text/html", html);
}

void handleJson() {
    char buf[10], buf2[10];
    dtostrf(hr, 5, 1, buf);
    sprintf(buf2, "%d", spo2);
    Serial.println("GET /json");
    String json = "{\"hr\":";
    json.concat(buf);
    json.concat(",\"spo2\":");
    json.concat(spo2);
    json.concat("}");
    server.send(200, "text/json", json);
}

void setup()
{
    Serial.begin(115200);
    pinMode(LED_PIN, OUTPUT);

    sv1.setPeriodHertz(50);
    sv1.attach(SV1PIN, 1000, 2000);
    sv2.setPeriodHertz(50);
    sv2.attach(SV2PIN, 1000, 2000);
    p = true;

    wifiMulti.addAP("ssid1", "pass1");
    wifiMulti.addAP("ssid2", "pass2");
    wifiMulti.addAP("ssid3", "pass3");

    Serial.println("Connecting Wifi...");
    if(wifiMulti.run() == WL_CONNECTED) {
        Serial.println("");
        Serial.println("WiFi connected");
        Serial.println("IP address: ");
        Serial.println(WiFi.localIP());
    }
    if (MDNS.begin("esp32hr")) {
        Serial.println("MDNS responder started");
    }
    SPIFFS.begin();
    
    Serial.print("Initializing pulse oximeter..");

    if (!pox.begin()) {
        Serial.println("FAILED");
        for(;;);
    } else {
        Serial.println("SUCCESS");
    }
    pox.setOnBeatDetectedCallback(onBeatDetected);

    server.on("/", handleRoot);
    server.on("/json", handleJson);
    server.begin();
}

void loop()
{
    server.handleClient();

    pox.update();

    if (millis() - tsLastReport > REPORTING_PERIOD_MS) {
        hr = pox.getHeartRate();
        spo2 = pox.getSpO2();
        Serial.print("Heart rate:");
        Serial.print(hr);
        Serial.print("bpm / SpO2:");
        Serial.print(spo2);
        Serial.println("%");

        tsLastReport = millis();
    }
}

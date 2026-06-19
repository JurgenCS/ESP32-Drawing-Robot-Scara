#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncJson.h>
#include <ArduinoJson.h>
#include <ESPmDNS.h>

// Own libraries
#include <config.h>
#include <gcode.h>

// Website
#include <web/index_gz.h>

const char *ssid = "SSID";
const char *password = "PSWD";

// WiFi and server setup
AsyncWebServer server(80);

String get_wifi_status(int status){
    switch(status){
        case WL_IDLE_STATUS:
        return "WL_IDLE_STATUS";
        case WL_SCAN_COMPLETED:
        return "WL_SCAN_COMPLETED";
        case WL_NO_SSID_AVAIL:
        return "WL_NO_SSID_AVAIL";
        case WL_CONNECT_FAILED:
        return "WL_CONNECT_FAILED";
        case WL_CONNECTION_LOST:
        return "WL_CONNECTION_LOST";
        case WL_CONNECTED:
        return "WL_CONNECTED";
        case WL_DISCONNECTED:
        return "WL_DISCONNECTED";
    }
}

void setupWiFi()
{
  WiFi.mode(WIFI_STA);
  int status = WL_IDLE_STATUS;
  Serial.println("\nConnecting");
  Serial.println(get_wifi_status(status));
  WiFi.begin(ssid, password);
  
  while(status != WL_CONNECTED){
      delay(500);
      status = WiFi.status();
      Serial.println(get_wifi_status(status));
  }

  Serial.println("\nConnected to the WiFi network");
  Serial.print("Local ESP32 IP: ");
  Serial.println(WiFi.localIP());
}

void setup()
{
  Serial.begin(115200);
  delay(2000);
  servoLift.attach(liftServoPin);
  servoLeft.attach(servoLeftPin);
  servoRight.attach(servoRightPin);

  setupWiFi();

  Serial.println("Homing XY.");
  homeXY();

  // Define route handlers
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
            {
    // Check if website hashes still match
    if (request->hasHeader("If-None-Match")) {
      String etag = request->header("If-None-Match");
      if (etag.equals(index_gz_sha)) {
        request->send(304);
        return;
      }
    }

    // Send new version otherwise
    AsyncWebServerResponse *response = request->beginResponse_P(200, "text/html", index_gz, index_gz_len);
    response->addHeader("Content-Encoding", "gzip");
    response->addHeader("ETag", index_gz_sha);
    request->send(response); });

  server.on("/gcode", HTTP_POST, [](AsyncWebServerRequest *request)
            {
              if(request->hasParam("gcode", true))
              {
                String gcode = request->getParam("gcode", true)->value();
                setGCode(gcode);
                request->send(200, "text/plain", "OK");
              }

              request->send(400, "text/plain", "Missing gcode parameter"); });

  server.on("/status", HTTP_GET, [](AsyncWebServerRequest *request)
            {
      AsyncResponseStream *response = request->beginResponseStream("application/json");

      JsonDocument doc;

      Position pos = getCurrentPosition();
      doc["x"] = pos.x;
      doc["y"] = pos.y;
      doc["busy"] = isBusy();
      doc["raised"] = servoLift.read() == LIFT_UP_ANGLE;

      serializeJson(doc, *response);

      request->send(response); });

  server.on("/config", HTTP_GET, [](AsyncWebServerRequest *request) {
      AsyncResponseStream *response = request->beginResponseStream("application/json");

      JsonDocument doc;

      Position pos = getCurrentPosition();
      doc["minX"] = MIN_X;
      doc["maxX"] = MAX_X;
      doc["minY"] = MIN_Y;
      doc["maxY"] = MAX_Y;
      doc["homeX"] = HOMING_POSITION.x;
      doc["homeY"] = HOMING_POSITION.y;
      doc["speed"] = getSpeed();
      doc["minSpeed"] = MIN_SPEED;
      doc["maxSpeed"] = MAX_SPEED;

      serializeJson(doc, *response);

      request->send(response);
  });

  server.on("/assembly", HTTP_POST, [](AsyncWebServerRequest *request)
            {
              assemblyPosition();
              request->send(200, "text/plain", "OK"); });

  server.on("/restart", HTTP_POST, [](AsyncWebServerRequest *request)
            {
              ESP.restart(); });

  server.begin();
}

void loop()
{
  machineLoop();
  updateToolPosition();
}

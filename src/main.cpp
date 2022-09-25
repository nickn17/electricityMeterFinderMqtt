/*
Goodwe/Finder rs485, 9600 baud 8 data bits 1 stop bit parity None
id 51 - main electricity meter 7M.24.8.230.0010 | FINDER, 1phase 40A
id 52 - FVE/PV electricity meter 7M.24.8.230.0010 | FINDER, 1phase 40A
*/

#include <Arduino.h>
#include <M5Stack.h>
#include <time.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <HTTPClient.h>
#include <ArduinoOTA.h>
#include <PubSubClient.h>
#include <ESP32WebServer.h>
#include "Free_Fonts.h"
#include "modbus.h"

M5Display &tft = M5.Lcd;
TFT_eSprite spr = TFT_eSprite(&M5.Lcd);
ESP32WebServer server(80);

const char *ssid = "xxx"; // Wifi ssid
const char *password = "xxx"; // Wifi password
const char *mqtt_server = "10.20.30.196";
const char *mqtt_id ="entryway-modbus";
const char *mqtt_username ="sensors";
const char *mqtt_password = "xyz";

IPAddress local_IP(10, 20, 30, 239); // Device static IP address
IPAddress gateway(10, 20, 30, 1);    // Gateway
IPAddress subnet(255, 255, 255, 0);  // Network mask
IPAddress primaryDNS(10, 20, 30, 1); // Primary DNS (local)
IPAddress secondaryDNS(8, 8, 4, 4);  // Seconday DNS (google)

WiFiClient espClient;
PubSubClient client(espClient);

bool onAir = false;
bool nightMode = false;
bool alarmMode = false;
uint8_t readBytes = 0;
uint32_t tim;
int32_t tmpInt32;
uint8_t brightnessVal = 0;
uint8_t oldBrightnessVal = 0;

// Electricity meter (51 - main, 52 - pv) parameters
float main_voltage = 0;
float main_frequency = 0;
float main_temperature = 0;
float main_energy_power = 0;
float main_energy_total1 = 0;
uint32_t main_energy_total1mantissa = 0;
int16_t main_energy_total1exp = 0;
float main_energy_total2 = 0;
uint32_t main_energy_total2mantissa = 0;
int16_t main_energy_total2exp = 0;
float pv_voltage = 0;
float pv_frequency = 0;
float pv_temperature = 0;
float pv_energy_power = 0;
float pv_energy_total1 = 0;
uint32_t pv_energy_total1mantissa = 0;
int16_t pv_energy_total1exp = 0;
float pv_energy_total2 = 0;
float pv_energy_total2diff = 0;
uint32_t pv_energy_total2mantissa = 0;
int16_t pv_energy_total2exp = 0;

// Buffer
uint8_t data[10];

/**
   Handle webserver root
*/
void handleRoot()
{
  String text;

  // Render html5
  //<meta http-equiv='refresh' content='5'>
  text = "<html><head><link href='https://fonts.googleapis.com/css?family=Roboto+Condensed' rel='stylesheet'><style>body { margin:32px; background:black; color: white; font-family: 'Roboto Condensed', sans-serif; } td { padding: 0px 16px 0px 0px; text-align: center;} th { color: orange; text-align: center; padding: 0px 16px 0px 0px; } td:first-child { text-align: left; } .off { color: gray; } .on { color: lime; } .red { color: red; } .lime { color: lime; } .yellow { color: yellow; }</style></head>";
  text += "<body><h1>[ Entryway modbus ] </h1>";

  text += "<h2>MAIN</h2>";

  text += "<table style='min-width: 25%;'>";
  text += "<tr><td>main_energy_power</td><td class='right'>" + String(main_energy_power) + " kW</td></tr>";
  text += "<tr><td>main_voltage</td><td class='right'>" + String(main_voltage) + " V</td></tr>";
  text += "<tr><td>main_frequency</td><td class='right'>" + String(main_frequency) + " Hz</td></tr>";
  text += "<tr><td>main_temperature</td><td class='right'>" + String(main_temperature) + " &#8451;</td></tr>";
  text += "<tr><td>main_energy_total1</td><td class='right'>" + String(main_energy_total1) + " kWh</td></tr>";
  text += "<tr><td>main_energy_total2</td><td class='right'>" + String(main_energy_total2) + " kWh</td></tr>";
  /*text += "<tr style='display: none'><td>main_energy_total1mantissa</td><td class='right'>" + String(main_energy_total1mantissa) + "</td></tr>";
  text += "<tr style='display: none'><td>main_energy_total1exp</td><td class='right'>" + String(main_energy_total1exp) + "</td></tr>";
  text += "<tr style='display: none'><td>main_energy_total2mantissa</td><td class='right'>" + String(main_energy_total2mantissa) + "</td></tr>";
  text += "<tr style='display: none'><td>main_energy_total2exp</td><td class='right'>" + String(main_energy_total2exp) + "</td></tr>";*/
  text += "</table>";

  text += "<h2>PV</h2>";
  text += "<table style='min-width: 25%;'>";
  text += "<tr><td>pv_energy_power</td><td class='right'>" + String(pv_energy_power * 1000) + " W</td></tr>";
  text += "<tr><td>pv_voltage</td><td class='right'>" + String(pv_voltage) + " V</td></tr>";
  text += "<tr><td>pv_frequency</td><td class='right'>" + String(pv_frequency) + " Hz</td></tr>";
  text += "<tr><td>pv_temperature</td><td class='right'>" + String(pv_temperature) + " &#8451;</td></tr>";
  text += "<tr><td>pv_energy_total1</td><td class='right'>" + String(pv_energy_total1) + " kWh</td></tr>";
  text += "<tr><td>pv_energy_total2</td><td class='right'>" + String(pv_energy_total2) + " kWh</td></tr>";
  text += "<tr><td>pv_energy_total2diff</td><td class='right'>" + String(pv_energy_total2diff) + " kWh</td></tr>";
  /*  text += "<tr style='display: none'><td>pv_energy_total1mantissa</td><td class='right'>" + String(pv_energy_total1mantissa) + "</td></tr>";
    text += "<tr style='display: none'><td>pv_energy_total1exp</td><td class='right'>" + String(pv_energy_total1exp) + "</td></tr>";
    text += "<tr style='display: none'><td>pv_energy_total2mantissa</td><td class='right'>" + String(pv_energy_total2mantissa) + "</td></tr>";
    text += "<tr style='display: none'><td>pv_energy_total2exp</td><td class='right'>" + String(pv_energy_total2exp) + "</td></tr>";*/
  text += "</table>";

  //  text += "<p>UPTIME " + String(tp.tv_sec, DEC) + " sec. / DAY OF WEEK: " + String(dayOfWeek, DEC) + "";
  // text += "Note: Energy consumption: Idle < 1W, 1 fan = 0.1W, 1 heater = ~7W</p>";
  text += "</body></html>";

  // Send page to client
  server.send(200, "text/html", text);
}

/**
   Handle export
*/
void handleExport()
{
  String text;

  // Render html5
  //<meta http-equiv='refresh' content='5'>
  text = "";
  text += String(round(main_energy_power * 1000)) + ";";
  text += String(main_voltage) + ";";
  text += String(main_frequency) + ";";
  text += String(main_temperature) + ";";
  text += String(round(main_energy_total1 * 1000)) + ";";
  text += String(round(main_energy_total2 * 1000)) + ";";
  text += String(round(pv_energy_power * 1000)) + ";";
  text += String(pv_voltage) + ";";
  text += String(pv_frequency) + ";";
  text += String(pv_temperature) + ";";
  text += String(round(pv_energy_total1 * 1000)) + ";";
  text += String(round(pv_energy_total2 * 1000)) + ";";
  text += String(round(pv_energy_total2diff * 1000)) + ";";

  // Send page to client
  server.send(200, "text/html", text);
}

/**
   Handle 404 Page not found
*/
void handleNotFound()
{

  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";

  for (uint8_t i = 0; i < server.args(); i++)
  {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }

  server.send(404, "text/plain", message);
}

/**
  SETUP DEVICE
 **/
void setup()
{
  Serial.begin(115200); // SERIAL
  setCpuFrequencyMhz(80);
  M5.begin();
  M5.Power.begin();
  M5.Speaker.mute();
  // vypnutí reproduktoru
  pinMode(25, OUTPUT);
  dacWrite(25, 0);

  // Init Screen
  m5.Lcd.setBrightness(255);
  m5.Lcd.fillScreen(BLACK); // CLEAR SCREEN
  m5.Lcd.setRotation(1);
  spr.setColorDepth(8);
  spr.createSprite(320, 240);

  // Wifi
  WiFi.enableSTA(true);
  if (!WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS))
  {
    Serial.println("STA Failed to configure");
  }

  Serial.println("Connecting");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  // OTA
  ArduinoOTA.setPort(3232);
  ArduinoOTA.setHostname("Entryway modbus m5stack");
  // No authentication by default
  ArduinoOTA.setPassword((const char *)"m5smodbus");
  ArduinoOTA.onStart([]()
                     { Serial.println("Start"); });
  ArduinoOTA.onEnd([]()
                   { Serial.println("\nEnd"); });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total)
                        { Serial.printf("Progress: %u%%\r", (progress / (total / 100))); });
  ArduinoOTA.onError([](ota_error_t error)
                     {
     Serial.printf("Error[%u]: ", error);
     if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
     else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
     else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
     else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
     else if (error == OTA_END_ERROR) Serial.println("End Failed"); });
  ArduinoOTA.begin();

  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  // Modbus
  mb_init(0xac, 9600, &tim);
  Serial1.begin(9600, SERIAL_8N1, 16, 17);

  // Mqtt client
  client.setServer(mqtt_server, 1883);

  // Webserver
  server.on("/", handleRoot);
  server.on("/export", handleExport);
  /*server.on("/inline", []()
            { server.send(200, "text/plain", "this works as well"); });*/
  server.on("/onair/1", []()
            { onAir = true;  server.send(200, "text/plain", "onair enabled"); });
  server.on("/onair/0", []()
            { onAir = false;  server.send(200, "text/plain", "onair disabled"); });
  server.on("/alarm/1", []()
            { alarmMode = true;  server.send(200, "text/plain", "alarm enabled"); });
  server.on("/alarm/0", []()
            { alarmMode = false;  server.send(200, "text/plain", "alarm disabled"); });
  server.on("/nightmode/1", []()
            { nightMode = true;  server.send(200, "text/plain", "nightmode enabled"); });
  server.on("/nightmode/0", []()
            { nightMode = false;  server.send(200, "text/plain", "nightmode disabled"); });
  server.onNotFound(handleNotFound);
  server.begin();
  Serial.println("HTTP server started");
}

/**
 * Reconnect MQTT broker
 */
void reConnect()
{
  // Loop until we're reconnected
  while (!client.connected())
  {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect(mqtt_id, mqtt_username, mqtt_password))
    {
      Serial.println("connected");
      // Subscribe
      // client.subscribe("entryway-modbus/output");
    }
    else
    {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

/**
 * Read reponse from modbus
 */
bool readResponse()
{

  delay(50);
  readBytes = 0;

  while (Serial1.available() > 0)
  {
    uint8_t inByte = Serial1.read();
    data[readBytes] = inByte;
    Serial.printf(">");
    Serial.print(inByte);
    Serial.printf(" ");

    delay(10);
    readBytes++;
    if (readBytes >= 10)
      return false;
  }

  delay(50);

  return true;
}

/**
 * Main loop
 */
void loop()
{
  M5.update();
  ArduinoOTA.handle();
  server.handleClient();

  // ###########################################################
  // Display
  // ###########################################################
  spr.fillRect(0, 0, 320, 240, BLACK);
  spr.setFreeFont(FSS12);
  spr.setTextColor(WHITE);

  spr.setTextSize(1);
  spr.setCursor(10, 20);
  spr.setTextDatum(TL_DATUM);
  spr.println(main_voltage);
  spr.setCursor(110, 20);
  spr.println(main_frequency);
  spr.setCursor(210, 20);
  spr.println(main_temperature);
  spr.setCursor(10, 110);
  spr.println(main_energy_total1);
  spr.setCursor(160, 110);
  spr.println(main_energy_total2);
  spr.setTextSize(3);
  spr.setCursor(10, 80);
  spr.setTextDatum(TL_DATUM);
  spr.println((main_energy_power * 1000));
  spr.fillRect(0, 118, 320, 1, GREEN);

  spr.setTextSize(1);
  spr.setCursor(10, 120 + 20);
  spr.setTextDatum(TL_DATUM);
  spr.println(pv_voltage);
  spr.setCursor(110, 120 + 20);
  spr.println(pv_frequency);
  spr.setCursor(210, 120 + 20);
  spr.println(pv_temperature);
  spr.setCursor(10, 120 + 110);
  spr.println(pv_energy_total1);
  spr.setCursor(160, 120 + 110);
  spr.println(pv_energy_total2);
  spr.setTextSize(3);
  spr.setCursor(10, 120 + 80);
  spr.setTextDatum(TL_DATUM);
  spr.println((pv_energy_power * 1000));

  brightnessVal = (nightMode && !onAir && !alarmMode ? 5 : 255);
  if (brightnessVal != oldBrightnessVal)
  {
    tft.setBrightness(brightnessVal);
    oldBrightnessVal = brightnessVal;
  }

  if (onAir)
  {
    spr.fillRect(0, 0, 320, 240, RED);
    spr.setTextSize(3);
    spr.setCursor(10, 80);
    spr.setTextDatum(TL_DATUM);
    spr.println("ON AIR");
  }
  if (alarmMode)
  {
    spr.fillRect(0, 0, 320, 240, RED);
    spr.setTextSize(3);
    spr.setCursor(10, 80);
    spr.setTextDatum(TL_DATUM);
    spr.println("ALARM ON");
  }

  spr.pushSprite(0, 0);

  // ###########################################################
  // MODBUS START
  // ###########################################################
  // # U1 T5
  // 51 4 0 107 0 2 4 5
  // >51 >4 >4 >255 >0 >9 >86 >94 >61
  char tempString[15];
  char data_str1[] = {51, 4, 0, 107, 0, 2};
  mb_send_frame((uint8_t *)data_str1, 6);
  Serial.println("");
  if (readResponse())
  {
    if (data[0] == 51 && data[1] == 4 && readBytes == 9)
    {
      main_voltage = (float)((data[4] << 16) + (data[5] << 8) + data[6]) / 10.0;
      dtostrf(main_voltage, 1, 2, tempString);
      client.publish("home/entryway_modbus/main_voltage", tempString);
      Serial.println("OK");
    }
  }

  // # Frequency  T5
  // 51 4 0 105 0 2 165 197
  // >51 >4 >4 >254 >0 >19 >138 >85 >56
  char data_str2[] = {51, 4, 0, 105, 0, 2};
  mb_send_frame((uint8_t *)data_str2, 6);
  Serial.println("");
  if (readResponse())
  {
    if (data[0] == 51 && data[1] == 4 && readBytes == 9)
    {
      main_frequency = (float)((data[4] << 16) + (data[5] << 8) + data[6]) / 100.0;
      dtostrf(main_frequency, 1, 2, tempString);
      client.publish("home/entryway_modbus/main_frequency", tempString);
      Serial.println("OK");
    }
  }

  // # Internal Temperature T17
  // 51 4 0 181 0 1 36 62
  // >51 >4 >2 >12 >28 >132 >61
  char data_str3[] = {51, 4, 0, 181, 0, 1};
  mb_send_frame((uint8_t *)data_str3, 6);
  Serial.println("");
  if (readResponse())
  {
    if (data[0] == 51 && data[1] == 4 && readBytes == 7)
    {
      main_temperature = (float)((data[3] << 8) + data[4]) / 100.0;
      dtostrf(main_temperature, 1, 2, tempString);
      client.publish("home/entryway_modbus/main_temperature", tempString);
      Serial.println("OK");
    }
  }

  // # Active Power Total (Pt) T6
  // 51 4 0 140 0 2 180 50
  // >51 >4 >4 >255 >0 >0 >0 >216 >83
  char data_str4[] = {51, 4, 0, 140, 0, 2};
  mb_send_frame((uint8_t *)data_str4, 6);
  Serial.println("");
  if (readResponse())
  {
    if (data[0] == 51 && data[1] == 4 && readBytes == 9)
    {
      tmpInt32 = (((data[4] & 0x7f) << 16) + (data[5] << 8) + data[6]);
      if (bitRead(data[4], 7) == 1)
        tmpInt32 = tmpInt32 - 0x7FFFFF;
      main_energy_power = tmpInt32 / 10000.0;
      dtostrf(main_energy_power, 1, 5, tempString);
      client.publish("home/entryway_modbus/main_energy_power", tempString);
      Serial.println("OK");
    }
  }

  // # 4 30401 Energy Counter n1 Exponent T2 Exponent
  // 51 4 1 145 0 1 69 247
  // >51 >4 >2 >0 >0 >128 >244
  char data_str5[] = {51, 4, 1, 145, 0, 1};
  mb_send_frame((uint8_t *)data_str5, 6);
  Serial.println("");
  if (readResponse())
  {
    if (data[0] == 51 && data[1] == 4 && readBytes == 7)
    {
      tmpInt32 = (((data[3] & 0x7f) << 8) + data[4]);
      if (bitRead(data[3], 7) == 1)
        tmpInt32 = tmpInt32 - 0x7FFF;
      main_energy_total1exp = tmpInt32;
      dtostrf(main_energy_total1exp, 1, 3, tempString);
      client.publish("home/entryway_modbus/main_total_power1exp", tempString);
      Serial.println("OK");
    }
  }

  // # 4 30402 Energy Counter n2 Exponent T2 Exponent
  // 51 4 1 146 0 1 244 54
  // >51 >4 >2 >0 >0 >128 >244
  char data_str6[] = {51, 4, 1, 146, 0, 1};
  mb_send_frame((uint8_t *)data_str6, 6);
  Serial.println("");
  if (readResponse())
  {
    if (data[0] == 51 && data[1] == 4 && readBytes == 7)
    {
      tmpInt32 = (((data[3] & 0x7f) << 8) + data[4]);
      if (bitRead(data[3], 7) == 1)
        tmpInt32 = tmpInt32 - 0x7FFF;
      main_energy_total2exp = tmpInt32;
      dtostrf(main_energy_total2exp, 1, 3, tempString);
      client.publish("home/entryway_modbus/main_total_power2exp", tempString);
      Serial.println("OK");
    }
  }

  // # 4 30406 30407 Energy Counter n1 T3 Mantissa x EXP(Register 30401) x 1 Wh
  // 51 4 1 150 0 2 116 59
  // >51 >4 >4 >0 >0 >0 >0 >232 >71
  char data_str7[] = {51, 4, 1, 150, 0, 2};
  mb_send_frame((uint8_t *)data_str7, 6);
  Serial.println("");
  if (readResponse())
  {
    if (data[0] == 51 && data[1] == 4 && readBytes == 9)
    {
      tmpInt32 = (((data[3] << 24) + (data[4] << 16) + (data[5] << 8) + data[6]));
      main_energy_total1 = main_energy_total1mantissa = tmpInt32;
      dtostrf(main_energy_total1, 1, 3, tempString);
      client.publish("home/entryway_modbus/main_total_power1mantissa", tempString);
      main_energy_total1 = main_energy_total1 * pow(10, main_energy_total1exp) / 1000.0;
      dtostrf(main_energy_total1, 1, 3, tempString);
      client.publish("home/entryway_modbus/main_total_power1", tempString);
      Serial.println("OK");
    }
  }

  // # 4 30408 30409 Energy Counter n2 T3 Mantissa x EXP(Register 30402) x 1 Wh
  // 51 4 1 152 0 2 213 251
  // >51 >4 >4 >0 >0 >0 >0 >232 >71
  char data_str8[] = {51, 4, 1, 152, 0, 2};
  mb_send_frame((uint8_t *)data_str8, 6);
  Serial.println("");
  if (readResponse())
  {
    if (data[0] == 51 && data[1] == 4 && readBytes == 9)
    {
      tmpInt32 = (((data[3] << 24) + (data[4] << 16) + (data[5] << 8) + data[6]));
      main_energy_total2 = main_energy_total2mantissa = tmpInt32;
      dtostrf(main_energy_total2, 1, 3, tempString);
      client.publish("home/entryway_modbus/main_total_power2mantissa", tempString);
      main_energy_total2 = main_energy_total2 * pow(10, main_energy_total2exp) / 1000.0;
      dtostrf(main_energy_total2, 1, 3, tempString);
      client.publish("home/entryway_modbus/main_total_power2", tempString);
      Serial.println("OK");
    }
  }

  // ########################################################################################
  // ## PV meter
  // ########################################################################################

  // # U1 T5
  // 52 4 0 107 0 2 4 5
  // >52 >4 >4 >255 >0 >9 >86 >94 >61
  char data_str11[] = {52, 4, 0, 107, 0, 2};
  mb_send_frame((uint8_t *)data_str11, 6);
  Serial.println("");
  if (readResponse())
  {
    if (data[0] == 52 && data[1] == 4 && readBytes == 9)
    {
      pv_voltage = (float)((data[4] << 16) + (data[5] << 8) + data[6]) / 10.0;
      dtostrf(pv_voltage, 1, 2, tempString);
      client.publish("home/entryway_modbus/pv_voltage", tempString);
      Serial.println("OK");
    }
  }

  // # Frequency  T5
  // 52 4 0 105 0 2 165 197
  // >52 >4 >4 >254 >0 >19 >138 >85 >56
  char data_str12[] = {52, 4, 0, 105, 0, 2};
  mb_send_frame((uint8_t *)data_str12, 6);
  Serial.println("");
  if (readResponse())
  {
    if (data[0] == 52 && data[1] == 4 && readBytes == 9)
    {
      pv_frequency = (float)((data[4] << 16) + (data[5] << 8) + data[6]) / 100.0;
      dtostrf(pv_frequency, 1, 2, tempString);
      client.publish("home/entryway_modbus/pv_frequency", tempString);
      Serial.println("OK");
    }
  }

  // # Internal Temperature T17
  // 52 4 0 181 0 1 36 62
  // >52 >4 >2 >12 >28 >132 >61
  char data_str13[] = {52, 4, 0, 181, 0, 1};
  mb_send_frame((uint8_t *)data_str13, 6);
  Serial.println("");
  if (readResponse())
  {
    if (data[0] == 52 && data[1] == 4 && readBytes == 7)
    {
      pv_temperature = (float)((data[3] << 8) + data[4]) / 100.0;
      dtostrf(pv_temperature, 1, 2, tempString);
      client.publish("home/entryway_modbus/pv_temperature", tempString);
      Serial.println("OK");
    }
  }

  // # Active Power Total (Pt) T6
  // 52 4 0 140 0 2 180 50
  // >52 >4 >4 >255 >0 >0 >0 >216 >83
  char data_str14[] = {52, 4, 0, 140, 0, 2};
  mb_send_frame((uint8_t *)data_str14, 6);
  Serial.println("");
  if (readResponse())
  {
    if (data[0] == 52 && data[1] == 4 && readBytes == 9)
    {
      tmpInt32 = (((data[4] & 0x7f) << 16) + (data[5] << 8) + data[6]);
      if (bitRead(data[4], 7) == 1)
        tmpInt32 = tmpInt32 - 0x7FFFFF;
      pv_energy_power = (-1) * (tmpInt32 / 10000.0);
      dtostrf(pv_energy_power, 1, 5, tempString);
      client.publish("home/entryway_modbus/pv_energy_power", tempString);
      Serial.println("OK");
    }
  }

  // # 4 30401 Energy Counter n1 Exponent T2 Exponent
  // 52 4 1 145 0 1 69 247
  // >52 >4 >2 >0 >0 >128 >244
  char data_str15[] = {52, 4, 1, 145, 0, 1};
  mb_send_frame((uint8_t *)data_str15, 6);
  Serial.println("");
  if (readResponse())
  {
    if (data[0] == 52 && data[1] == 4 && readBytes == 7)
    {
      tmpInt32 = (((data[3] & 0x7f) << 8) + data[4]);
      if (bitRead(data[3], 7) == 1)
        tmpInt32 = tmpInt32 - 0x7FFF;
      pv_energy_total1exp = tmpInt32;
      dtostrf(pv_energy_total1exp, 1, 3, tempString);
      client.publish("home/entryway_modbus/pv_total_power1exp", tempString);
      Serial.println("OK");
    }
  }

  // # 4 30402 Energy Counter n2 Exponent T2 Exponent
  // 52 4 1 146 0 1 244 54
  // >52 >4 >2 >0 >0 >128 >244
  char data_str16[] = {52, 4, 1, 146, 0, 1};
  mb_send_frame((uint8_t *)data_str16, 6);
  Serial.println("");
  if (readResponse())
  {
    if (data[0] == 52 && data[1] == 4 && readBytes == 7)
    {
      tmpInt32 = (((data[3] & 0x7f) << 8) + data[4]);
      if (bitRead(data[3], 7) == 1)
        tmpInt32 = tmpInt32 - 0x7FFF;
      pv_energy_total2exp = tmpInt32;
      dtostrf(pv_energy_total2exp, 1, 3, tempString);
      client.publish("home/entryway_modbus/pv_total_power2exp", tempString);
      Serial.println("OK");
    }
  }

  // # 4 30406 30407 Energy Counter n1 T3 Mantissa x EXP(Register 30401) x 1 Wh
  // 52 4 1 150 0 2 116 59
  // >52 >4 >4 >0 >0 >0 >0 >232 >71
  char data_str17[] = {52, 4, 1, 150, 0, 2};
  mb_send_frame((uint8_t *)data_str17, 6);
  Serial.println("");
  if (readResponse())
  {
    if (data[0] == 52 && data[1] == 4 && readBytes == 9)
    {
      tmpInt32 = (((data[3] << 24) + (data[4] << 16) + (data[5] << 8) + data[6]));
      pv_energy_total1 = pv_energy_total1mantissa = tmpInt32;
      dtostrf(pv_energy_total1, 1, 3, tempString);
      client.publish("home/entryway_modbus/pv_total_power1mantissa", tempString);
      pv_energy_total1 = pv_energy_total1 * pow(10, pv_energy_total1exp) / 1000.0;
      dtostrf(pv_energy_total1, 1, 3, tempString);
      client.publish("home/entryway_modbus/pv_total_power1", tempString);
      Serial.println("OK");
    }
  }

  // # 4 30408 30409 Energy Counter n2 T3 Mantissa x EXP(Register 30402) x 1 Wh
  // 52 4 1 152 0 2 213 252
  // >52 >4 >4 >0 >0 >0 >0 >232 >71
  char data_str18[] = {52, 4, 1, 152, 0, 2};
  mb_send_frame((uint8_t *)data_str18, 6);
  Serial.println("");
  if (readResponse())
  {
    if (data[0] == 52 && data[1] == 4 && readBytes == 9)
    {
      tmpInt32 = (((data[3] << 24) + (data[4] << 16) + (data[5] << 8) + data[6]));
      pv_energy_total2 = pv_energy_total2mantissa = tmpInt32;
      dtostrf(pv_energy_total2, 1, 3, tempString);
      client.publish("home/entryway_modbus/pv_total_power2mantissa", tempString);
      pv_energy_total2 = pv_energy_total2 * pow(10, pv_energy_total2exp) / 1000.0;
      dtostrf(pv_energy_total2, 1, 3, tempString);
      client.publish("home/entryway_modbus/pv_total_power2", tempString);
      pv_energy_total2diff = pv_energy_total2 - pv_energy_total1;
      dtostrf(pv_energy_total2diff, 1, 3, tempString);
      client.publish("home/entryway_modbus/pv_total_power2diff", tempString);
      Serial.println("OK");
    }
  }

  // ###########################################################
  // MODBUS END
  // ###########################################################

  // ###########################################################
  // Mqtt client
  // ###########################################################
  if (!client.connected())
  {
    reConnect();
  }
  client.loop();

  //
  delay(50);
}

/**
 * Send 1 byte to modbus
 */
void mb_send_one_byte(uint8_t data)
{
  Serial.print(data);
  Serial.printf(" ");
  Serial1.write(data);
}

/**
 * Modbus error
 */
void mb_get_frame_error_cb(eMBErrorCode)
{
  Serial.printf("mb_get_frame_error_cb ... \r\n");
}

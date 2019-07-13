/*
  Código que lee un sensor de humedad y temperatura DHT11 y envia las lecturas sobre wifi a un socket TCP
  además realiza consultas NTP periódicas para añadir un timestamp a las lecturas del sensor, añade actualizaciones OTA
  placa de desarrollo Node-MCU V1.0 (ESP-12E)
  sensor de humedad y temperatura DHT11
*/


#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <DHT11.h>
#include <ArduinoOTA.h>
#include <ESP8266mDNS.h>

int pin = D2;
unsigned long previousMillis = 0;
unsigned long interval = 60000;
const char* ssid     = "XXXXXXXXX";      // SSID
const char* password = "XXXXXXXXX";      // Password
const char* host = "192.168.1.128";  // IP serveur - Server IP
const int   port = 5045;            // Port serveur - Server Port
String cmd = "";

unsigned int localPort = 2390;      // local port to listen for UDP packets
IPAddress timeServerIP; // time.google.com NTP server address
const char* ntpServerName = "time.google.com";
const int NTP_PACKET_SIZE = 48; // NTP time stamp is in the first 48 bytes of the message
byte packetBuffer[ NTP_PACKET_SIZE]; //buffer to hold incoming and outgoing packets

WiFiClient client;
WiFiUDP udp;
DHT11 dht11(pin);

void setup() {
  Serial.begin(115200);
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  // Port defaults to 8266
  ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[ChipID]
  ArduinoOTA.setHostname("myesp8266");

  // No authentication by default
  ArduinoOTA.setPassword("admin");

  // Password can be set with it's md5 value as well
  // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
  // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_SPIFFS
      type = "filesystem";
    }

    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      Serial.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      Serial.println("End Failed");
    }
  });
  ArduinoOTA.begin();

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  Serial.println("Starting UDP");
  udp.begin(localPort);
  Serial.print("Local port: ");
  Serial.println(udp.localPort());
}

void loop()
{
  ArduinoOTA.handle();

  WiFi.hostByName(ntpServerName, timeServerIP);
  unsigned long currentMillis = millis();

  if (currentMillis - previousMillis >= interval) {

    previousMillis = currentMillis;

    sendNTPpacket(timeServerIP); // send an NTP packet to a time server
    delay(1000);

    int cb = udp.parsePacket();
    if (!cb) {
      Serial.println("no packet yet");
    } else {
      Serial.print("packet received, length=");
      Serial.println(cb);
      udp.read(packetBuffer, NTP_PACKET_SIZE); // read the packet into the buffer
      unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
      unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
      unsigned long secsSince1900 = highWord << 16 | lowWord;
      const unsigned long seventyYears = 2208988800UL;
      unsigned long epoch = secsSince1900 - seventyYears;
      client.print(((epoch  % 86400L) / 3600) + 2); // print the hour (86400 equals secs per day)
      client.print(':');
      if (((epoch % 3600) / 60) < 10) {
        client.print('0');
      }
      client.print((epoch  % 3600) / 60); // print the minute (3600 equals secs per minute)
      client.print(':');
      if ((epoch % 60) < 10) {
        client.print('0');
      }
      client.print(epoch % 60); // print the second
      client.print(", ");
      client.flush();

      readDHT11();

    }
  }
}
void readDHT11() {
  int err;
  float temp, hum;
  if ((err = dht11.read(hum, temp)) == 0)   // Si devuelve 0 es que ha leido bien
  {
    client.connect(host, port);
    Serial.println("conectado");
    client.print("Temperatura: ");
    client.print(temp);
    client.print(", ");
    client.print(" Humedad: ");
    client.print(hum);
    client.println();
    client.flush();
  }
  else
  {
    client.connect(host, port);
    Serial.println("error");
    client.println();
    client.print("Error Num :");
    client.print(err);
    client.println();
    client.flush();
  }
}
void sendNTPpacket(IPAddress & address) {
  Serial.println("sending NTP packet...");
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  packetBuffer[12]  = 49;
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;
  udp.beginPacket(address, 123);
  udp.write(packetBuffer, NTP_PACKET_SIZE);
  udp.endPacket();
}

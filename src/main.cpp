// Gateway

#include <Arduino.h>
#include <ArduinoJson.h>          // JSON library for Arduino
#include <TinyGsmClientSIM7600.h> // Library for GSM communication using SIM7600 module
#include <LoRa.h>                 // Library for LoRa communication
#include <Wire.h>                 // I2C communication library
#include <HardwareSerial.h>
#include <TinyGPS++.h>

#define SerialAT Serial1
#define SerialMon Serial

TinyGsmSim7600 modem(SerialAT);
TinyGsmSim7600::GsmClientSim7600 client(modem);
TinyGPSPlus gps;

// Define Pin Configurations
#define SerialAT Serial1
#define SerialMon Serial
#define MIN_SPACE 500000
#define UART_BAUD 115200
#define PIN_DTR 25
#define PIN_TX 26
#define PIN_RX 27
#define PWR_PIN 4
#define PIN_RST 5
#define PIN_SUP 12
#define BAT_ADC 35
#define ss 15
#define rst 14
#define dio0 13
#define apiKey (String) "pk.71031a62fba9814c0898ae766b971df1"

// Global variables
bool DEBUG = true;
String response, LoRaData, latText, lonText, gpsinfo;
String serialres;
String latitude, longitude;
String extractedString = "";
String NodeName;
float degrees = 0.0;
float lat, lon;
float temp, humi;
unsigned long startTime;

// LoRa configuration
RTC_DATA_ATTR int gSyncWord;
RTC_DATA_ATTR int gTxPower;
RTC_DATA_ATTR long gfreq;
RTC_DATA_ATTR double ginterval;
RTC_DATA_ATTR int gspreadingFactor;
RTC_DATA_ATTR long gsignalBandwidth;

// Default LoRa Gateway configuration
int dgSyncWord = 0xF1;
int dgTxPower = 20;
int dgspreadingFactor = 9;
long dgfreq = 923E6;
long dgsignalBandwidth = 125E3;
double dginterval = 3;

// End devices LoRa configuration
int eSyncWord;
int eTxPower;
int espreadingFactor;
long efreq;
long esignalBandwidth;

double interval;
int sendSyncWord = 5;

// Network information
struct NETWORK_INFO
{
  // Location
  String date;
  String time;
  String lat;
  String lon;

  // Cell site information
  char type[10];
  char mode[10];
  String mcc;
  String mnc;
  int lac = 0;
  String cid;
  char freq_b[15];
  double rsrq = 0;
  double rsrp = 0;
  double rssi = 0;
  int rssnr;
};
NETWORK_INFO networkinfo;

void modemPowerOn()
{
  const int SUPPLY_PIN = PIN_SUP;
  const int RESET_PIN = PIN_RST;
  const int POWER_PIN = PWR_PIN;

  pinMode(SUPPLY_PIN, OUTPUT);
  digitalWrite(SUPPLY_PIN, HIGH);

  pinMode(RESET_PIN, OUTPUT);
  digitalWrite(RESET_PIN, LOW);
  delay(100);
  digitalWrite(RESET_PIN, HIGH);
  delay(3000);
  digitalWrite(RESET_PIN, LOW);

  pinMode(POWER_PIN, OUTPUT);
  digitalWrite(POWER_PIN, LOW);
  delay(100);
  digitalWrite(POWER_PIN, HIGH);
  delay(1000);
  digitalWrite(POWER_PIN, LOW);
}

String sendAT(String command, int interval, boolean debug)
{
  String response = "";
  SerialAT.println(command);
  long int startTime = millis();

  while (((millis() - startTime)) < interval)
  {
    while (SerialAT.available() > 0)
    {
      int readData = SerialAT.read();
      response += char(readData);
    }
  }
  SerialAT.flush();

  if (debug)
  {
    SerialMon.print(response);
  }

  return response;
}

float convertlatCoordinate(String coordString)
{
  degrees = 0.0;
  String degreesString = coordString.substring(0, 2);
  int degreesValue = degreesString.toInt();

  String minutesString = coordString.substring(2);
  float minutesValue = minutesString.toFloat();

  degrees = degreesValue + (minutesValue / 60.0);

  Serial.print("Latitude : ");
  Serial.println(degrees, 6);
  return degrees;
}

float convertlonCoordinate(String coordString)
{
  degrees = 0.0;
  String degreesString = coordString.substring(0, 3);
  int degreesValue = degreesString.toInt();

  String minutesString = coordString.substring(3);
  float minutesValue = minutesString.toFloat();

  degrees = degreesValue + (minutesValue / 60.0);

  Serial.print("Longtitude : ");
  Serial.println(degrees, 6);
  return degrees;
}

bool readgps(int interval)
{
  serialres = "";
  lat = 0;
  lon = 0;
  long int startTime = millis();
  while (((millis() - startTime)) < interval)
  {
    while (Serial2.available() > 0)
    {
      int readData = Serial2.read();
      serialres += char(readData);
    }
  }
  Serial2.flush();
  if (serialres.indexOf(",A") != -1 && serialres.indexOf("$GPRMC") != -1)
  {
    int startPos = serialres.indexOf("$GPRMC");
    int endPos = serialres.indexOf("E", startPos);
    extractedString = serialres.substring(startPos, endPos + 2);
    Serial.println(extractedString);
    // Find the positions of 'N,' and 'E,' in the input string
    int nPos = serialres.indexOf("N,");
    int ePos = serialres.indexOf("E,");

    // Check if 'N,' and 'E,' were found in the input string
    if (nPos != -1 && ePos != -1)
    {
      // Extract the latitude and longitude substrings
      latitude = extractedString.substring(nPos - 11, nPos);
      lat = convertlatCoordinate(latitude);
      latText = String(lat, 6);

      longitude = extractedString.substring(ePos - 12, ePos);
      lon = convertlonCoordinate(longitude);
      lonText = String(lon, 6);
      return true;
    }
  }
  else
  {
    return false;
  }
  return false;
}

void waitForGPSFix(float timeoutMinutes)
{
  unsigned long startTime = millis();
  unsigned long timeoutMillis = timeoutMinutes * 60000;

  while (millis() - startTime < timeoutMillis)
  {
    lat = 0;
    lon = 0;
    unsigned long elapsedTime = (millis() - startTime) / 1000;

    if (readgps(300) && lat > 13 && lat < 14 && lon > 100 && lon < 101)
    {
      // GPS fix found
      break;
    }
  }
}

float calculateAverage(float arrayofval[], int sizes)
{
  float sum = 0;

  for (int i = 0; i < sizes; i++)
  {
    sum += arrayofval[i];
  }

  return sum / sizes;
}

void GPSavg(int attemps)
{
  float latlist[attemps], lonlist[attemps];

  for (int i = 0; i < attemps; i++)
  {
    Serial.print("\n------------------ ");
    Serial.print(i + 1);
    Serial.println(" ------------------");
    waitForGPSFix(1);
    latlist[i] = lat;
    lonlist[i] = lon;
  }
  latText = String(calculateAverage(latlist, attemps), 6);
  lonText = String(calculateAverage(lonlist, attemps), 6);

  Serial.print("\n------------------ ");
  Serial.print("All values");
  Serial.println(" ------------------");
  for (int i = 0; i < attemps; i++)
  {
    Serial.print("Latitude ");
    Serial.print(i);
    Serial.print(": ");
    Serial.print(String(latlist[i], 6));
    Serial.print("  Longitude ");
    Serial.print(i);
    Serial.print(": ");
    Serial.println(String(lonlist[i], 6));
  }
  Serial.println("------------------------------------------------");
}

void readcellinfo()
{
  SerialMon.println("\n----------   Start of readcellinfo()   ----------\n");
  // Send AT command to retrieve cell info
  String info = sendAT("AT+CPSI?", 10000, 1);

  // Process the response
  int startIndex = info.indexOf("+CPSI: ");
  info = info.substring(startIndex + 7, startIndex + 80);
  info.replace("\r", "");
  info.replace("\n", "");
  info.replace("AT+CLBS=4ERROR", "");
  info.replace("AT+CPSI?+CPSI: NO SERVICE", "");

  // Extract the values from the response
  int commaIndex = 0;
  int lastCommaIndex = 0;
  String values[15];
  for (int i = 0; i < 15; i++)
  {
    commaIndex = info.indexOf(',', lastCommaIndex);
    String temp = info.substring(lastCommaIndex, commaIndex);
    values[i] = temp;
    lastCommaIndex = commaIndex + 1;
  }

  // Convert and store the relevant values in the networkinfo struct
  int lacDec = (int)strtol(values[3].c_str(), NULL, 16);
  networkinfo.rssnr = values[13].toInt();
  networkinfo.mcc = values[2].substring(0, 3);
  networkinfo.mnc = values[2].substring(4, 6);
  networkinfo.lac = lacDec;
  networkinfo.cid = values[4];
  delay(1000);
  Serial.println("Base station info");
  Serial.println("RSSNR : " + String(networkinfo.rssnr));
  Serial.println("MCC   : " + networkinfo.mcc);
  Serial.println("MNC   : " + networkinfo.mnc);
  Serial.println("LAC   : " + String(networkinfo.lac));
  Serial.println("Cell id  : " + networkinfo.cid);
  SerialMon.println("\n----------   End of readcellinfo()   ----------\n");
}

void sendLocationRequest()
{
  SerialMon.println("\n----------   Start of sendLocationRequest()   ----------\n");
  String payload = "{\"token\":\"" + apiKey + "\",\"radio\":\"lte\",\"mcc\":" + networkinfo.mcc + ",\"mnc\":" + networkinfo.mnc + ",\"cells\":[{\"lac\":" + networkinfo.lac + ",\"cid\":" + networkinfo.cid + ",\"psc\":0}],\"address\":1}";
  String response;
  client.connect("ap1.unwiredlabs.com", 80);
  String request = "POST /v2/process.php HTTP/1.1\r\n";
  request += "Host: ap1.unwiredlabs.com\r\n";
  request += "Content-Type: application/x-www-form-urlencoded\r\n";
  request += "Content-Length: ";
  request += String(payload.length());
  request += "\r\n\r\n";
  request += payload;
  client.print(request);
  while (client.connected())
  {
    while (client.available())
    {
      char c = client.read();
      response += c;
      client.write(c);
    }
  }
  client.stop();
  Serial.println("Response =" + response);
  int startIndex = response.indexOf("\"lat\":");
  int endIndex = response.indexOf(",\"lon\":");
  latText = response.substring(startIndex + 6, endIndex);
  startIndex = endIndex + 7;
  endIndex = response.indexOf(",\"accuracy\":");
  lonText = response.substring(startIndex, endIndex);
  Serial.println("Latitude: " + latText);
  Serial.println("Longitude: " + lonText);
  SerialMon.println("\n----------   End of sendLocationRequest()   ----------\n");
}

void sendHttpRequest()
{
  bool DEBUG = true;
  SerialMon.println("\n----------   Start of sendHttpRequest()   ----------\n");

  String http_str = "AT+HTTPPARA=\"URL\",\"https://tuz1jwn73m.execute-api.ap-southeast-1.amazonaws.com/data?"
                    "nodename=" +
                    NodeName +
                    "&temperature=" + temp +
                    "&humidity=" + humi +
                    "&latitude=" + latText +
                    "&longitude=" + lonText + "\"\r\n";

  Serial.println(http_str);
  sendAT("AT+HTTPINIT", 2000, DEBUG);
  sendAT(http_str, 2000, DEBUG);
  sendAT("AT+HTTPACTION=0", 3000, DEBUG);
  sendAT("AT+HTTPTERM", 2000, DEBUG);

  delay(1000);
  SerialMon.println("\n----------   End of sendHttpRequest()   ----------\n");
}

String fetchJsonConfig()
{
  bool DEBUG = true;
  SerialMon.println("\n---------- Start of fetchJsonConfig() ----------\n");
  String jsonConfig;

  for (int i = 0; i < 5; i++)
  {
    if (jsonConfig.indexOf('{') != -1)
    {
      break;
    }
    String httpCommand = "AT+HTTPPARA=\"URL\",\"https://tuz1jwn73m.execute-api.ap-southeast-1.amazonaws.com/showconfig\"";

    sendAT("AT+HTTPINIT", 3000, DEBUG);
    sendAT(httpCommand, 3000, DEBUG);
    sendAT("AT+HTTPACTION=0", 3000, DEBUG);
    jsonConfig = sendAT("AT+HTTPREAD=0,500", 3000, DEBUG);
    sendAT("AT+HTTPTERM", 2000, DEBUG);
  }

  String jsonPart;
  int startPos = jsonConfig.indexOf('{');
  int endPos = jsonConfig.lastIndexOf('}');

  if (startPos != -1 && endPos != -1)
  {
    jsonPart = jsonConfig.substring(startPos, endPos + 1);
    Serial.println(jsonPart);
  }
  else
  {
    Serial.println("JSON not found in the response.");
  }
  SerialMon.println("\n---------- End of fetchJsonConfig() ----------\n");
  return jsonPart;
}

void changelora(String jsonInput)
{
  Serial.print(jsonInput);
  StaticJsonDocument<512> doc;
  DeserializationError error = deserializeJson(doc, jsonInput);
  if (error)
  {
    Serial.print("Parsing failed: ");
    Serial.println(error.c_str());
    return;
  }

  if (eSyncWord != gSyncWord)
  {
    gSyncWord = eSyncWord;
    Serial.println(gSyncWord);
    Serial.println("SyncWord changed");
  }
  if (eTxPower != gTxPower)
  {
    gTxPower = eTxPower;
    Serial.println("TxPower changed");
  }
  if (efreq * 1000000 != gfreq)
  {
    gfreq = efreq * 1000000;
    Serial.println("TxPower changed");
  }
  if (ginterval != interval)
  {
    ginterval = interval;
    Serial.println("interval changed");
  }

  Serial.print("\nSyncWord: ");
  Serial.println(gSyncWord, HEX);
  Serial.print("TxPower: ");
  Serial.println(gTxPower);
  Serial.print("Frequency: ");
  Serial.println(gfreq);
  Serial.print("Interval: ");
  Serial.println(ginterval);
  Serial.print("SpreadingFactor: ");
  Serial.println(gspreadingFactor);
  Serial.print("SignalBandwidth: ");
  Serial.println(gsignalBandwidth);
}

String createJsonString(int SyncWord, int TxPower, long freq, double interval)
{
  Serial.println("\n----------   Start of createJsonString()   ----------\n");
  StaticJsonDocument<512> doc;

  doc["SyncWord"] = SyncWord;
  doc["TxPower"] = TxPower;
  doc["freq"] = freq * 1000000;
  doc["interval"] = interval;

  String jsonString;
  serializeJson(doc, jsonString);

  Serial.println("\n---------- End of createJsonString() ----------\n");

  return jsonString;
}

void parseJsonConfig(String jsonInput)
{
  StaticJsonDocument<512> doc;
  DeserializationError error = deserializeJson(doc, jsonInput);

  if (error)
  {
    Serial.print("Parsing failed: ");
    Serial.println(error.c_str());
    eSyncWord = dgSyncWord;
    eTxPower = dgTxPower;
    efreq = dgfreq;
    interval = dginterval;
    return;
  }

  eSyncWord = doc["Syncword"];
  eTxPower = doc["Tx_power"];
  efreq = doc["Frequency"];
  interval = doc["Tx_Interval"];

  changelora(jsonInput);
}

bool connect2LTE()
{
  SerialMon.println("\n----------   Start of connect2LTE()   ----------\n");
  boolean DEBUG = 1;

  delay(1000);
  sendAT("AT+NETCLOSE", 1000, DEBUG);

  delay(1000);
  sendAT("AT+CPIN?", 2000, DEBUG);

  delay(1000);
  sendAT("AT+CSOCKSETPN=1", 5000, DEBUG);
  String res = sendAT("AT+NETOPEN", 5000, DEBUG);

  if (res.indexOf("OK") == -1 || res.indexOf("not") != -1)
  {
    esp_restart();
  }

  String response = sendAT("AT+IPADDR", 5000, DEBUG);

  // sendAT("AT+CPING=\"rung.ddns.net\",1,4", 10000, DEBUG);
  SerialMon.println("\n----------   End of connect2LTE()   ----------\n");
  return true;
}

void processJsonInput(const char *jsonInput)
{
  SerialMon.println("\n----------   Start of processJsonInput()   ----------\n");
  StaticJsonDocument<512> doc;
  DeserializationError error = deserializeJson(doc, jsonInput);

  if (error)
  {
    Serial.print("Parsing failed: ");
    Serial.println(error.c_str());
    return;
  }

  NodeName = doc["NodeName"].as<String>();
  temp = doc["Temperature"];
  humi = doc["Humidity"];
  SerialMon.println("\n----------   End of processJsonInput()   ----------\n");
}

void sleep(float sec)
{
  Serial.println("\n----------   Start of sleep()   ----------\n");
  double min_d = sec / 60;
  // Set wakeup time
  esp_sleep_enable_timer_wakeup((ginterval - min_d) * 60 * 0.8 * 1000000);

  // Print the duration in minutes to the serial monitor
  Serial.print("Duration: ");
  Serial.print(sec / 60);
  Serial.println(" minutes");

  // Go to sleep now
  Serial.print("Going to sleep for ");
  Serial.print((ginterval - min_d) * 0.8);
  Serial.println(" minutes");
  Serial.println("\n----------   End of sleep()   ----------\n");
  esp_deep_sleep_start();
}

void setup()
{
  SerialMon.println("\n----------   Start of Setup   ----------\n");
  startTime = millis();
  Serial.begin(UART_BAUD);
  Serial1.begin(UART_BAUD, SERIAL_8N1, PIN_RX, PIN_TX);
  Serial2.begin(9600, SERIAL_8N1, 32, 33);
  LoRa.setPins(ss, rst, dio0);

  if (gSyncWord == 0 || gTxPower == 0 || gfreq == 0 || ginterval == 0)
  {
    gSyncWord = dgSyncWord;
    gTxPower = dgTxPower;
    gfreq = dgfreq;
    gspreadingFactor = dgspreadingFactor;
    gsignalBandwidth = dgsignalBandwidth;
    ginterval = dginterval;
    Serial.println("No value use default");
  }
  while (!LoRa.begin(gfreq))
  {
    Serial.println(".");
    delay(500);
  }

  LoRa.setTxPower(gTxPower);
  LoRa.setSyncWord(gSyncWord);
  LoRa.setSpreadingFactor(gspreadingFactor);
  LoRa.setSignalBandwidth(gsignalBandwidth);
  LoRa.enableCrc();

  // Show LoRa module configuraion
  Serial.print("\nSyncWord: ");
  Serial.println(gSyncWord, HEX);
  Serial.print("TxPower: ");
  Serial.println(gTxPower);
  Serial.print("Frequency: ");
  Serial.println(gfreq);
  Serial.print("Interval: ");
  Serial.println(ginterval);
  Serial.print("SpreadingFactor: ");
  Serial.println(gspreadingFactor);
  Serial.print("SignalBandwidth: ");
  Serial.println(gsignalBandwidth);

  Serial.println("\nLoRa Initialized!\n");

  modemPowerOn();
  delay(500);
  GPSavg(0);
  connect2LTE();
  parseJsonConfig(fetchJsonConfig());
  SerialMon.println("\n----------   End of Setup   ----------\n");
  SerialMon.println("\nWaiting for Data\n");
}

void loop()
{
  int packetSize = LoRa.parsePacket();
  if (packetSize)
  {
    Serial.print("Received packet '");

    char LoRaData[255];
    int dataIndex = 0;

    while (LoRa.available())
    {
      char receivedChar = LoRa.read();
      Serial.print(receivedChar);

      LoRaData[dataIndex] = receivedChar;
      dataIndex++;

      if (dataIndex >= sizeof(LoRaData) - 1)
      {
        LoRaData[dataIndex] = '\0';
        break;
      }

      if (dataIndex == 1 && receivedChar != '{')
      {
        dataIndex = 0;
        break;
      }
    }

    Serial.print("' with RSSI ");
    Serial.println(LoRa.packetRssi());

    if (dataIndex > 0)
    {
      LoRaData[dataIndex] = '\0';
      delay(5000);

      String jsonOutput = createJsonString(eSyncWord, eTxPower, efreq, interval);

      Serial.println("Switching to sending state...");
      Serial.print("Packet send: ");
      Serial.println(jsonOutput);
      LoRa.setSyncWord(0XF2);
      LoRa.beginPacket();
      LoRa.print(jsonOutput);
      LoRa.endPacket();
      Serial.println("Packet sent.");

      processJsonInput(LoRaData);
      Serial.print(latText);
      Serial.print(lonText);
      float latValue = latText.toFloat();
      float lonValue = lonText.toFloat();

      if (latValue > 13 && lonValue > 100)
      {
        sendHttpRequest();
      }
      else
      {
        Serial.println("No valid GPS info, performing other actions...");
        readcellinfo();
        sendLocationRequest();
        sendHttpRequest();
        // esp_restart();
      }
      unsigned long endTime = millis();
      unsigned long duration = endTime - startTime;
      float durationSeconds = duration / 1000.0;

      sleep(durationSeconds);
    }
    delay(100);
  }

  connect2LTE();
  request();
}
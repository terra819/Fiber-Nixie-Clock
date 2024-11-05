#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>  // https://github.com/tzapu/WiFiManager
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <TimeLib.h>
#include <Adafruit_NeoPixel.h>
#include <iostream>
#include <vector>
#include <cstdlib>
#include <ctime>

const String CLOCKNAME = "Fiber Nixie Clock";
#define LED_PIN 2
#define LED_COUNT 325
Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

static const char ntpServerName[] = "us.pool.ntp.org";

// Web client configurable vars

// Timezone:
int timeZone = -5;

// Hours Display:
int hoursDisplay = 12;

// Time Format:
String timeFormat = "hhmm";

// Display Color:
String displayColor = "red";
uint32_t color = strip.Color(255, 0, 0);

int brightness = 50;
int maxBrightness = 250;

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, ntpServerName, 0, 50000);
time_t getNtpTime();
void digitalClockDisplay(int forceUpdate);
void printDigits(int digits);

// Set web server port number to 80
WiFiServer server(80);

// Variable to store the HTTP request
String header;

// LED settings
int ledCountPerDigit = 81;
int ledCountPerSeparator = 1;

// mapping for fibers in drilled holes to lights in each pcb
std::vector<std::vector<int>> digitLeds = {
  { 78, 77, 73, 70, 58, 57, 56, 55, 37, 36, 20, 19, 16, 15, 3, 7, 13, 12, 24, 25, 26, 27, 47, 48, 63, 64, 67, 79 },
  { 72, 71, 58, 57, 56, 55, 37, 36, 20, 19, 18, 17 },
  { 67, 79, 78, 77, 73, 70, 58, 57, 54, 53, 42, 43, 30, 28, 25, 24, 11, 10, 9, 8, 7, 3, 2, 1 },
  { 67, 79, 78, 77, 73, 70, 58, 57, 54, 53, 42, 43, 34, 35, 20, 19, 16, 15, 3, 7, 13, 12 },
  { 76, 69, 68, 62, 61, 60, 59, 50, 45, 44, 43, 42, 41, 40, 38, 72, 71, 58, 57, 56, 55, 37, 36, 20, 19, 18, 17 },
  { 74, 75, 77, 78, 80, 81, 66, 65, 64, 63, 48, 47, 46, 45, 44, 43, 42, 34, 35, 20, 19, 16, 15, 3, 7, 13, 12 },
  { 76, 69, 68, 62, 61, 60, 59, 50, 45, 29, 28, 25, 24, 12, 13, 7, 3, 15, 16, 19, 20, 35, 34, 42, 43, 30 },
  { 81, 80, 78, 77, 75, 74, 72, 71, 58, 57, 54, 52, 40, 39, 33, 32, 31, 23, 22, 21, 14 },
  { 78, 77, 73, 70, 58, 57, 54, 53, 42, 43, 51, 49, 63, 64, 67, 79, 34, 35, 20, 19, 16, 15, 3, 7, 13, 12, 24, 25, 28, 30 },
  { 78, 77, 73, 70, 58, 57, 54, 53, 42, 43, 51, 49, 63, 64, 67, 79, 52, 40, 39, 33, 32, 31, 23, 22, 21, 14 }
};

String timeStr;

void setup() {
  Serial.begin(115200);

  // WifiManager
  WiFiManager wifiManager;
  wifiManager.autoConnect("AutoConnectAP");
  Serial.println("Connected.");

  // Wifi connected, get time
  server.begin();
  timeClient.begin();
  setSyncProvider(getNtpTime);
  setSyncInterval(60000);

  // Startup lights
  strip.begin();
  strip.show();
  strip.setBrightness(brightness);  // Set BRIGHTNESS to about 1/5 (max = 255)
}

time_t prevDisplay = 0;  // when the digital clock was displayed

void loop() {
  if (timeStatus() != timeNotSet) {
    if (now() != prevDisplay) {  //update the display only if time has changed
      prevDisplay = now();
      digitalClockDisplay(0);
    }
  }

  WiFiClient client = server.available();  // Listen for incoming clients

  if (client) {
    String currentLine = "";
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        Serial.write(c);
        header += c;
        if (c == '\n') {
          if (currentLine.length() == 0) {
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println("Connection: close");
            client.println();

            Serial.print("header: ");
            Serial.println(header);

            // sets the timezone
            int timezoneIndex = header.indexOf("GET /timezone/");
            if (timezoneIndex >= 0) {
              int timezoneSuffixIndex = header.indexOf(" HTTP/1.1");
              timeZone = header.substring(14, timezoneSuffixIndex).toInt();
              Serial.print("timeZone: ");
              Serial.println(timeZone);
            }

            // sets the hoursDisplay
            if (header.indexOf("GET /hours/12") >= 0) {
              hoursDisplay = 12;
            }
            if (header.indexOf("GET /hours/24") >= 0) {
              hoursDisplay = 24;
            }

            Serial.print("hoursDisplay: ");
            Serial.println(hoursDisplay);

            // sets the timeFormat
            if (header.indexOf("GET /format/hhmm") >= 0) {
              timeFormat = "hhmm";
            }
            if (header.indexOf("GET /format/hmm") >= 0) {
              timeFormat = "hmm";
            }

            Serial.print("timeFormat: ");
            Serial.println(timeFormat);

            // sets the color
            if (header.indexOf("GET /color/red") >= 0) {
              displayColor = "red";
              color = strip.Color(255, 0, 0);
            }
            if (header.indexOf("GET /color/orange") >= 0) {
              displayColor = "orange";
              color = strip.Color(250, 136, 6);
            }
            if (header.indexOf("GET /color/yellow") >= 0) {
              displayColor = "yellow";
              color = strip.Color(255, 255, 0);
            }
            if (header.indexOf("GET /color/green") >= 0) {
              displayColor = "green";
              color = strip.Color(0, 146, 0);
            }
            if (header.indexOf("GET /color/blue") >= 0) {
              displayColor = "blue";
              color = strip.Color(38, 112, 221);
            }
            if (header.indexOf("GET /color/purple") >= 0) {
              displayColor = "purple";
              color = strip.Color(111, 1, 133);
            }
            if (header.indexOf("GET /color/pink") >= 0) {
              displayColor = "pink";
              color = strip.Color(248, 30, 219);
            }
            if (header.indexOf("GET /color/white") >= 0) {
              displayColor = "white";
              color = strip.Color(255, 255, 255);
            }

            // sets the brightness
            if (header.indexOf("GET /brightness/up") >= 0) {
              if (brightness < maxBrightness) {
                brightness += 10;
                strip.setBrightness(brightness);
              }
            }
            if (header.indexOf("GET /brightness/down") >= 0) {
              if (brightness > 10) {
                brightness -= 10;
                strip.setBrightness(brightness);
              }
            }

            Serial.print("color: ");
            Serial.println(displayColor);

            // Refresh Clock with new settings
            digitalClockDisplay(1);

            // Display the HTML web page for configuration
            client.println("<!doctype html>");
            client.println("<html lang=\"en\">");
            client.println("<head>");
            client.println("<meta charset=\"utf-8\">");
            client.println("<title>" + CLOCKNAME + " Options</title>");
            client.println("<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
            client.println("<link rel=\"icon\" href=\"data:,\">");
            client.println("<style>");
            client.println("html {");
            client.println("font-family: Helvetica;");
            client.println("display: inline-block;");
            client.println("margin: 0px auto;");
            client.println("text-align: center;");
            client.println("}");

            client.println(".button {");
            client.println("background-color: #195B6A;");
            client.println("border: none;");
            client.println("color: white;");
            client.println("padding: 8px;");
            client.println("text-decoration: none;");
            client.println("font-size: 12px;");
            client.println("margin: 02px;");
            client.println("cursor: pointer;");
            client.println("width: 90%;");
            client.println("}");

            client.println(".selectedButton {");
            client.println("background-color: #287be9;");
            client.println("}");

            client.println(".color {");
            client.println("width: 10%;");
            client.println("height: 40px;");
            client.println("border: solid black 1px;");
            client.println("}");

            client.println(".selectedColor {");
            client.println("border: solid black 5px;");
            client.println("position: relative;");
            client.println("top: -4px;");
            client.println("}");

            client.println(".red {");
            client.println("background-color: rgb(255, 0, 0);");
            client.println("}");

            client.println(".orange {");
            client.println("background-color: rgb(250, 136, 6);");
            client.println("}");

            client.println(".yellow {");
            client.println("background-color: rgb(255, 255, 0);");
            client.println("color: black;");
            client.println("}");

            client.println(".green {");
            client.println("background-color: rgb(0, 146, 0);");
            client.println("}");

            client.println(".blue {");
            client.println("background-color: rgb(38, 112, 221);");
            client.println("}");

            client.println(".purple {");
            client.println("background-color: rgb(111, 1, 133);");
            client.println("}");

            client.println(".pink {");
            client.println("background-color: rgb(248, 30, 219);");
            client.println("}");

            client.println(".white {");
            client.println("background-color: rgb(255, 255, 255);");
            client.println("color: black;");
            client.println("}");
            client.println("</style>");
            client.println("<script>");
            client.println("function redirect(url) {");
            client.println("var base_url = window.location.origin;");
            client.println("window.document.location.href = base_url + url;");
            client.println("}");
            client.println("</script>");
            client.println("</head>");

            client.println("<body>");

            // Web Page Heading
            client.println("<h1>" + CLOCKNAME + "</h1>");

            // Display current Time
            client.println("<p>Current Time Formatted: <b> " + timeStr + "</b></p>");

            // Display Timezone options
            client.println("<b>Timezone</b>");
            client.println("<p>");
            client.println("<select name=\"timezone_offset\" id=\"timezone-offset\" class=\"button\" onChange=\"redirect(this.options[this.selectedIndex].value)\" title=\"Timezone Selection\">");
            client.println("<option value=\"/timezone/-12\"");
            if (timeZone == -12) {
              client.println("selected");
            }
            client.println(">(GMT -12:00) Eniwetok, Kwajalein</option>");
            client.println("<option value=\"/timezone/-11\"");
            if (timeZone == -11) {
              client.println("selected");
            }
            client.println(">(GMT -11:00) Midway Island, Samoa</option>");
            client.println("<option value=\"/timezone/-10\"");
            if (timeZone == -10) {
              client.println("selected");
            }
            client.println(">(GMT -10:00) Hawaii</option>");
            client.println("<option value=\"/timezone/-9\"");
            if (timeZone == -9) {
              client.println("selected");
            }
            client.println(">(GMT -9:00) Alaska</option>");
            client.println("<option value=\"/timezone/-8\"");
            if (timeZone == -8) {
              client.println("selected");
            }
            client.println(">(GMT -8:00) Pacific Time (US &amp; Canada)</option>");
            client.println("<option value=\"/timezone/-7\"");
            if (timeZone == -7) {
              client.println("selected");
            }
            client.println(">(GMT -7:00) Mountain Time (US &amp; Canada)</option>");
            client.println("<option value=\"/timezone/-6\"");
            if (timeZone == -6) {
              client.println("selected");
            }
            client.println(">(GMT -6:00) Central Time (US &amp; Canada), Mexico City</option>");
            client.println("<option value=\"/timezone/-5\"");
            if (timeZone == -5) {
              client.println("selected");
            }
            client.println(">(GMT -5:00) Eastern Time (US &amp; Canada), Bogota, Lima</option>");
            client.println("<option value=\"/timezone/-4\"");
            if (timeZone == -4) {
              client.println("selected");
            }
            client.println(">(GMT -4:00) Atlantic Time (Canada), Caracas, La Paz</option>");
            client.println("<option value=\"/timezone/-3\"");
            if (timeZone == -3) {
              client.println("selected");
            }
            client.println(">(GMT -3:00) Brazil, Buenos Aires, Georgetown</option>");
            client.println("<option value=\"/timezone/-2\"");
            if (timeZone == -2) {
              client.println("selected");
            }
            client.println(">(GMT -2:00) Mid-Atlantic</option>");
            client.println("<option value=\"/timezone/-1\"");
            if (timeZone == -1) {
              client.println("selected");
            }
            client.println(">(GMT -1:00) Azores, Cape Verde Islands</option>");
            client.println("<option value=\"/timezone/0\"");
            if (timeZone == -0) {
              client.println("selected");
            }
            client.println(">(GMT) Western Europe Time, London, Lisbon, Casablanca</option>");
            client.println("<option value=\"/timezone/1\"");
            if (timeZone == 1) {
              client.println("selected");
            }
            client.println(">(GMT +1:00) Brussels, Copenhagen, Madrid, Paris</option>");
            client.println("<option value=\"/timezone/2\"");
            if (timeZone == 2) {
              client.println("selected");
            }
            client.println(">(GMT +2:00) Kaliningrad, South Africa</option>");
            client.println("<option value=\"/timezone/3\"");
            if (timeZone == 3) {
              client.println("selected");
            }
            client.println(">(GMT +3:00) Baghdad, Riyadh, Moscow, St. Petersburg</option>");
            client.println("<option value=\"/timezone/4\"");
            if (timeZone == 4) {
              client.println("selected");
            }
            client.println(">(GMT +4:00) Abu Dhabi, Muscat, Baku, Tbilisi</option>");
            client.println("<option value=\"/timezone/5\"");
            if (timeZone == 5) {
              client.println("selected");
            }
            client.println(">(GMT +5:00) Ekaterinburg, Islamabad, Karachi, Tashkent</option>");
            client.println("<option value=\"/timezone/6\"");
            if (timeZone == 6) {
              client.println("selected");
            }
            client.println(">(GMT +6:00) Almaty, Dhaka, Colombo</option>");
            client.println("<option value=\"/timezone/7\"");
            if (timeZone == 7) {
              client.println("selected");
            }
            client.println(">(GMT +7:00) Bangkok, Hanoi, Jakarta</option>");
            client.println("<option value=\"/timezone/8\"");
            if (timeZone == 8) {
              client.println("selected");
            }
            client.println(">(GMT +8:00) Beijing, Perth, Singapore, Hong Kong</option>");
            client.println("<option value=\"/timezone/9\"");
            if (timeZone == 9) {
              client.println("selected");
            }
            client.println(">(GMT +9:00) Tokyo, Seoul, Osaka, Sapporo, Yakutsk</option>");
            client.println("<option value=\"/timezone/10\"");
            if (timeZone == 10) {
              client.println("selected");
            }
            client.println(">(GMT +10:00) Eastern Australia, Guam, Vladivostok</option>");
            client.println("<option value=\"/timezone/11\"");
            if (timeZone == 11) {
              client.println("selected");
            }
            client.println(">(GMT +11:00) Magadan, Solomon Islands, New Caledonia</option>");
            client.println("<option value=\"/timezone/12\"");
            if (timeZone == 12) {
              client.println("selected");
            }
            client.println(">(GMT +12:00) Auckland, Wellington, Fiji, Kamchatka</option>");
            client.println("<option value=\"/timezone/13\"");
            if (timeZone == 13) {
              client.println("selected");
            }
            client.println(">(GMT +13:00) Apia, Nukualofa</option>");
            client.println("<option value=\"/timezone/14\"");
            if (timeZone == 14) {
              client.println("selected");
            }
            client.println(">(GMT +14:00) Line Islands, Tokelau</option>");
            client.println("</select>");
            client.println("</p>");

            // Hours Display
            client.println("<b>Hours Display</b>");
            client.println("<p>");
            client.println("<a href=\"/hours/12\"><button type=\"submit\" title=\"Twelve Hour Display Button\" class=\"button");
            if (hoursDisplay == 12) {
              client.println("selectedButton");
            }
            client.println("\">12 Hours</button></a>");
            client.println("<a href=\"/hours/24\"><button type=\"submit\" title=\"Twenty-Four Hour Display Button\" class=\"button");
            if (hoursDisplay == 24) {
              client.println("selectedButton");
            }
            client.println("\">24 Hours</button></a>");
            client.println("</p>");

            // Time Format
            client.println("<b>Time Format</b>");
            client.println("<p>");
            client.println("<a href=\"/format/hhmm\"><button title=\"Leading Zero Hour Display Button\" class=\"button");
            if (timeFormat == "hhmm") {
              client.println("selectedButton");
            }
            client.println("\">HH:MM</button></a>");
            client.println("<a href=\"/format/hmm\"><button title=\"No Leading Zero Hour Display Button\" class=\"button");
            if (timeFormat == "hmm") {
              client.println("selectedButton");
            }
            client.println("\">H:MM</button></a>");
            client.println("</p>");

            // Display Color
            client.println("<b>Display Color</b>");
            client.println("<p>");
            client.println("<a href=\"/color/red\"><button type=\"submit\" title=\"Red Color Button\" class=\"red color");
            if (displayColor == "red") {
              client.println("selectedColor");
            }
            client.println("\"></button></a>");
            client.println("<a href=\"/color/orange\"><button type=\"submit\" title=\"Orange Color Button\" class=\"orange color");
            if (displayColor == "orange") {
              client.println("selectedColor");
            }
            client.println("\"></button></a>");
            client.println("<a href=\"/color/yellow\"><button type=\"submit\" title=\"Yellow Color Button\" class=\"yellow color");
            if (displayColor == "yellow") {
              client.println("selectedColor");
            }
            client.println("\"></button></a>");
            client.println("<a href=\"/color/green\"><button type=\"submit\" title=\"Green Color Button\" class=\"green color");
            if (displayColor == "green") {
              client.println("selectedColor");
            }
            client.println("\"></button></a>");
            client.println("<a href=\"/color/blue\"><button type=\"submit\" title=\"Blue Color Button\" class=\"blue color");
            if (displayColor == "blue") {
              client.println("selectedColor");
            }
            client.println("\"></button></a>");
            client.println("<a href=\"/color/purple\"><button type=\"submit\" title=\"Purple Color Button\" class=\"purple color");
            if (displayColor == "purple") {
              client.println("selectedColor");
            }
            client.println("\"></button></a>");
            client.println("<a href=\"/color/pink\"><button type=\"submit\" title=\"Pink Color Button\" class=\"pink color");
            if (displayColor == "pink") {
              client.println("selectedColor");
            }
            client.println("\"></button></a>");
            client.println("<a href=\"/color/white\"><button type=\"submit\" title=\"White Color Button\" class=\"white color");
            if (displayColor == "white") {
              client.println("selectedColor");
            }
            client.println("\"></button></a>");
            client.println("</p>");

            // Brightness
            client.println("<b>Brightness: </b>");
            client.println(brightness);
            if (brightness == maxBrightness) {
              client.println("(MAX)");
            }
            if (brightness == 10) {
              client.println("(MIN)");
            }

            client.println("<p>");
            client.println("<a href=\"/brightness/up\"><button type=\"submit\" title=\"Brightness Up Button\" class=\"button\">Up</button></a>");
            client.println("<a href=\"/brightness/down\"><button type=\"submit\" title=\"Brightness Down Button\" class=\"button\">Down</button></a>");
            client.println("</p>");

            client.println("</body>");
            client.println("</html>");
            client.println();
            break;
          } else {
            currentLine = "";
          }
        } else if (c != '\r') {
          currentLine += c;
        }
      }
    }

    // Clear the header variable
    header = "";
    // Close the connection
    client.stop();
    Serial.println("Client disconnected.");
    Serial.println("");
  }
}

void digitalClockDisplay(int forceUpdate) {
  // Variables to save date and time
  String formattedTime;
  int hourInt;
  int minInt;

  // The formattedTime comes with the following format:
  // 16:04:14
  // We need to extract hour and minute
  formattedTime = timeClient.getFormattedTime();

  // Extract hour
  int splitT = formattedTime.indexOf(":");
  hourInt = formattedTime.substring(0, splitT).toInt();
  // Extract minute
  minInt = formattedTime.substring(splitT + 1, formattedTime.length() - 1).toInt();

  // adjusted timzone
  hourInt += timeZone;
  if (hourInt < 1) {
    hourInt = 24 + hourInt;
  }

  if (hoursDisplay == 12) {
    // adjusted non military time
    if (hourInt > 12) {
      hourInt -= 12;
    }
  }

  String tempTimeStr;
  if (hourInt < 10) {
    tempTimeStr += '0';
  }
  tempTimeStr += hourInt;
  if (minInt < 10) {
    tempTimeStr += '0';
  }
  tempTimeStr += minInt;
  tempTimeStr = formatTimeStr(tempTimeStr);
  if (tempTimeStr != timeStr || forceUpdate > 0) {
    timeStr = tempTimeStr;

    // update display
    // digits right to left
    int len = timeStr.length();
    strip.clear();
    int digit1 = timeStr.substring(len - 1, len).toInt();
    lightDigit(digit1, 0, color);
    int digit2 = timeStr.substring(len - 2, len - 1).toInt();
    lightDigit(digit2, ledCountPerDigit, color);
    int digit3 = timeStr.substring(len - 4, len - 3).toInt();
    lightDigit(digit3, (ledCountPerDigit * 2) + ledCountPerSeparator, color);
    if (len == 5) {
      int digit4 = timeStr.substring(len - 5, len - 4).toInt();
      lightDigit(digit4, (ledCountPerDigit * 3) + ledCountPerSeparator, color);
    }
    strip.setPixelColor(((ledCountPerDigit * 2) + ledCountPerSeparator) - 1, color);
    strip.show();
  }
}

void lightDigit(int num, int offset, uint32_t color) {
  std::vector<int> leds = digitLeds[num];
  for (size_t index = 0; index < leds.size(); index++) {
    int pixelToLight = (leds[index] - 1) + offset;
    strip.setPixelColor(pixelToLight, color);
  }
}

time_t getNtpTime() {
  Serial.println("getNtpTime");
  timeClient.update();
  return timeClient.getEpochTime();
}

String formatTimeStr(String time) {
  String formattedTime = time.substring(0, 1) + time.substring(1, 2) + ':' + time.substring(2, 3) + time.substring(3, 4);
  if (formattedTime.substring(0, 1).toInt() == 0 && timeFormat == "hmm") {
    formattedTime = formattedTime.substring(1, 5);
  }
  return formattedTime;
}
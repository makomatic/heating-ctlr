#include <SPI.h>
#include <Ethernet.h>
#include <EthernetUdp.h>
#include <RCSwitch.h>
#include <OneWire.h>
#include <Wire.h>
#include <dht11.h>
#include <DallasTemperature.h>

// Variables
byte mac[] = { 0x54, 0x55, 0x58, 0x10, 0x00, 0x24 };  // 84.85.88.16.0.36
byte ip[]  = { 192, 168, 178, 222 };
byte gateway[] = { 192, 168, 178, 1 };
byte subnet[]  = { 255, 255, 255, 0 };
byte i;
byte present = 0;
byte data[12];
byte addr[8];
String readString = String(100);      // string for fetching data from address
float temp_workingroom = 0.0;
float dht11temp = 0.0;
float humidity = 0.0;
float dewpoint = 0.0;
unsigned int localPort = 8888;
IPAddress timeServer(64, 236, 96, 53);
const int NTP_PACKET_SIZE= 48;
byte packetBuffer[ NTP_PACKET_SIZE];
String title = "";
float value = 0.0;
String unit = "";
float temp;
float temp2;  
float temp3;
float temp4;
float temp5;
int hours;
int minutes;
int seconds;
boolean relayState = LOW;
const int relayPin = 15;
const int reedPin(50);
int reedValue;
int impulses = 0;
int lastState = 1;


// Initalize Classes
EthernetServer server(80);
RCSwitch mySwitch = RCSwitch();
OneWire ds(18);
DallasTemperature sensors(&ds);
dht11 DHT11;
EthernetUDP Udp;

// Setup
void setup(){
  pinMode(relayPin, OUTPUT);
  pinMode(reedPin, INPUT_PULLUP);
  mySwitch.enableTransmit(17);
  Ethernet.begin(mac, ip, gateway);
  server.begin();
  Wire.begin();
  sensors.begin();
  delay(1000);               // maybe 750ms is enough, maybe not
  Serial.begin(9600);
  Serial.println("Server started");
  
  // time stuff - TODO: Fix the upstream Time lib and use it (will provide standalone timesafety then)!
  unsigned long epoch = getTime();
  hours = ((epoch  % 86400L) / 3600); // (86400 equals secs per day) 
  minutes = ((epoch  % 3600) / 60); // (3600 equals secs per minute)
  seconds = (epoch %60); // second
}

// Main loop
void loop()
{
  // read reed switch - TODO: Use Interrupts!
  reedValue = digitalRead(reedPin);
  if (reedValue != lastState) {
    if (reedValue == 0) {
      impulses += 1;
    }
  }
  lastState = reedValue;
  // set Relay
  digitalWrite(relayPin, checkTherm(temp5, temp, temp2));
  

  // DS18B20 stuff
  sensors.requestTemperatures();
  temp = sensors.getTempCByIndex(0);                 
  temp2 = sensors.getTempCByIndex(1);
  temp3 = sensors.getTempCByIndex(2);
  temp4 = sensors.getTempCByIndex(3);
  temp5 = sensors.getTempCByIndex(4);

  // dht11 stuff
  int chk = DHT11.read(16);
  if (chk == DHTLIB_OK) {
    humidity = DHT11.humidity;
    dht11temp = DHT11.temperature;
    dewpoint = dewPoint(dht11temp, humidity);
  }
  
  // Create a client connection - TODO: Use a simple UDP Server, which respons with a json object instead of parsing HTML
  EthernetClient client = server.available();
  if (client) {
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();

        //read char by char HTTP request
        if (readString.length() < 100) {

          //store characters to string
          readString = readString + c;
          // very simple but it works...
        }

        Serial.print(c);  //output chars to serial port

        if (c == '\n') {  //if HTTP request has ended
          //--------------------------HTML------------------------
          client.println("HTTP/1.1 200 OK");
          client.println("Content-Type: text/html");
          client.println();
          client.print("<html><head>");
          client.print("<title>MyControl</title>");
          client.println("</head>");
          client.print("<body bgcolor='#444444'>");
          //---Überschrift---
          client.println("<br><hr />");
          client.println("<h1><div align='center'><font color='#2076CD'>Heizungssteuerung</font color></div></h1>");
          client.println("<hr /><br>");
         
          client.print("Server started at: ");
          client.print(hours);
          client.print(":");
          client.print(minutes);
          client.print(":");
          client.print(seconds);
          client.print(" UTC");
          client.println("<br>");
          
          //---Überschrift---
          
          client.println("<table border='1' width='500' cellpadding='5'>");
          writeTable(client, "Aussentemperatur", temp5 , "C");
          writeTable(client, "Vorlauftemperatur 1", temp, "C");
          writeTable(client, "Vorlauftemperatur 2", temp2, "C");
          writeTable(client, "Solltemperatur Vorlauf", forerun(temp5), "C");
          writeTable(client, "Flurtempeartur", temp4, "C" );
          writeTable(client, "Kellertemperatur", temp3, "C");
          writeTable(client, "Kellerfeuchtigkeit", humidity, "%");
          writeTable(client, "Impulse", impulses, "");
          client.println("<tr bgColor='#222222'>");
          client.print("<td bgcolor='#222222'><font face='Verdana' color='#CFCFCF' size='2'>");
          client.print("Absenkung");
          client.println("<br></font></td>");
          client.print("<td align='center' bgcolor='#222222'>");
          if (relayState) {
            client.print("AN");
          }
          else {
            client.print("AUS");
          }
          client.print("</td>");
          client.println("<td align='center' bgcolor='#222222'></td>");
          client.println("<td align='center' bgcolor='#222222'></td>");
          client.println("</tr>");

          client.println("</tr>");
          client.println("</table>");
          client.println("</body></html>");
          //clearing string for next read
          readString="";
          //stopping client
          client.stop();
        }
      }
    }
  }
}

// Functions
double dewPoint(double celsius, double humidity) {
  double a = 17.271;
  double b = 237.7;
  double temp = (a * celsius) / (b + celsius) + log(humidity/100);
  double Td = (b * temp) / (a - temp);
  return Td;
}

unsigned long sendNTPpacket(IPAddress& address) {
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  packetBuffer[0] = 0b11100011;
  packetBuffer[1] = 0;
  packetBuffer[2] = 6;
  packetBuffer[3] = 0xEC;
  packetBuffer[12]  = 49;
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;                  
  Udp.beginPacket(address, 123);
  Udp.write(packetBuffer,NTP_PACKET_SIZE);
  Udp.endPacket();
}

void writeTable(EthernetClient& client, String title, float value, String unit) {
  client.println("<tr bgColor='#222222'>");
  client.print("<td bgcolor='#222222'><font face='Verdana' color='#CFCFCF' size='2'>");
  client.print(title);
  client.println("<br></font></td>");
  client.print("<td align='center' bgcolor='#222222'>");
  client.print(value);
  client.print(" ");
  client.print(unit);
  client.print("</td>");
  client.println("<td align='center' bgcolor='#222222'></td>");
  client.println("<td align='center' bgcolor='#222222'></td>");
  client.println("</tr>");
}

float forerun(float temp5) {
  float forerun_thermo = -temp5+45;
  return forerun_thermo;
}

boolean checkTherm(float temp5, float temp, float temp2) {
  if (temp2 > forerun(temp5)) {
    relayState = HIGH;
  }
  if (temp < forerun(temp5)) {
    relayState = LOW;
  }
  return relayState;
}

unsigned long getTime() {
  Udp.begin(localPort);
  Serial.println("Sending NTP packet");
  sendNTPpacket(timeServer);
  delay(1000);
  if (Udp.parsePacket()){  
    Udp.read(packetBuffer,NTP_PACKET_SIZE);
    unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
    unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);  
    unsigned long secsSince1900 = highWord << 16 | lowWord;    
    // now convert NTP time into everyday time:
    // Unix time starts on Jan 1 1970. In seconds, that's 2208988800:
    const unsigned long seventyYears = 2208988800UL;     
    unsigned long epoch = secsSince1900 - seventyYears; 
    return epoch;
  }
  else {
    Serial.println("No packet received");
  }
}

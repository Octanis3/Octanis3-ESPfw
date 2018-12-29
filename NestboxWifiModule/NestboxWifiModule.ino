//
// Echo server the Arduino M0 Pro (based on Atmel SAMD21)
//#define DEBUG_PRINTING
//#define DEBUG_ESP_HTTP_SERVER
#define DEBUG_SERIAL  Serial1
#define APP_SERIAL    Serial
//#define USE_SERIAL2   1 //use this if using an external ESP module (NodeMCU)
#define DEBUG_OUTPUT DEBUG_SERIAL
#define RESPONSE_DELAY  10 //ms

#include <stdarg.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>


const byte DNS_PORT = 53;
IPAddress apIP(192, 168, 1, 1);

DNSServer dnsServer;
ESP8266WebServer server ( 80 );


// Debug printing callback
void min_debug_print(const char *fmt, ...)
{
    char buf[256];
    va_list args;
    va_start (args, fmt );
    vsnprintf(buf, 256, fmt, args);
    va_end (args);
    DEBUG_SERIAL.print(buf);
}

#include "min.h"
#include "min.c"

struct min_context min_ctx;

////////////////////////////////// CALLBACKS ///////////////////////////////////

uint16_t min_tx_space(uint8_t port)
{
  uint16_t n = APP_SERIAL.availableForWrite();

  // This is a lie but we will handle the consequences
  return 512U;
}

// Send a character on the designated port.
void min_tx_byte(uint8_t port, uint8_t byte)
{
  // If there's no space, spin waiting for space
  uint32_t n;
  do {
    n = APP_SERIAL.write(&byte, 1U);
    //n = DEBUG_SERIAL.write("Sending byte" + &byte +"\n");
  }
  while(n == 0);
}

void write_variable(char code, unsigned int len, uint32_t value){
  //For sending time
  uint8_t tx_buf [32];
  tx_buf[0] = code | 0x80;
  for(int i = 0; i < len; i++){
    tx_buf[len-i] = value & 0xFF;
    value = value >> 8;
  }
   min_send_frame(&min_ctx, 0x33, tx_buf, 0x05);
}

void request_variable(char code){
  //For sending time
  uint8_t tx_buf [32];
  tx_buf[0] = code;
  min_send_frame(&min_ctx, 0x33, tx_buf, 1);
}

uint32_t read_variable(char* code){
  uint8_t rx_buf [32];
  size_t buf_len;
  uint32_t result = 0;
  if(APP_SERIAL.available() > 0) {
    buf_len = APP_SERIAL.readBytes(rx_buf, 32U);
  }
  else {
    buf_len = 0;
  }
 min_poll(&min_ctx, rx_buf, (uint8_t)buf_len);

 //DEBUG_SERIAL.printf("Recieved message %c \n", min_ctx.rx_frame_payload_buf[0]);
 *code = min_ctx.rx_frame_payload_buf[0];

 for(int i=1; i<min_ctx.rx_frame_payload_bytes; i++)
 {
   //DEBUG_SERIAL.printf("raw: %d \n", min_ctx.rx_frame_payload_buf[i]);
   result = (result<<8) + min_ctx.rx_frame_payload_buf[i];
   //DEBUG_SERIAL.printf("Parsed value: %d \n", result);

 }
 
   //DEBUG_SERIAL.printf("\n");
   //DEBUG_SERIAL.printf("Parsed value: %d \n", result);

   return result;
}


// Tell MIN the current time in milliseconds.
uint32_t min_time_ms(void)
{
  return millis();
}

void min_application_handler(uint8_t min_id, uint8_t *min_payload, uint8_t len_payload, uint8_t port)
{
  // In this simple example application we just echo the frame back when we get one
  bool result = min_queue_frame(&min_ctx, min_id, min_payload, len_payload);
  if(!result) {
    DEBUG_SERIAL.println("Queue failed");
  }
}

//The functions for the DNS server

void timeUpdate(){
  uint32_t new_time = 0;
  if(server.args())
  {
    if(server.argName(0) == "set")
    {
      String argval = server.arg(0);
      new_time = argval.toInt();
     // DEBUG_SERIAL.println("new timestamp string: "+argval);
     // DEBUG_SERIAL.printf("new timestamp to set: %d\n",new_time);
    }
  }

  if(new_time > 0)
  {
    write_variable('Z', 4, new_time);
  }
  else
  {
    request_variable('Z');
  }
  char code;
  delay(RESPONSE_DELAY); // wait for MSP to process and reply.
  new_time = read_variable(&code); //read back time from MSP as confirmation
  String t = String(new_time, DEC);
  server.send(200, "text/plain", code+t);
}


void rtcAlarmUpdate(){
  uint32_t rtca = 0;
  if(server.args())
  {
    if(server.argName(0) == "ph")
    {
      String argval = server.arg(0);
      uint32_t ph = argval.toInt();
      rtca = rtca + ph<<24;
    }
    if(server.argName(1) == "pm")
    {
      String argval = server.arg(1);
      uint32_t pm = argval.toInt();
      rtca = rtca + pm<<16;
    }
    if(server.argName(2) == "rh")
    {
      String argval = server.arg(2);
      uint32_t rh = argval.toInt();
      rtca = rtca + rh<<8;
    }
    if(server.argName(3) == "rm")
    {
      String argval = server.arg(3);
      uint32_t rm = argval.toInt();
      rtca = rtca + rm;
    }
  }

  if(rtca > 0)
  {
    write_variable('S', 4, rtca);
  }
  else
  {
    request_variable('S');
  }
  char code;
  delay(RESPONSE_DELAY); // wait for MSP to process and reply.
  rtca = read_variable(&code); //read back time from MSP as confirmation
  String t = String(rtca, DEC);
  server.send(200, "text/plain", code+t);
}


inline void serverGetVariable(char code){
  uint32_t value = 0;
  request_variable(code);
  value = read_variable(&code);
  String t = String(value, DEC);
  server.send(200, "text/plain", code+t);
}

void batteryUpdate(){
  serverGetVariable('B');
}

void weightUpdate(){
  serverGetVariable('W');
}

void offsetUpdate(){
  serverGetVariable('O');
}

void rfidUpdate(){
  serverGetVariable('R');
}

void triggerTare(){
  serverGetVariable('t');
}

void triggerSDflush(){
   serverGetVariable('F');
}

void triggerLoadCellOn(){
   serverGetVariable('L');
}

void triggerLoadCellOff(){
   serverGetVariable('l');
}

void heartbeatUpdate(){
  uint32_t value = 0;
  char code;
  request_variable('H');
  delay(RESPONSE_DELAY);
  value = read_variable(&code);
   //  DEBUG_SERIAL.printf("Returned: %d \n", value);

  String t = String(value, DEC);

   //  DEBUG_SERIAL.print(t);

  server.send(200, "text/plain", code+t);
}

void HelloWorld(){
  String t = "Hello World";
  server.send(200, "text/plain", t);
}

void handleNotFound() {
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += ( server.method() == HTTP_GET ) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";

  for ( uint8_t i = 0; i < server.args(); i++ ) {
    message += " " + server.argName ( i ) + ": " + server.arg ( i ) + "\n";
  }

  server.send ( 404, "text/plain", message );
}



void handleRoot() {
  server.send(200, "text/html", "Hello world");
}

#define MSG_LENGTH    4+4+1 
#define UID_POS       0
#define TIMESTAMP_POS 4
#define INOUT_POS     8
#define TABLE_LENGTH 10
unsigned int buffer_pos = 0;
unsigned long uid_mem[TABLE_LENGTH];
unsigned long time_mem[TABLE_LENGTH];
short in_out_mem[TABLE_LENGTH];


void setup() {
  APP_SERIAL.begin(9600);
  DEBUG_SERIAL.begin(9600);

  #ifdef USE_SERIAL2
    APP_SERIAL.swap();
  #endif
  
  while(!APP_SERIAL) {
    ; // Wait for serial port
  }
  while(!DEBUG_SERIAL) {
    ; // Wait for serial port
  }
  min_init_context(&min_ctx, 0);


  //Begin the wifi and DNS server
  WiFi.mode(WIFI_AP);
 
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
  // Do a little work to get a unique-ish name. Append the
  // last two bytes of the MAC (HEX'd) to "Thing-":

  uint8_t mac[WL_MAC_ADDR_LENGTH];
  WiFi.softAPmacAddress(mac);
  String macID = String(mac[WL_MAC_ADDR_LENGTH - 2], HEX) +
                 String(mac[WL_MAC_ADDR_LENGTH - 1], HEX);


  String AP_NameString = "Nestbox_" + macID;

  char AP_NameChar[AP_NameString.length() + 1];
  memset(AP_NameChar, 0, AP_NameString.length() + 1);

  for (int i=0; i<AP_NameString.length(); i++)
    AP_NameChar[i] = AP_NameString.charAt(i);

  WiFi.softAP(AP_NameChar);

  dnsServer.setTTL(300);
  dnsServer.setErrorReplyCode(DNSReplyCode::ServerFailure);

  // start DNS server for a specific domain name
  dnsServer.start(DNS_PORT, "www.nestbox.local", apIP);

  server.on ("/time", timeUpdate);
  server.on ("/S", rtcAlarmUpdate);
  server.on ("/battery", batteryUpdate);
  server.on ("/heartbeat", heartbeatUpdate);
  server.on ("/weight", weightUpdate);
  server.on ("/offset", offsetUpdate);
  server.on ("/rfid", rfidUpdate);
  server.on ("/t", triggerTare);
  server.on ("/F", triggerSDflush);
  server.on ("/L", triggerLoadCellOn);
  server.on ("/l", triggerLoadCellOff);

  server.on ("/Hello_World", HelloWorld);

  server.on("/", handleRoot);
//  server.on("/time", HTTP_POST, handleUpdateTable); // OLD!! 
  server.onNotFound (handleNotFound);
  server.begin();
  DEBUG_SERIAL.println ("HTTP server started");
}

void loop() {
  char buf[32];
  size_t buf_len;

  dnsServer.processNextRequest();
  server.handleClient();
  /*
  write_variable();
  DEBUG_SERIAL.print("Finished sending frame\n");
  read_variable();*/
  //APP_SERIAL.printf("%c%i\n", message, zeit);
}

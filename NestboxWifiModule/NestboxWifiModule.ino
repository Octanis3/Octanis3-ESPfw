//
// Echo server the Arduino M0 Pro (based on Atmel SAMD21)


#include <stdarg.h>
#define DEBUG_PRINTING
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>

#define DEBUG_SERIAL  Serial1 //todo check
#define APP_SERIAL  Serial

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
    Serial.print(buf);
}

#include "min.h"
#include "min.c"

struct min_context min_ctx;

////////////////////////////////// CALLBACKS ///////////////////////////////////

uint16_t min_tx_space(uint8_t port)
{
  uint16_t n = Serial.availableForWrite();

  // This is a lie but we will handle the consequences
  return 512U;
}

// Send a character on the designated port.
void min_tx_byte(uint8_t port, uint8_t byte)
{
  // If there's no space, spin waiting for space
  uint32_t n;
  do {
    n = Serial.write(&byte, 1U);
    //n = Serial1.write("Sending byte" + &byte +"\n");
  }
  while(n == 0);
}

void write_variable(char code, unsigned int len, uint32_t value){
   //delay(1000);
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
   //delay(1000);
  //For sending time
  uint8_t tx_buf [32];
  tx_buf[0] = code;
  min_send_frame(&min_ctx, 0x33, tx_buf, 1);
}

uint32_t read_variable(char* code){
  uint8_t rx_buf [32];
  size_t buf_len;
  uint32_t result = 0;
  if(Serial.available() > 0) {
    buf_len = Serial.readBytes(rx_buf, 32U);
  }
  else {
    buf_len = 0;
  }
 min_poll(&min_ctx, rx_buf, (uint8_t)buf_len);

 Serial1.printf("Recieved message %c \n", min_ctx.rx_frame_payload_buf[0]);
 *code = min_ctx.rx_frame_payload_buf[0];

 for(int i=1; i<min_ctx.rx_frame_payload_bytes; i++)
 {
   Serial1.printf("raw: %d \n", min_ctx.rx_frame_payload_buf[i]);
   result = (result<<8) + min_ctx.rx_frame_payload_buf[i];
   Serial1.printf("Parsed value: %d \n", result);

 }
 
   Serial1.printf("\n");
   Serial1.printf("Parsed value: %d \n", result);

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
    Serial.println("Queue failed");
  }
}

//The functions for the DNS server

void timeUpdate(){
  static uint32_t value = 1234;

  value = value + 100;      
  char code;
  write_variable('Z', 4, value);
  delay(100);
  value = read_variable(&code);
  String t = String(value, DEC);
  server.send(200, "text/plain", code+t);
}

void batteryUpdate(){
  uint32_t value = 0;
  char code;
  request_variable('B');
  delay(100);
  value = read_variable(&code);
  String t = String(value, DEC);
  server.send(200, "text/plain", code+t);
}

void heartbeatUpdate(){
  uint32_t value = 0;
  char code;
  request_variable('H');
  delay(100);
  value = read_variable(&code);
     Serial1.printf("Returned: %d \n", value);

  String t = String(value, DEC);

     Serial1.print(t);

  server.send(200, "text/plain", code+t);
}

void HelloWorld(){
  String t = "Hello World";
  server.send(200, "text/plain", t);
}

void handleUpdateTable(){
  if( ! server.hasArg("cur-time") || server.arg("cur-time")) { // If the POST request doesn't have username and password data
    server.send(400, "text/plain", "400: Invalid Request");         // The request is invalid, so send HTTP status 400
    return;
  } else {                                                                              // Username and password don't match
    server.send(200, "text/html", "<h1>New time, " + server.arg("cur-time") + "!</h1>");
  }
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
  Serial.begin(9600);
  Serial1.begin(9600);
  Serial.swap();
  while(!Serial) {
    ; // Wait for serial port
  }
  while(!Serial1) {
    ; // Wait for serial port
  }
  min_init_context(&min_ctx, 0);


  //Begin the wifi and DNS server
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
  WiFi.softAP("Nestbox123");
  
  dnsServer.setTTL(300);
  dnsServer.setErrorReplyCode(DNSReplyCode::ServerFailure);

  // start DNS server for a specific domain name
  dnsServer.start(DNS_PORT, "www.nestbox.local", apIP);

  server.on ("/time", timeUpdate);
  server.on ("/battery", batteryUpdate);
  server.on ("/heartbeat", heartbeatUpdate);

  server.on ("/Hello_World", HelloWorld);

  server.on("/", handleRoot);
  server.on("/time", HTTP_POST, handleUpdateTable);
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
  Serial1.print("Finished sending frame\n");
  read_variable();*/
  //Serial.printf("%c%i\n", message, zeit);
}

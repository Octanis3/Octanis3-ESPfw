/*
 * Copyright (c) 2015, Majenko Technologies
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice, this
 *   list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright notice, this
 *   list of conditions and the following disclaimer in the documentation and/or
 *   other materials provided with the distribution.
 *
 * * Neither the name of Majenko Technologies nor the names of its
 *   contributors may be used to endorse or promote products derived from
 *   this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>

#define DEBUG_SERIAL  Serial
#define APP_SERIAL  Serial1

const byte DNS_PORT = 53;
IPAddress apIP(192, 168, 1, 1);

DNSServer dnsServer;
ESP8266WebServer server ( 80 );

int ln = 0;
String TimeDate = "";
char buffer[12];

const int led = 13;


void updateTime()
{
  // Use WiFiClient class to create TCP connections
  WiFiClient client;
  
 // DEBUG_SERIAL.println(">Sending Header<", 1, 0);

  // Read all the lines of the reply from server and print them to DEBUG_SERIAL
  // expected line is like : Date: Thu, 01 Jan 2015 22:00:14 GMT
  
  String dateTime = "";
 // DEBUG_SERIAL.println(">  Listening...<", 2, 0);

  while(client.available())
  {
    String line = client.readStringUntil('\r');

    if (line.indexOf("Date") != -1)
    {
      DEBUG_SERIAL.print("=====>");
    } else
    {
      // DEBUG_SERIAL.print(line);
      // date starts at pos 7
      //TimeDate = line.substring(7);
      //DEBUG_SERIAL.println(TimeDate);
      // time starts at pos 14
      //TimeDate = line.substring(7, 15);
      //TimeDate.toCharArray(buffer, 10);
      DEBUG_SERIAL.println("UTC Date/Time:");
      TimeDate = line.substring(16, 24);
      TimeDate.toCharArray(buffer, 10);
      DEBUG_SERIAL.println(buffer);
    }
  }

  DEBUG_SERIAL.println();
  DEBUG_SERIAL.println("closing connection");
  DEBUG_SERIAL.println("Close connection");
  DEBUG_SERIAL.println(">Waiting  3 Sec<");
  delay(1000);
  DEBUG_SERIAL.println(">Waiting  2 Sec<");
  delay(1000);
  DEBUG_SERIAL.println(">Waiting  1 Sec<");
  delay(1000);
  DEBUG_SERIAL.println(">WiFi Connected<");

  APP_SERIAL.println ( "Hello from app serial!" );

}

#define TABLE_LENGTH 10
unsigned int buffer_pos = 0;
unsigned long uid_mem[TABLE_LENGTH];
unsigned long time_mem[TABLE_LENGTH];
short in_out_mem[TABLE_LENGTH];

void handleRoot() {
  
  updateTime();
  
  digitalWrite ( led, 1 );
  char temp[1200];
  int sec = millis() / 1000;
  int min = sec / 60;
  int hr = min / 60;

  snprintf ( temp, 1200,
"<html>\
  <head>\
    <meta http-equiv='refresh' content='5'/>\
    <title>ESP8266 Demo</title>\
    <style>\
      body { background-color: #FEFFCD; font-family: Helvetica, Sans-Serif; Color: #68792E; }\
      th{text-align: left;}\
    </style>\
  </head>\
  <body>\
    <h1>Nestbox Passage Monitoring </h1>\
    <p>Uptime: %02d:%02d:%02d</p>\

    <h3>Last 10 ID's read:</h3>",  
    hr, min % 60, sec % 60, buffer
  );


  String uid_table = "<table style=\"width:400px\">\
    <tr><th>Timestamp</th><th>4 byte UID</th><th>IN (1)/ OUT (0)</th></tr>";

  strcat(temp,uid_table.c_str());

  for(int i=1; i < TABLE_LENGTH; i++)
  {
    char table_elem[80];

    unsigned int index = (i+buffer_pos) % TABLE_LENGTH;
    
    snprintf(table_elem, 80, "<tr><td>%06d</td><td>%x</td><td>%01d</td></tr>", time_mem[index], uid_mem[index], in_out_mem[index]);
    strcat(temp, table_elem);
  }
  
    
   String page_end = "</table><br><br><br>\
    //<img src=\"http://nestbox.octanis.org/assets/img/owl.png\"/>\
  </body>\
</html>";

  strcat(temp,page_end.c_str());
  
  server.send ( 200, "text/html", temp );
  delay(50);
  digitalWrite ( led, 0 );
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
  digitalWrite ( led, 1 );
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
  digitalWrite ( led, 0 );
}

#define MSG_LENGTH    4+4+1 
#define UID_POS       0
#define TIMESTAMP_POS 4
#define INOUT_POS     8

void update_passages()
{
  
  APP_SERIAL.setTimeout(2000);
  byte uid_input[MSG_LENGTH];

  byte readlength = APP_SERIAL.readBytes(uid_input,MSG_LENGTH);
  if(readlength == MSG_LENGTH)
  {
    uid_mem[buffer_pos] = *(unsigned long*)(&uid_input[UID_POS]);    //0xdeadbeef;
    time_mem[buffer_pos] = *(unsigned long*)(&uid_input[TIMESTAMP_POS]); //1000 + 10 * buffer_pos;
    in_out_mem[buffer_pos] = uid_input[INOUT_POS];
    buffer_pos += 1;
    buffer_pos = buffer_pos % TABLE_LENGTH;
  }
}

void setup ( void ) {
  pinMode ( led, OUTPUT );
  digitalWrite ( led, 0 );

  APP_SERIAL.begin(115200);
  APP_SERIAL.swap();
  
  DEBUG_SERIAL.begin ( 115200 );

  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
  WiFi.softAP("Nestbox");
  
  dnsServer.setTTL(300);
  dnsServer.setErrorReplyCode(DNSReplyCode::ServerFailure);

  // start DNS server for a specific domain name
  dnsServer.start(DNS_PORT, "www.nestbox.local", apIP);


  server.on ( "/", handleRoot );
  server.on ( "/inline", []() {
    server.send ( 200, "text/plain", "this works as well" );
  } );
  server.on("/time", HTTP_POST, handleUpdateTable);
  server.onNotFound ( handleNotFound );
  server.begin();
  DEBUG_SERIAL.println ( "HTTP server started" );
}
void loop ( void ) {
  dnsServer.processNextRequest();
  server.handleClient();

}

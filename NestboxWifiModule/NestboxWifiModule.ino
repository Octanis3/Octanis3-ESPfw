//
// Echo server the Arduino M0 Pro (based on Atmel SAMD21)


#include <stdarg.h>
#define DEBUG_PRINTING

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

void write_variable(){
   //delay(1000);
  //For sending time
  uint8_t tx_buf [32];
  tx_buf[0] = 'Z' | 0x80;
  for(int i = 1; i <= 4; i++){
    tx_buf[i] = 0x33;
  }
   min_send_frame(&min_ctx, 0x33, tx_buf, 0x05);
}

void read_variable(){
  uint8_t rx_buf [32];
  size_t buf_len;
  if(Serial.available() > 0) {
    buf_len = Serial.readBytes(rx_buf, 32U);
  }
  else {
    buf_len = 0;
  }
 min_poll(&min_ctx, rx_buf, (uint8_t)buf_len);

 Serial1.printf("Recieved message %c \n", min_ctx.rx_frame_payload_buf[0]);
 Serial1.printf("%d", min_ctx.rx_frame_payload_buf[1]);
 Serial1.printf("%d", min_ctx.rx_frame_payload_buf[2]);
 Serial1.printf("%d", min_ctx.rx_frame_payload_buf[3]);
 Serial1.printf("%d", min_ctx.rx_frame_payload_buf[4]);
 Serial1.printf("\n");


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
}

void loop() {
  char buf[32];
  size_t buf_len;

  
  /*if(Serial.available() > 0) {
    buf_len = Serial.readBytes(buf, 32U);
  }
  else {
    buf_len = 0;
  }*/
  /*buf[0] = 'h';
  buf[1] = 'i';
  buf_len = 3;*/
  write_variable();
  Serial1.print("Finished sending frame\n");
  read_variable();
  //Serial.printf("%c%i\n", message, zeit);
}

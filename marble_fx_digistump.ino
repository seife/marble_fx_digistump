/* * Logitech TrackMan Marble FX wheel driver
 *
 * Copyright © 2018-2021 Stefan Seyfried <seife@tuxbox-git.slipkontur.de>
 *
 * This program is free software. It comes without any warranty, to
 * the extent permitted by applicable law. You can redistribute it
 * and/or modify it under the terms of the Do What The Fuck You Want
 * To Public License, Version 2, as published by Sam Hocevar. See
 * http://www.wtfpl.net/ for more details.
 *
 *  tested on: ATTiny85 digispark (probably clone)
 *
 *  PS2++ protocol specs from http://web.archive.org/web/20030714000535/http://dqcs.com/logitech/ps2ppspec.htm
 *
 * based on this: 
 *   Arduino Forum > Topics > Device Hacking > Logitech TrackMan Marble FX USB converter
 *   https://forum.arduino.cc/index.php?topic=365472.0
 *
 * Original implementation was done on a "CJMCU Beetle" mini-Arduino
 *   Leonardo (ATMega32u4) board, but it works as well on a ATTiny85 board.
 *   See https://github.com/seife/marble_fx. This is the successor with even
 *   cheaper hardware :-)
 *
 *  default HW setup
 *   wire PS/2 connector to digispark PIN 0 (data) and 2 (clk)
 *   see: http://playground.arduino.cc/ComponentLib/Ps2mouse
 *
 *  driver limitations:
 *   use at your own risk.
 *   super hack. tested on my own TrackMan Marble FX(T-CJ12) only
 *
 *  functionality:
 *   press red button to emulate wheel movement with the ball
 */

#include <DigiMouse.h>
/* from usbdrv.h, updated in the asm interrupt routine */
extern volatile schar usbRxLen;

/*
 * Pin definitions
 */
#define DATA_PIN 0
#define CLK_PIN  2
#define LED_PIN  1

bool led = 0;
/* convenience for limited debugging */
#define LED_ON     { led = HIGH; digitalWrite(LED_PIN, led); }
#define LED_OFF    { led = LOW;  digitalWrite(LED_PIN, led); }
#define LED_TOGGLE { led = !led; digitalWrite(LED_PIN, led); }

/* global variables */
bool redbutton = false;
int scroll_sum = 0;
uint8_t lastbuttons = 0;
long lastchange = 0;

/*
 * https://www.arduino.cc/reference/en/language/functions/digital-io/pinmode/
 * correctly set the mouse clock and data pins for
 * various conditions.
 */
void setpin(int pin, bool value)
{
  if (value) {
    pinMode(pin, INPUT_PULLUP);
  } else {
    pinMode(pin, OUTPUT);
    digitalWrite(pin, LOW);
  }
}

void mouse_write(uint8_t data)
{
  uint8_t i;
  uint8_t parity = 1;
  /* disable intereupts, so USB does not interfere
   * not sure if this is strictly necessary, but it does not hurt */
  cli();
  /* put pins in output mode */
  setpin(DATA_PIN, HIGH);
  setpin(CLK_PIN, HIGH);
  delayMicroseconds(300);
  setpin(CLK_PIN, LOW);
  delayMicroseconds(300);
  setpin(DATA_PIN, LOW);
  delayMicroseconds(10);
  /* start bit */
  setpin(CLK_PIN, HIGH);
  /* wait for mouse to take control of clock); */
  while (digitalRead(CLK_PIN) == HIGH)
    ;
  /* clock is low, and we are clear to send data */
  for (i=0; i < 8; i++) {
    if (data & 0x01) {
      setpin(DATA_PIN, HIGH);
    } else {
      setpin(DATA_PIN, LOW);
    }
    /* wait for clock cycle */
    while (digitalRead(CLK_PIN) == LOW)
      ;
    while (digitalRead(CLK_PIN) == HIGH)
      ;
    parity = parity ^ (data & 0x01);
    data >>= 1;
  }  
  /* parity */
  setpin(DATA_PIN, parity);
  while (digitalRead(CLK_PIN) == LOW)
    ;
  while (digitalRead(CLK_PIN) == HIGH)
    ;
  /* stop bit */
  setpin(DATA_PIN, HIGH);
  delayMicroseconds(50);
  while (digitalRead(CLK_PIN) == HIGH)
    ;
  /* wait for mouse to switch modes */
  while ((digitalRead(CLK_PIN) == LOW) || (digitalRead(DATA_PIN) == LOW))
    ;
  /* put a hold on the incoming data. */
  setpin(CLK_PIN, LOW);
  sei();
}

/*
 * Get a byte of data from the mouse
 */
uint8_t mouse_read(void)
{
  uint8_t data = 0x00;
  int i;
  uint8_t bit = 0x01;

  /* disable intereupts, so USB does not interfere */
  cli();
  setpin(CLK_PIN, HIGH);
  setpin(DATA_PIN, HIGH);
  delayMicroseconds(50);
  while (digitalRead(CLK_PIN) == HIGH)
    ;
  delayMicroseconds(5);               /* debounce */
  while (digitalRead(CLK_PIN) == LOW) /* eat start bit */
    ;
  for (i=0; i < 8; i++) {
    while (digitalRead(CLK_PIN) == HIGH)
      ;
    if (digitalRead(DATA_PIN) == HIGH) {
      data = data | bit;
    }
    while (digitalRead(CLK_PIN) == LOW)
      ;
    bit <<= 1;
  }
  /* eat parity bit, (ignored) */
  while (digitalRead(CLK_PIN) == HIGH)
    ;
  while (digitalRead(CLK_PIN) == LOW)
    ;
  /* eat stop bit */
  while (digitalRead(CLK_PIN) == HIGH)
    ;
  while (digitalRead(CLK_PIN) == LOW)
    ;

  /* stop incoming data. */
  setpin(CLK_PIN, LOW);
  sei();
  return data;
}

void mouse_init()
{
  setpin(CLK_PIN, HIGH);
  setpin(DATA_PIN, HIGH);
  /* reset */
  mouse_write(0xff);
  mouse_read();  /* ack byte */
  mouse_read();  /* blank */
  mouse_read();  /* blank */
  mouse_write(0xf0);  /* remote mode */
  mouse_read();  /* ack */
  delayMicroseconds(100);
}

// PS2++, extended ps/2 protocol spec.
// http://web.archive.org/web/20030714000535/http://dqcs.com/logitech/ps2ppspec.htm
// also, linux kernel ps2 mouse drivers have extensive code to look up the protocol.
static uint8_t magic[] = { 0xe8, 0x00, 0xe8, 0x03, 0xe8, 0x02, 0xe8, 0x01, 0xe6, 0xe8, 0x03, 0xe8, 0x01, 0xe8, 0x02, 0xe8, 0x03 };
void ps2pp_write_magic_ping()
{
  /* e8 00 e8 03 e8 02 e8 01 e6 e8 03 e8 01 e8 02 e8 03 */
  for (uint8_t i = 0; i < sizeof(magic); i++)
    mouse_write(magic[i]);
}

bool ps2pp_decode(uint8_t b0, uint8_t b1, uint8_t b2)
{
  /* values from linux/drivers/input/mouse/logips2pp.c */
  if ((b0 & 0x48) != 0x48 || (b1 & 0x02) != 0x02)
    return false;
  // mouse extra info
  if ((b0 & 0x30) == 0x0 && (b1 & 0xf0) == 0xd0) {
    redbutton = (b2 & 0x10);
  }
  return true;
}

/* the main() program code */
void setup()
{
  /* enable the watchdog, so that we will notice if the host suspends:
     If the USB bus does not answer, usbInterruptIsReady() will fail
     and the endless loop waiting for it to clear will reset the board */
  wdt_enable(WDTO_4S);
  pinMode(LED_PIN, OUTPUT);
  LED_ON;
  /* now init ps2. If there is no mouse, then this will hang forever => reset */
  mouse_init();
  LED_OFF;
  ps2pp_write_magic_ping();
  LED_ON;
  DigiMouse.begin();
  /* wait until it's initialized... */
  while(usbRxLen <= 0) {
    digitalWrite(LED_PIN, !(millis()&0x3ff));
    wdt_reset();
  }
}

long last_move = 0;
int jigglecount = 0;

void move(int8_t x, int8_t y, int8_t z)
{
  DigiMouse.move(x, y, z);
  last_move = millis();
  jigglecount = 0;
}

void loop()
{
  wdt_reset();
  LED_ON;
  /* this will trigger the watchdog, if the host is suspended */
  while(!usbInterruptIsReady())
    DigiMouse.poll();
  LED_OFF;
  mouse_write(0xeb);  /* give me data! */
  wdt_reset();
  mouse_read();      /* ignore ack */
  uint8_t mstat = mouse_read();
  uint8_t btns = mstat & 0x07; /* 3 buttons */
  int8_t mx    = (int8_t)mouse_read();
  int8_t my    = (int8_t)mouse_read();

  if (!ps2pp_decode(mstat, mx, my)) {
    if (redbutton) { /* translate y scroll into wheel-scroll */
      int8_t scroll = my / 8;
      if (! scroll) {
        scroll_sum += my;
        scroll = scroll_sum / 8;
      }
      if (scroll != 0) {
        scroll_sum = 0;
        move(0, 0, scroll);
      }
    } else {
      /* -my to get the direction right... */
      if (mx != 0 || my != 0) {
        move(mx, -my, 0);
      }
      scroll_sum = 0;
    }

    /* handle normal buttons */
    long now = millis();
    if (btns != lastbuttons) {
      /* debounce - my marble fx has a nervous right-click syndrome ;-) */
      if (now - lastchange > 50) {
        DigiMouse.setButtons(btns);
        lastchange = now;
        lastbuttons = btns;
      }
    }
  }

  long  jiggle = (millis() - last_move);
  if (jiggle > 30000L * (jigglecount + 1) && jiggle < 1800000) {
    jigglecount++;
    DigiMouse.move(1, 1, 0);
    DigiMouse.update();
    DigiMouse.move(-1, -1, 0);
  }
  DigiMouse.delay(20);
}

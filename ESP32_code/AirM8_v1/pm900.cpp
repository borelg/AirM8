/*
  pm900.cpp - Library for temtop PM900 dust sensor.
  Created by Luca Marchiorello, February 13, 2020.
  Released into the public domain.
*/

#include "pm900.h"

PM900::PM900() {
}

bool PM900::begin(HardwareSerial* s) {
  serial = s;
  serial->begin(9600);
  valid = false;
  return true;
}

bool PM900::read() {
  // using while to empty the input buffer in case of incomplete or malformed data
  while (serial->available() > 0) {
    valid = false;
    checksum = 0;
    for (int i = sizeof(currentPacket) - 1; i >= 0 ; i--) {
      *(currentByte + i) = serial->read();
      checksum += *(currentByte + i);
      delay(2);
    }
    checksum -= *(currentByte);
    checksum -= *(currentByte + 1);
    if (checksum == currentPacket.checksum
        && currentPacket.length == 28
        && currentPacket.start_b1 == 0x42
        && currentPacket.start_b2 == 0x4D
       ) valid = true;
  }
  return valid;
}

bool PM900::isValid() {
  return valid;
}

uint16_t PM900::getcount10() {
  return currentPacket.count10;
}

uint16_t PM900::getcount5() {
  return currentPacket.count5;
}

uint16_t PM900::getcount2_5() {
  return currentPacket.count2_5;
}

uint16_t PM900::getcount1() {
  return currentPacket.count1;
}

uint16_t PM900::getcount0_5() {
  return currentPacket.count0_5;
}

uint16_t PM900::getcount0_3() {
  return currentPacket.count0_3;
}

uint16_t PM900::getatmpm10() {
  return currentPacket.atmpm10;
}

uint16_t PM900::getatmpm2_5() {
  return currentPacket.atmpm2_5;
}

uint16_t PM900::getatmpm1() {
  return currentPacket.atmpm1;
}

uint16_t PM900::getpm10() {
  return currentPacket.pm10;
}

uint16_t PM900::getpm2_5() {
  return currentPacket.pm2_5;
}

uint16_t PM900::getpm1() {
  return currentPacket.pm1;
}

void PM900::next(){
	valid = false;
}
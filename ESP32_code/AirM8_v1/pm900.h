/*
  pm900.h - Library for temtop PM900 dust sensor.
  Created by Luca Marchiorello, February 13, 2020.
  Released into the public domain.
*/

#ifndef pm900_h
#define pm900_h

#include "Arduino.h"

// inverted protocol endianness
struct PM900packet {
  uint16_t checksum;
  uint16_t reserved;
  uint16_t count10;
  uint16_t count5;
  uint16_t count2_5;
  uint16_t count1;
  uint16_t count0_5;
  uint16_t count0_3;
  uint16_t atmpm10;
  uint16_t atmpm2_5;
  uint16_t atmpm1;
  uint16_t pm10;
  uint16_t pm2_5;
  uint16_t pm1;
  uint16_t length;
  uint8_t start_b2;
  uint8_t start_b1;
};

class PM900 {
  public:
    PM900();
    bool begin(HardwareSerial*);
	bool begin(int,int);
    bool read(void);
    bool isValid(void);
    uint16_t getcount10(void);
    uint16_t getcount5(void);
    uint16_t getcount2_5(void);
    uint16_t getcount1(void);
    uint16_t getcount0_5(void);
    uint16_t getcount0_3(void);
    uint16_t getatmpm10(void);
    uint16_t getatmpm2_5(void);
    uint16_t getatmpm1(void);
    uint16_t getpm10(void);
    uint16_t getpm2_5(void);
    uint16_t getpm1(void);
	void next(void);

  private:
    HardwareSerial* serial;
    PM900packet currentPacket;
    bool valid;
    uint16_t checksum;
    byte *currentByte = (byte *) & currentPacket;

};

#endif
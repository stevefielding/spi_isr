#include <Arduino.h>
#include <spi_isr.h>
#include "spi_regs.h"

byte testReg1[SPI_BUFFER_LEN];
byte testReg2[SPI_BUFFER_LEN];

bool writeRegs(uint16_t len, uint16_t addr, byte data[]) {
uint16_t i;

  if (addr == TEST_REG1_ADDR) {
    for (i=0; i < len; i++)
      testReg1[i] = data[i];
  }
  else if (addr == TEST_REG2_ADDR) {
    for (i=0; i < len; i++)
      testReg2[i] = data[i];
  }
  Serial.println("writeRegs");
  return false;
}

bool readRegs(uint16_t len, uint16_t addr, byte data[]) {
uint16_t i;
  if (addr == TEST_REG1_ADDR) {
    for (i=0; i < len; i++)
      data[i] = testReg1[i];
  }
  else if (addr == TEST_REG2_ADDR) {
    for (i=0; i < len; i++)
      data[i] = testReg2[i];
  }
  Serial.println("readRegs");
  return false;
}

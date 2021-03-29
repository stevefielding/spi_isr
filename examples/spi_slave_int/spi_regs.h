
#ifndef SPI_REGS_INCLUDE
#define SPI_REGS_INCLUDE

//#define SPI_BUFFER_LEN 512

enum addrMap {
  TEST_REG1_ADDR = 0x1000,
  TEST_REG2_ADDR = 0xf020
  
};

bool writeRegs(uint16_t len, uint16_t addr, byte data[]);
bool readRegs(uint16_t len, uint16_t addr, byte data[]);

#endif

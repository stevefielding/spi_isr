
#ifndef SPI_ISR_INCLUDE
#define SPI_ISR_INCLUDE


#define SYNC1_NIBBLE 0x50
#define SYNC2_NIBBLE 0xa0
#define WRITE_INIT 0x0
#define READ_INIT 0x1
#define DATA_ACCESS 0x2
#define STATUS_READ 0x3
#define RX_RDY_SPI_STATUS_MASK 0x1
#define WR_COMP_SPI_STATUS_MASK 0x2
#define RX_OVR_SPI_STATUS_MASK 0x4
#define TX_UNDR_SPI_STATUS_MASK 0x8
#define WR_REG_ERROR_SPI_STATUS_MASK 0x10
#define RD_REG_ERROR_SPI_STATUS_MASK 0x20
#define SPI_BUFFER_LEN 512

class cSpi_isr
{
public:
  cSpi_isr();
  void SPI0_Handler();
  void spi_isr_init(uint8_t chipSelectPin);
  void spi_update_regs();

  virtual bool readMyRegs(uint16_t len, uint16_t addr, byte data[])
  {
      return(true);
  };

  virtual bool writeMyRegs(uint16_t len, uint16_t addr, byte data[])
  {
      return(true);
  };


private:
  enum isrStateDefs {GET_CTRL_BYTE, GET_SYNC, GET_DATALEN, GET_ADDRESS1, GET_ADDRESS2, WAIT_STATUS_SENT, TRANSFER_READ_DATA, TRANSFER_WRITE_DATA};
  uint16_t isrState = GET_CTRL_BYTE;
  uint16_t isrState2;
  enum txStateDefs {TX_IDLE_STATE, TX_DATA_STATE, TX_STATUS_STATE};
  uint16_t txState = TX_IDLE_STATE;

  bool readTransaction;
  bool readDataReq;       // request copying registers to spiData 
  bool writeDataReq;      // request copying spiData to registers
  bool readDataReady;     // reg contents have been written to spiData array, and the data is ready for access via spi bus
  bool writeDataComplete; // spiData has been copied to the regs, and the write transaction is complete
  bool readRegError;      // An error occured when attempting to read regs and copy to spiData
  bool writeRegError;     // An error occured when attempting to copy data from spiData to the regs
  bool rxOverRunError = false;
  bool txUnderRunError = false;
  byte spiStatus;
  uint16_t spiReadCnt;
  uint16_t spiWriteCnt;
  byte spiData[SPI_BUFFER_LEN];
  byte spiRx;
  uint16_t opcode;
  uint16_t dataLen;
  uint16_t dataLenHi;
  uint16_t dataAddress;
  uint16_t sdIndex = 0;
};


#endif

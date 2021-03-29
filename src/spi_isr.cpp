 
// ------------------------- spi_isr ---------------------------
// Supports data read, data write and status read
// SPI transactions are full duplex, and whenever the master sends data it also receives data.
// The MOSI MISO streams shown below occur simultaneously
// MOSI is from the spi master, MISO is from this spi slave 
// ------ Write data example
// Initialize a write of 2 bytes of data to address 0x1234:   
//    MOSI: WRITE_INIT SYNC2_NIBBLE 0x01 0x12 0x34.
//    MISO: 0x00       0x00         0x00 0x00 0x00
// Write 0xff, 0xfe to 0x1234:
//    MOSI: DATA_ACCESS SYNC2_NIBBLE 0xff 0xfe.
//    MISO: 0x00        0x00         0x00 0x00
// Check for write complete: 
//    MOSI:  STATUS_READ SYNC2_NIBBLE 0x00.
//    MISO:  0x00        0x00         0x02

// ------ Read data example
// For status read and data read, the contents of the data do not matter. For these examples I am using zeros
// Initialize a read of 2 bytes of data from 0x1234.
//    MOSI: READ_INIT SYNC2_NIBBLE 0x01 0x12 0x34.
//    MISO: 0x00      0x00         0x00 0x00 0x00
// Check for read data ready:
//    MOSI:  STATUS_READ SYNC2_NIBBLE 0x00.
//    MISO:  0x00        0x00         0x01
// Read data from address 0x1234. 
//    MOSI: DATA_ACCESS SYNC2_NIBBLE 0x00 0x00. 
//    MISO: 0x00        0x00         0xff 0xfe


// ----------------- Instantiating in your code
// You will need to derive your own class from this class and over-ride
// readMyRegs and writeMyRegs
/*
class cSpi_isr_derived : public cSpi_isr {
   public:
   bool readMyRegs(uint16_t len, uint16_t addr, byte data[]){
      return readRegs(len, addr, data);
   }
   bool writeMyRegs(uint16_t len, uint16_t addr, byte data[]){
      return writeRegs(len, addr, data);
   }
};
*/
// You will need to connect SPI0_Handler to the ISR that is imlemented 
// in the spi_isr class
/*
cSpi_isr_derived spi_isr;
void SPI0_Handler() {
  spi_isr.SPI0_Handler();
}
*/
// ---------------------------------------------


#include <Arduino.h>
#include <SPI.h>
#include "spi_isr.h"
//#include "spi_regs.h"

// Constructor
cSpi_isr::cSpi_isr() {}

// Transmit reg empty interrupts occur at the beginning of the spi transaction
// Receive reg full interrupts occur at the end of the spi transaction
// Thus to start the data and status transmission the TX reg must be loaded after the ctrl
// byte is received and will be transmitted after the sync byte.
// This is why I reversed the order of the ctrl and sync bytes
// ISR execution times:
// Upto 2.3uS and downto 0.8uS execution time for a pair of isr calls. Probably RX and TX respectively. Intra pair gap is
// 2uS at 0.5MHz spi clock, and 0.6uS at 1.8MHz
// Looking at the spi transactions from the RPi, there is very little gap between bytes. 
// At 1.8MHz: 4.5uS between bytes,
// 1.4uS and 0.8uS processing time (This is in the middle of the data payload transfer, ie TRANSFER_WRITE_DATA)
// 0.6uS intra pair gap and 1.5uS inter pair gap. CPU utilization is 2.2*100/4.5 = 49%
// At 0.5MHz: 16uS between bytes,
// 1.4uS and 0.8uS processing time, 1.6uS intra pair gap, and 11.8uS inter pair gap. CPU util = 2.2*100/16 = 13.7%

void cSpi_isr::SPI0_Handler() {
  NVIC_DisableIRQ(SPI0_IRQn);
  PIOB->PIO_SODR = PIO_SODR_P26;  // much faster than digitalWrite(LED_BUILTIN, HIGH);
  uint32_t iStatus = REG_SPI0_SR;

  // check for errors first. Then they can be reported immediately
  // ------------------ Receive over run interrupt
  if(iStatus & SPI_SR_OVRES)
    rxOverRunError = true;
    
  // ------------------ Transmit under run interrupt
  if(iStatus & SPI_SR_UNDES)
    txUnderRunError = true;
  
  // ----------------- Receive data full interrupt
  if(iStatus & SPI_SR_RDRF)  
  {
    spiRx = REG_SPI0_RDR;
    switch(isrState) {
      // Expect sync in the ms nibble , and opcode in the ls nibble.
      // 4 opcodes: readInit, writeInit, statusRead, dataAcess
      case GET_CTRL_BYTE:
        txState = TX_IDLE_STATE;
        // process the opcode if top nibble matches the ctrl sync
        if ((spiRx & 0xf0) == SYNC1_NIBBLE) {
          isrState = GET_SYNC;
          opcode = (uint16_t) spiRx & 0x3;
          if (opcode == DATA_ACCESS) {
            if (readTransaction == true) {
              txState = TX_DATA_STATE;
              spiReadCnt = 0;
              isrState2 = TRANSFER_READ_DATA;
            }
            else {       
              spiWriteCnt = 0;
              isrState2 = TRANSFER_WRITE_DATA;
            }
          }
          else if (opcode == READ_INIT) {
            readTransaction = true;
            isrState2 = GET_DATALEN;
          }
          else if (opcode == WRITE_INIT) {
            readTransaction = false;
            isrState2 = GET_DATALEN;          
          }
          else if (opcode == STATUS_READ) {
            spiStatus = 0x00;
            if (readDataReady) {
              spiStatus |= RX_RDY_SPI_STATUS_MASK;
              readDataReady = false;
            }
            if (writeDataComplete) {
              spiStatus |= WR_COMP_SPI_STATUS_MASK;
              writeDataComplete = false;
            }
            if (rxOverRunError) {
              spiStatus |= RX_OVR_SPI_STATUS_MASK;
              rxOverRunError = false;
            }
            if (txUnderRunError) {
              spiStatus |= TX_UNDR_SPI_STATUS_MASK;
              txUnderRunError = false;
            }
            if (writeRegError) {
              spiStatus |= WR_REG_ERROR_SPI_STATUS_MASK;
              writeRegError = false;
            }
            if (readRegError) {
              spiStatus |= RD_REG_ERROR_SPI_STATUS_MASK;
              readRegError = false;
            }
            txState = TX_STATUS_STATE;
            isrState2 = WAIT_STATUS_SENT;          
          }
        }
        break;
      // Get sync nibble, and dataLen[11:8]
      case GET_SYNC:
        spiReadCnt++;
        if ((spiRx & 0xf0) == SYNC2_NIBBLE) {
          isrState = isrState2;
          dataLenHi = ((uint16_t) spiRx & 0xf) << 8;
        }
        else {
          isrState = GET_CTRL_BYTE;
        }
        break;        
      // Get dataLen[7:0]
      case GET_DATALEN:
        dataLen = dataLenHi | (uint16_t) spiRx;
        // Only procede to the next state if dataLen is valid
        if ((dataLen <= SPI_BUFFER_LEN) && (dataLen != 0))
          isrState = GET_ADDRESS1;
        else
          isrState = GET_CTRL_BYTE;
        break;
      // Wait msb of 16-bit address
      case GET_ADDRESS1:
        dataAddress = (uint16_t) spiRx << 8;
        isrState = GET_ADDRESS2;
        break;
      // Get lsb of 16-bit address
      case GET_ADDRESS2:
        dataAddress = dataAddress | (uint16_t) spiRx;
        isrState = GET_CTRL_BYTE;
        if (readTransaction)
          readDataReq = true;
        break;
      // transfer the read data from spiData array to spiTx
      case TRANSFER_READ_DATA:
        spiReadCnt++;
        //if (spiReadCnt == dataLen || spiReadCnt >= SPI_BUFFER_LEN) {
        if (spiReadCnt >= dataLen) {
          txState = TX_IDLE_STATE;
          isrState = GET_CTRL_BYTE;
        }
        break;
      // Transfer the spiRx data to spiData array
      case TRANSFER_WRITE_DATA:
        spiData[spiWriteCnt++] = spiRx;
        //if (spiWriteCnt == dataLen || spiWriteCnt >= SPI_BUFFER_LEN) {
        if (spiWriteCnt >= dataLen) {
          writeDataReq = true;
          isrState = GET_CTRL_BYTE;
        }
        break;
      // Wait for the previously loaded status data to be sent.
      case WAIT_STATUS_SENT:
        txState = TX_IDLE_STATE;
        isrState = GET_CTRL_BYTE;
        break;
      default:
        isrState = GET_CTRL_BYTE;
        break;
    }
  }

  // ------------------ Transmit data empty interrupt
  if(iStatus & SPI_SR_TDRE)
  {
    if (txState == TX_STATUS_STATE) {
      REG_SPI0_TDR = (uint32_t) spiStatus;
    }
    else if (txState == TX_DATA_STATE) {
      REG_SPI0_TDR = (uint32_t) spiData[sdIndex++];
      if (sdIndex == SPI_BUFFER_LEN)
        sdIndex = 0;
    }
    else {
      sdIndex = 0;
      REG_SPI0_TDR = (uint32_t) 0x0; 
    }
  }

  PIOB->PIO_CODR = PIO_CODR_P26;
  //digitalWrite(LED_BUILTIN, LOW);
  NVIC_EnableIRQ(SPI0_IRQn);
}

void cSpi_isr::spi_isr_init(uint8_t chipSelectPin) {
  SPI.begin(chipSelectPin);
  REG_SPI0_CR = SPI_CR_SWRST;     // reset SPI
  REG_SPI0_CR = SPI_CR_SPIEN;     // enable SPI
  REG_SPI0_MR = SPI_MR_MODFDIS;     // slave and no modefault
  REG_SPI0_CSR = 0x02;    // BITS=0, DLYBCT=0, DLYBS=0, SCBR=0, 8 bit transfer

  // enable tx and rx interrupts
  REG_SPI0_IDR = ~SPI_IDR_RDRF;
  REG_SPI0_IER = SPI_IDR_RDRF;
  REG_SPI0_IDR &= ~SPI_IDR_TDRE;
  REG_SPI0_IER |= SPI_IDR_TDRE;
  // enable rx over run interrupt
  REG_SPI0_IDR &= ~SPI_IDR_OVRES;
  REG_SPI0_IER |= SPI_IDR_OVRES;
  // enable tx under run interrupt
  REG_SPI0_IDR &= ~SPI_IDR_UNDES;
  REG_SPI0_IER |= SPI_IDR_UNDES;
  
  NVIC_EnableIRQ(SPI0_IRQn);
 
}

// service the read and write requests generated in the spi isr
// check for readDataReq and writeDataReq and perform register read and write operations
// Call this function from your main loop
void cSpi_isr::spi_update_regs() {
  // service the read and write requests generated in the spi isr
  if (readDataReq) {
    readRegError = readMyRegs(dataLen, dataAddress, spiData);
    readDataReq = false;
    readDataReady = true;
  }
  if (writeDataReq) {
    writeRegError = writeMyRegs(dataLen, dataAddress, spiData);
    writeDataReq = false;
    writeDataComplete = true; 
  }
}

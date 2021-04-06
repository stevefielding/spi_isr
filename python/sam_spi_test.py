# ------------------------- sam_spi_test ---------------------------
import time
import spidev
from base2designs.util.spiMaster import SpiMaster

# init spiMaster at 1.8MHz
# Measured 250KByte/s at 1.8MHz. ie 2Mbps
#spiMaster = SpiMaster(350000)
spiMaster = SpiMaster(1800000)

# txData1: 512 bytes of data at address 0x1000. 
# Assumes ATSAM has at least 512 byte buffer
txData1 = list(range(256))
txData1.extend(range(256))
dataLen1 = len(txData1)
dataAddr1 = 0x1000

# txData2: 5 bytes of data at address 0xf020
txData2 = list(range(255,250,-1))
dataLen2 = len(txData2)
dataAddr2 = 0xf020

loopCnt = 0
start = time.time()
while True:
  loopCnt += 1

  # ------ unsync the ATSAM spi state machine
  # unsync the spi state machine by sending a short packet, that does not match  dataLen
  # Note that you can drastically improve bit rate if you remove the unsync and resync calls
  spiMaster.unsyncSpi()
  
  # ------ resync the ATSAM spi state machine
  spiMaster.resyncSpi()

  # ------- Write/Read spi test registers
  # change the first byte of txData1 every pass through the loop
  txData1[0] = loopCnt & 0xff
  wrRegError = spiMaster.spiDataWrite(dataAddr1, txData1)
  # change the first byte of txData2 every pass through the loop
  txData2[0] = (loopCnt+1) & 0xff
  wrRegError = spiMaster.spiDataWrite(dataAddr2, txData2)
  spiRxData, rdRegError = spiMaster.spiDataRead(dataAddr1, dataLen1, True, txData1)
  spiRxData, rdRegError  = spiMaster.spiDataRead(dataAddr2, dataLen2, True, txData2)

  if loopCnt % 100 == 0:
    print("#Loop: {}".format( loopCnt))
    print("Moved {} MBytes of data in {} s. {} KByte/s".format(round(loopCnt * 2048 / 1000000, 2), round(time.time() - start, 0),
        round((loopCnt * 2048 * 0.001) / (time.time() -start), 0)))

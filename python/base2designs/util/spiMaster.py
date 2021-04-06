# ------------------------- spiMaster ---------------------------
# Supports data read, data write and status read
# SYNC_NIBBLE2[7:4] is the actual sync and SYNC_NIBBLE[3:0] contains the most significant 4 bits of data length.
# In these examples the dataLen is less than 255, and thus SYNC_NIBBLE[3:0] = dataLen[11:8] = 0
# ------ Write data example
# Initialize a write of 2 bytes of data to address 0x1234.  TX: WRITE_INIT, SYNC_NIBBLE2, 0x2, 0x12, 0x34. RX: 0x00, 0x00, 0x00, 0x00, 0x00
# Write 0xff, 0xfe to 0x1234:  TX: DATA_ACCESS, SYNC_NIBBLE2, 0xff, 0xfe.  RX: 0x00, 0x00, 0x00, 0x00
# Check for write complete:   TX: STATUS_READ, SYNC_NIBBLE2, 0x00.   RX: 0x00, 0x00, 0x02

# ------ Read data example
# Note that SPI transactions are full duplex, and you have to send data to receive data.
# This is only applicable to status read and data read, and the contents of the data do not matter. For these examples I am using zeros
# Initialize a read of 2 bytes of data from 0x1234. TX: READ_INIT, SYNC_NIBBLE2, 0x2, 0x12, 0x34. RX: 0x00, 0x00, 0x00, 0x00, 0x00
# Check for read data ready:   TX: STATUS_READ, SYNC_NIBBLE2, 0x00.  RX: 0x00, 0x00, 0x01
# Read data from address 0x1234. TX: DATA_ACCESS, SYNC_NIBBLE2, 0x00, 0x00. RX: 0x00, 0x00, 0xff, 0xfe

import time
import spidev

# sync nibble. The upper nibble is sync, and the lower nibble is part of the byte count
# during READ_INIT and WRITE_INIT
SYNC_NIBBLE2 = 0xa0

# command bytes. The upper nibble is sync, and the lower nibble is the command
WRITE_INIT = 0x50
READ_INIT = 0x51
DATA_ACCESS = 0x52
STATUS_READ = 0x53

# error status masks
RX_RDY_SPI_STATUS_MASK = 0x1
WR_COMP_SPI_STATUS_MASK = 0x2
RX_OVR_SPI_STATUS_MASK = 0x4
TX_UNDR_SPI_STATUS_MASK = 0x8
# RD and WR REG_ERROR indicate read/write is out of bounds or in the case of a 16-bit or 32-bit access
# the read/write is not on a 16-bit or 32-bit boundary
WR_REG_ERROR_SPI_STATUS_MASK = 0x10
RD_REG_ERROR_SPI_STATUS_MASK = 0x20

class SpiMaster:

  # -------------------------------- __init__  -------------------------------
  def __init__(self, speed):
    spi_ch = 0

    # Enable SPI
    self.spi = spidev.SpiDev(0, spi_ch)
    # occasional tx under run at 2MHz, major problems at 2.4MHz. 1.8MHz is totally solid no
    # over runs or under runs detected
    # With a single CAN obd channel running failures occur all the way down to 0.8MHz.
    # 0.7MHz is borderline. 14660 loops before failure. 
    # Should probably operate at 350KHz or less to be on the safe side.
    # Modified CAN_Acquisition.cpp so that only the CAN1 and Timer3 interrupt are disabled 
    # during a critical sectio of code. After this mod, it appears that spi clock of 1.8MHz
    # is solid even with CAN interface running.
    self.spi.max_speed_hz = speed

  # -------------------------------- spiDataWrite  -------------------------------
  def spiDataWrite(self, addr, data):
    # spi data write consists of 2 phases
    # 1. Write init
    #    Opcode, SYNC_NIBBLE2, dataLen-1, addressMSB, addressLSB
    # 2. Write data
    #    opcode, SYNC_NIBBLE2, data_write

    # ------------ write init phase  
    dataError = True
    # Repeat the request if there is an error
    while dataError:
      dataLen = len(data)
      #print("sync_byte: {}, len: {}".format(SYNC_NIBBLE2 | (dataLen >> 8), dataLen & 0xff))
      msg = [WRITE_INIT, SYNC_NIBBLE2 | (dataLen >> 8), dataLen & 0xff, (addr >> 8) & 0xff, addr & 0xff]
      spiRxData = self.spi.xfer2(msg)
      dataRdy, wrComp, rxUndErr, txOvrErr, wrRegError, rdRegError = self.spiStatusRead()
      dataError = rxUndErr or txOvrErr
      if dataError:
        print("WRITE_INIT data error detected, repeating request")
        #exit()

    # ----------------- write data phase
    dataError = True
    wrRegErrorSticky = False
    while dataError and not wrRegErrorSticky:
      msg = [DATA_ACCESS, SYNC_NIBBLE2]
      msg.extend(data)
      spiRxData = self.spi.xfer2(msg)
      # Wait for write complete 
      cnt = 0
      dataRdy, wrComp, rxUndErr, txOvrErr, wrRegError, rdRegError = self.spiStatusRead()
      dataError = rxUndErr or txOvrErr
      wrRegErrorSticky |= wrRegError
      # Wait for write complete, but if it takes longer than 1000mS then something is wrong
      # Typically this delay is 1 or 2mS, with 1mS delay in spi_slave_int.ino main loop.
      # When the delay is removed then this loop is not even run once. ie "zero" delay
      while (not wrComp and cnt < 1000):
        cnt += 1
        dataRdy, wrComp, rxUndErr, txOvrErr, wrRegError, rdRegError = self.spiStatusRead()
        wrRegErrorSticky |= wrRegError
        time.sleep(0.001)
        dataError |= rxUndErr or txOvrErr
      if dataError:
        print("Write DATA_ACCESS error detected, this is bad, but we will try again anyway")
        #exit()

    return wrRegErrorSticky

  # -------------------------------- spiDataRead  -------------------------------
  def spiDataRead(self, addr, length, compare=False, data=None):
    # spi data read consists of 2 phases
    # 1. Read init
    #    Opcode, SYNC_NIBBLE2, dataLen-1, addressMSB, addressLSB
    # 2. Read data
    #    opcode, SYNC_NIBBLE2, data_read

    # ------------- read init phase  
    dataError = True
    rdRegErrorSticky = False
    # Repeat the request if there is an error
    while dataError:
      msg = [READ_INIT, SYNC_NIBBLE2 | (length >> 8), length & 0xff, (addr >> 8) & 0xff, addr & 0xff]
      spiRxData = self.spi.xfer2(msg)
      # Wait for data ready, and check for errors 
      dataRdy, wrComp, rxUndErr, txOvrErr, wrRegError, rdRegError = self.spiStatusRead()
      rdRegErrorSticky |= rdRegError
      dataError = rxUndErr or txOvrErr
      cnt = 0
      # Wait for dataRdy, but if it takes longer than 1000mS then something is wrong
      # Typically this delay is 1 or 2mS, with 1mS delay in spi_slave_int.ino main loop.
      # When the delay is removed then this loop is not even run once. ie "zero" delay
      while (not dataRdy and  cnt < 1000):
        cnt += 1
        dataRdy, wrComp, rxUndErr, txOvrErr, wrRegError, rdRegError = self.spiStatusRead()
        rdRegErrorSticky |= rdRegError
        dataError |= rxUndErr or txOvrErr
        time.sleep(0.001)
      if dataError:
        print("READ_INIT data error detected, repeating read request")
        #exit()

    # --------------- read data phase
    dataError = True
    while dataError and not rdRegErrorSticky:
      msg = [DATA_ACCESS, SYNC_NIBBLE2]
      msg.extend([0]*length)
      spiRxData = self.spi.xfer2(msg)
      dataRdy, wrComp, rxUndErr, txOvrErr, wrRegError, rdRegError = self.spiStatusRead()
      dataError = rxUndErr or txOvrErr
      if dataError:
        print("read DATA_ACCESS data error detected, repeating data read")
        #exit()
    if compare == True:
      if spiRxData[2:] != data:
        print("Receive data does not match transmitted data")
        print("  TX: {}\n  RX: {}".format(data, spiRxData[2:]))
        exit()

    return spiRxData[2:], rdRegErrorSticky

  # -------------------------------- spiStatusRead -------------------------------
  def spiStatusRead(self):
    # spi status read consists of 1 phase
    # 1. Status read init
    #    Opcode, SYNC_NIBBLE2, status_read

    msg = [STATUS_READ, SYNC_NIBBLE2, 0x0]
    spiRxData = self.spi.xfer2(msg)
    if spiRxData[2] & RX_RDY_SPI_STATUS_MASK != 0x0:
      readDataReady = True
    else:
      readDataReady = False
    if spiRxData[2] & WR_COMP_SPI_STATUS_MASK != 0x0:
      writeDataComplete = True
    else:
      writeDataComplete = False
    if spiRxData[2] & RX_OVR_SPI_STATUS_MASK != 0x0:
      rxOverRunError = True
      print("[ERROR] Receive over run detected")
    else:
      rxOverRunError = False
    if spiRxData[2] & TX_UNDR_SPI_STATUS_MASK != 0x0:
      txUnderRunError = True
      print("[ERROR] Transmit under run detected")
    else:
      txUnderRunError = False
    if spiRxData[2] & WR_REG_ERROR_SPI_STATUS_MASK != 0x0:
      wrRegError = True
      print("[ERROR] Write reg error detected")
    else:
      wrRegError = False
    if spiRxData[2] & RD_REG_ERROR_SPI_STATUS_MASK != 0x0:
      rdRegError = True
      print("[ERROR] Read reg error detected")
    else:
      rdRegError = False

    return readDataReady, writeDataComplete, rxOverRunError, txUnderRunError, wrRegError, rdRegError

  # -------------------------------- unsyncSpi -------------------------------
  def unsyncSpi(self):  
    # !!!!!!!!!!!!!!!!!! For debug purposes only.
    #print("loopCnt: {}".format(loopCnt))
    # unsync the spi state machine by sending a short packet, that does not match  dataLen
    msg = [READ_INIT, SYNC_NIBBLE2, 10, 0x10, 0x0]
    spiRxData = self.spi.xfer2(msg)
    msg = [DATA_ACCESS, SYNC_NIBBLE2]
    msg.extend([0]*2)
    spiRxData = self.spi.xfer2(msg)
  
  # -------------------------------- resyncSpi -------------------------------
  def resyncSpi(self):
    # This is largely for debug, but it might be useful if we ever run into problems with the 
    # ATSAM spi state machine getting out of sync
    # This is how you resync the state machine.
    # Resync will not clear any pending reads or more importantly writes which could corrupt memory, but it
    # will get the spi state machine back to working order
    # resync by sending any byte where the upper nibble does not equal 0x5, and clearing the status by reading the status reg
    # The length of the resync depends on the size of the maximum size of the data buffer on the ATSAM
    # At the time that this was written the data buffer was 512 bytes. Also need to allow for the sync, and 
    msg = [0x00] * 260
    spiRxData = self.spi.xfer2(msg)
    # Now you need to give the state machine time to perform any bogus read or write before checking and clearing the status
    time.sleep(0.01)
    dataRdy, wrComp, rxUndErr, txOvrErr, wrRegError, rdRegError = self.spiStatusRead()

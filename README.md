# spi_isr
Arduino library to support spi slave interrupt service routines.  
You will need to do the following in order to use this library:  
1. From setup, call spi_isr_init() with the pin number used for spi chip select.
2. Implement two functions, readRegs() and writeRegs(). These functions transfer data to and from the data buffers defined in the library. 
3. link readRegs() and writeRegs() to virtual functions defined in the library.
4. Link SPI0_Handler().
5. From loop, call spi_update_regs().

Read and write requests must be formed into a series of messages.  
These messages allow the user to initialize a read or write, specifying the data length and address.  
The ISR does not actually read or write data to the users registers. This must be performed in a regularly 
scheduled function call, which tests for read or write requests, and copies data to or from a buffer that is
local to the isr.  
The user must ensure that they call the spi_update_regs function from their main loop. 
Then spi_update_regs will check to determine if there is data to be 
read or written and call readRegs or writeRegs if required.

Control message commands:
| Command       |  Value  | Description                                  |
| ------------- | ------- | -------------------------------------------- |
| WRITE_INIT    | 0x0     | Request a write of x data bytes to address y |
| READ_INIT     | 0x1     | Request a read of x data bytes from address y|
| DATA_ACCESS   | 0x2     | Read or write the data                       |
| STATUS_READ   | 0x3     | Check the status of the last operation       |

Error codes returned by STATUS_READ:
|        Error                 | Mask |        Description                                  |
| ---------------------------- | ---- | --------------------------------------------------- |
|RX_RDY_SPI_STATUS_MASK        | 0x1  | Receive data has been read and is ready for transfer|
|WR_COMP_SPI_STATUS_MASK       | 0x2  | Write data has been written                         |
|RX_OVR_SPI_STATUS_MASK        | 0x4  | SPI receive over run error                          | 
|TX_UNDR_SPI_STATUS_MASK       | 0x8  | SPI transmit under run error                        |
|WR_REG_ERROR_SPI_STATUS_MASK  | 0x10 | Unspecified write error                             |
|RD_REG_ERROR_SPI_STATUS_MASK  | 0x20 | Unspecified read error                              |

Individual messages can be combined into read and write data sequences
## Read sequence
1. READ_INIT
2. STATUS_READ
3. DATA_ACCESS
## Write sequence
1. WRITE_INIT
2. DATA_ACCESS
3. STATUS_READ

## Examples of each message type:  
## WRITE_INIT or READ_INIT: 
|           MOSI             |     MISO      |
| -------------------------- | ------------- |
|SYNC1_NIBBLE, ctrlNibble    | 0x0           |
|SYNC2_NIBBLE, dataLen[11:8] | 0x0           |
|dataLen[7:0]                | 0x0           |
|address[15:8]               | 0x0           |
|address[7:0]                | 0x0           |

## DATA_ACCESS:
If the previous message was a WRITE_INIT then the data is in MOSI,
else if the previous message was a READ_INIT then the data is in MISO
|           MOSI             |     MISO      |
| -------------------------- | ------------- |
|SYNC1_NIBBLE, ctrlNibble    | 0x0           |
|SYNC2_NIBBLE, 0x0           | 0x0           |
|data0                       | data0         |
|data1                       | data1         |
|.                           | .             |
|.                           | .             |
|.                           | .             |
|dataN                       | dataN         |

## STATUS_READ:    
|           MOSI             |     MISO      |
| -------------------------- | ------------- |
|SYNC1_NIBBLE, ctrlNibble    | 0x0           |
|SYNC2_NIBBLE, 0x0           | 0x0           |
|0x0                         | status        |

## Constant definitions
SYNC1_NIBBLE 0x50
SYNC2_NIBBLE 0xa0

## Implementing readRegs and writeRegs
You will need to over-ride the virtual functions, readMyRegs and writeMyRegs, and
then create a link to your own functions.  
These functions will be called when you call spi_update_regs from your main loop.
It is entirely up to the user to decide how to interpret the address and how to combine bytes into
16-bit and 32-bit values.  
readMyRegs and writeMyRegs take a 16-bit data length, 16-bit address, and a pointer to a byte data array.  
'readMyRegs':  Read 'length' bytes of user data and copy to the 'data' array. The bool return
value should indicate if the read was successful.  
'writeMyRegs': Write 'length' bytes of data from 'data' array to user data.

You will also need to link the class implemented SPI0_Handler to SPI0_Handler, call spi_isr_init from
setup.

See example project.

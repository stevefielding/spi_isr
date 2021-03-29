# spi_isr
Arduino library to support spi slave interrupt service routines.
You will need to over-ride the virtual functions, readMyRegs and writeMyRegs, and
then create a link to your own functions.  
These functions will be called when you call update_regs from your main loop.
It is entirely up to the user to decide how to interpret the address and how to combine bytes into
16-bit and 32-bit values.  
readMyRegs and writeMyRegs take a 16-bit data length, 16-bit address, and a pointer to a byte data array.  
'readMyRegs':  Read 'length' bytes of user data and copy to the 'data' array. The bool return
value should indicate if the read was successful.  
'writeMyRegs': Write 'length' bytes of data from 'data' array to user data.

You will also need to link the class implemented SPI0_Handler to SPI0_Handler, call spi_isr_init from
setup.

See example project.

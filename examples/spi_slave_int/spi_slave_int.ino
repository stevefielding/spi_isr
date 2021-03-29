 
// ------------------------- spi_slave_int ---------------------------


#include <Arduino.h>
#include <SPI.h>
#include <spi_isr.h>
#include "spi_regs.h"


// Deriving spi class so that we can over-ride 'readRegs' and 'writeRegs'
// and replace them calls to readRegs and writeRegs implemented in spi_regs.cpp
class cSpi_isr_derived : public cSpi_isr {
   public:
   bool readMyRegs(uint16_t len, uint16_t addr, byte data[]){
      return readRegs(len, addr, data);
   }
   bool writeMyRegs(uint16_t len, uint16_t addr, byte data[]){
      return writeRegs(len, addr, data);
   }
};

// We need to connect SPI0_Handler to the ISR that is imlemented 
// in the spi_isr class
cSpi_isr_derived spi_isr;
void SPI0_Handler() {
  spi_isr.SPI0_Handler();
}

#define SPI_SLAVE_CHIP_SELECT_PIN 54

void setup() {
	Serial.begin(115200);
	while(!Serial);
  Serial.println("Waiting for 10s");
  // Adding a 3s delay fixes a problem where the spi interface fails to work on power up.
  // Note that this is a lot shorter than the boot time for the Pi, but it is probably enought time for the boot loader to run. 
  // Theory: At power up, the spi pins from the RPi are messed up, and they somehow get the spi interrupt handler into a 
  // state that it cannot recover from.
  // Tried turning on the tx under run and rx over run interrupts in case the normal rx and tx interrupts where no longer working
  // and the status register needed to be read to clear the errors, but this made no difference. 
  // Although fixed for now, there could still be some underlying problem with the interface.
  delay(3000);
  pinMode(LED_BUILTIN, OUTPUT);
  Serial.println("Starting setup");        
	spi_isr.spi_isr_init(SPI_SLAVE_CHIP_SELECT_PIN);
}

int loopCnt = 0;
bool ledOn = false;
void loop() {

  // flash an led
  loopCnt++;
  if ((loopCnt % 100000) == 0) {
    if (ledOn == false) {
      ledOn = true; 
      //digitalWrite(LED_BUILTIN, HIGH);
    }
    else {
      ledOn = false; 
      //digitalWrite(LED_BUILTIN, LOW);
    }
  }  
  //delay(1);

  // service the read and write requests generated in the spi isr
  spi_isr.spi_update_regs();
}

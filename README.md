VibeSystem
==========

Firmware for Dame Products Vibe V1

To program the Vibe...

1) get a super-cheap MSP430 Launchpad...

http://www.ti.com/ww/en/launchpad/launchpads-msp430-msp-exp430g2.html#tabs

2) then update the firmware...

http://processors.wiki.ti.com/index.php/MSP430_LaunchPad_Firmware_Update

3) unplug the USB from the LaunchPad
4) then remove the prgramming jumpers from the LuanchPad
5) disconnect the battery from the Vibe
6) connect to the Vibe programming pins like this...

LaunchPad|Vibe
---------|----
TEST|TCK
RST|TDO
RXD|_no connection_
TXD|_no connection_
VCC|+3V

7) connect one of the GND pins on the launchPad to the GND pin on the Vibe programming heaeder.
8) Install the MSP430 IAR compiler from here...

http://www.iar.com/Products/IAR-Embedded-Workbench/TI-MSP430/

You can activate the code limited version - we don't need much code to the the Vibe vibe!

9) reconnect the USB to the LaunchPad

10) Open the workspace file in this repository with IAR. 

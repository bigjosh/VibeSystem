VibeSystem
==========

Firmware for Dame Products Vibe V2

User Interface
--------------
Pushing button cycles through 3 speed settings and then back to off.

Holding the button down for about 1/2 second also turns off. 

Automatically turns off on low battery condition. Pressing button when battery is low will not turn on motor but instead will light the red LED. 

Motor will not turn on while chargering. 

Pulsing white LED indicates charging. Solid on white LED indicates full charge. Solid white LED with pulsing red LED indicates charging fault including low charger voltage, defective battery, or over tempurature. 


Burning
-------

To program the Vibe from a Windows machine...

1. get a super-cheap MSP430 Launchpad...

  http://www.ti.com/ww/en/launchpad/launchpads-msp430-msp-exp430g2.html#tabs

2. then update the firmware...

  http://processors.wiki.ti.com/index.php/MSP430_LaunchPad_Firmware_Update

3. unplug the USB from the LaunchPad

4. remove the programming jumpers from the LuanchPad

5. disconnect the battery from the Vibe

6. connect to the Vibe programming pins like this...

  LaunchPad|Vibe
  ---------|----
  TEST|TCK
  RST|TDO
  RXD|_no connection_
  TXD|_no connection_
  VCC|+3V

  Conect to the USB side of these pins.

  Photo <a href="Programming%20Connections.jpg">here</a>.


7. connect one of the GND pins on the launchPad to the GND pin on the Vibe programming header.

8. Install the CC6 compiler from TI...

  http://www.ti.com/tool/CCSTUDIO

9. reconnect the USB to the LaunchPad. Best to leave the battery unconnected.

10. Open the workspace file in this repository. 

# Firefly
Own Dervivation of an electronic Firefly simulator with AVR Tiny based on a thread from http://www.mikrocontroller.net/topic/99803.  
This device is really nice to see and a perfect gift.  
I use Eclipse Kepler with avrdude and AVR Eclipse Plugin http://sourceforge.net/projects/avr-eclipse/
PCB Layout with Eagle is included.  
Variant 1 was not running stable after a couple of weeks.  
Variant 2 got an hardware watchdog circuit applied was running longer but also died after couple of months.  
Some people in the forum have the circuit running for years, but I could not dig out why mine was failing.  
The circuit will be powerd with an solar cell and 2 AAA NiMH akkus, and will dimm the leds like fireflies in darkness using different patterns.  
The original author of the SW is mentioned in the thread above.   
I will make a new PCB with gold plated battery clips, which is in my opinion the problem of the unstability.
Also I'm thinking of converting the project to an PIC24 using only C and get rid of the assembler stuff.  


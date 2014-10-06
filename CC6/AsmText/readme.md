Key files are:

*Main.asm - The  entry point on power up. Just a tiny stub that sets the stack pointer and jumps to C.
*CCode.c  - The meat of the program in ugly hacked code to get it to fit into this tiny device. 

Note that this project only works with the Debug build configuration. There is some settig in the release that does not 
liek the ASM startup. 

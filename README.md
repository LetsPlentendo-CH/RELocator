# RELocator
This is a tool for converting .rel files to .elf. It relocates the files to given memory addresses. I used this project for generating binary files for my Ghidra database of NSMBW.
## Usage
The makefile converts the four .rel files of NSMBW to .elf files. Place these files in the project directory (Get these from a dump of your game):
- d_basesNP.rel.LZ
- d_en_bossNP.rel.LZ
- d_enemiesNP.rel.LZ
- d_profileNP.rel.LZ

Then, with gcc installed, run `make`.
The output will give you commands to create binaries with `powerpc-eabi-objcopy`.

## To-Do
- Be able convert rels to relocatable .elf files
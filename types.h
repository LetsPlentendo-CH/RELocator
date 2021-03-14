#ifndef TYPES_H_
#define TYPES_H_

#define u8 unsigned char
#define u16 unsigned short
#define u32 unsigned int

#define s8 char
#define s16 short
#define s32 int

#define R_PPC_NONE            0
#define R_PPC_ADDR32          1
#define R_PPC_ADDR24          2
#define R_PPC_ADDR16          3
#define R_PPC_ADDR16_LO       4
#define R_PPC_ADDR16_HI       5
#define R_PPC_ADDR16_HA       6
#define R_PPC_ADDR14          7
#define R_PPC_ADDR14_BRTAKEN  8
#define R_PPC_ADDR14_BRNTAKEN 9
#define R_PPC_REL24           10
#define R_PPC_REL14           11
#define R_PPC_REL14_BRTAKEN   12
#define R_PPC_REL14_BRNTAKEN  13

#define R_DOLPHIN_NOP     201
#define R_DOLPHIN_SECTION 202
#define R_DOLPHIN_END     203
#define R_DOLPHIN_MRKREF  204

#define u8tu32(array, pos) (array[pos] << 24 | array[pos + 1] << 16 | array[pos + 2] << 8 | array[pos + 3])
#define align(val) (((val - 1) & 0xFFFFFFF0) + 0x10)

u8 swap8(u8 a) {
    return a;
}

u16 swap16(u16 a) {
    return (a >> 8 & 0x00FF) | (a << 8 & 0xFF00);
}

u32 swap32(u32 a) {
    return ((a >> 24) & 0x000000FF) | ((a >> 8) & 0x0000FF00) | ((a << 8) & 0x00FF0000) | ((a << 24) & 0xFF000000);
}

typedef struct relHeader {
    u32 id;
    u32 nextModule;
    u32 prevModule;
    u32 numSections;
    u32 sectionInfoOffset;
    u32 nameOffset;
    u32 nameSize;
    u32 version;
    u32 bssSize;
    u32 relOffset;
    u32 impOffset;
    u32 impSize;
    u8 prologSection;
    u8 epilogSection;
    u8 unresolvedSection;
    u8 bssSection;
    u32 prolog;
    u32 epilog;
    u32 unresolved;
    u32 align;
    u32 bssAlign;
    u32 fixSize;
} relHeader;

typedef struct {
    u32 offset;
    u32 secLength;
} section;

typedef struct {
    u32 moduleNum;
    u32 offset;
} imp;

typedef struct {
  u16 offset; // byte offset from previous entry
  u8 type;
  u8 section;
  u32 addend;
} reloc;

typedef struct {
    u32 fileSize;
    u8 realNumSections;
    relHeader hdr;
    section *sectionTable;
    section *elfSectionTable;
    imp *impTable;
    reloc **relocs;
    u8 *data;
    char fName[0x20];
} relFile;

#endif
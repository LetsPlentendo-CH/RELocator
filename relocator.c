// Part of RELocator, written by RootCubed 2020

// TODO:
// add prolog, epilog and unresolved functions to elf

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "types.h"
#include "elf.h"

#ifdef DEBUG
    #define DEBUG_PRINT(x) printf x
#else
    #define DEBUG_PRINT(x) do {} while (0)
#endif


void loadRel(u8 *, int, relFile *);
elfFile *relToElf(relFile *, int);
void writeElf(elfFile *, FILE *);
void relocateRelFile(relFile *, int);

relFile *relFileData;

u8 *fileBuf;
u32 *relocOffsets;

int main(int argc, char ** argv) {
    // relocator rel1.rel <addr> rel2.rel <addr> ... reln.rel <addr>
    int countRels = (argc - 1) / 2;

    relFileData = malloc((countRels + 1) * sizeof(relFile));
    relocOffsets = malloc((countRels + 1) * sizeof(u32));
    relocOffsets[0] = 0;

    for (int i = 0; i < countRels; i++) { // load all rels
        DEBUG_PRINT(("opening %s\n", argv[i * 2 + 1]));
        FILE *relF = fopen(argv[i * 2 + 1], "rb");
        fseek(relF, 0, SEEK_END);
        long size = ftell(relF);
        DEBUG_PRINT(("size 0x%04lx\n", size));
        rewind(relF);

        fileBuf = (u8 *)malloc(size);
        fread(fileBuf, 1, size, relF);
        fclose(relF);
        relFile tmpRel;

        strcpy(tmpRel.fName, argv[i * 2 + 1]);
        tmpRel.fName[strlen(tmpRel.fName) - 4] = '\0';
        loadRel(fileBuf, size, &tmpRel);
        DEBUG_PRINT(("loaded rel module %x (%s)\n\n\n", swap32(tmpRel.hdr.id), tmpRel.fName));

        free(fileBuf);
        relocOffsets[swap32(tmpRel.hdr.id)] = strtoul(argv[i * 2 + 2], NULL, 16);
        memcpy(&relFileData[swap32(tmpRel.hdr.id)], &tmpRel, sizeof(relFile));
    }

    elfFile **elfs = malloc((countRels + 2) * sizeof(elfFile *));
    //u32 sHdrNum = 2; // null and shrtrtab

    for (int i = 0; i < countRels; i++) {
        relocateRelFile(relFileData, i + 1);

        elfs[i] = relToElf(relFileData, i + 1);
        //sHdrNum += elfs[i]->hdr.e_shnum - 2;
    }

    char userCommands[0x1000];
    for (int i = 0; i < countRels; i++) {
        char fileName[256];
        char *tmpName = relFileData[i + 1].fName;
        strcpy(fileName, tmpName);
        strcat(fileName, ".elf");
        DEBUG_PRINT(("Saving %s:\n", fileName));
        FILE *outFile = fopen(fileName, "wb");
        writeElf(elfs[i], outFile);
        fclose(outFile);

        char binName[256];
        strcpy(binName, tmpName);
        strcat(binName, ".bin");

        char bssName[256];
        sprintf(bssName, ".bss.%s", tmpName);

        char tmpBuf[0x400];
        sprintf(tmpBuf, "powerpc-eabi-objcopy --set-start 0x%08x --set-section-flags %s=alloc,load,contents -Obinary %s %s\n",
            swap32(elfs[i]->secHdrs[1].sh_addr),
            bssName,
            fileName,
            binName);
        strcat(userCommands, tmpBuf);

        free(relFileData[i].impTable);
        free(relFileData[i].sectionTable);
        free(relFileData[i].elfSectionTable);
        free(relFileData[i].data);
        free(relFileData[i].relocs);
    }
    free(relocOffsets);

    printf("\nDone.\nRun these commands to generate binary files:\n%s\n", userCommands);
}

void loadRel(u8 *fileBuffer, int length, relFile *rel) {
    memcpy(&rel->hdr, fileBuffer, sizeof(relHeader));
    DEBUG_PRINT(("ID %d\n", swap32(rel->hdr.id)));
    DEBUG_PRINT(("numSections %d\n", swap32(rel->hdr.numSections)));
    DEBUG_PRINT(("sectionInfoOffset %d\n", swap32(rel->hdr.sectionInfoOffset)));
    DEBUG_PRINT(("prolog 0x%04x\n", swap32(rel->hdr.prolog)));
    DEBUG_PRINT(("epilog 0x%04x\n", swap32(rel->hdr.epilog)));
    DEBUG_PRINT(("unresolved 0x%04x\n", swap32(rel->hdr.unresolved)));
    DEBUG_PRINT(("bssSize %x\n", swap32(rel->hdr.bssSize)));
    DEBUG_PRINT(("fixSize %x\n", swap32(rel->hdr.fixSize)));
    DEBUG_PRINT(("---------SECTION TABLE--------\n"));
    DEBUG_PRINT(("|--Offset--|isExec|--Length--|\n"));
    u32 secInfoOffset = swap32(rel->hdr.sectionInfoOffset);
    u32 numSections = swap32(rel->hdr.numSections);
    rel->sectionTable = (section *)malloc(sizeof(section) * numSections);
    rel->elfSectionTable = (section *)malloc(sizeof(section) * numSections);
    int index = 0;
    rel->realNumSections = 0;
    for (u32 pos = secInfoOffset; pos < secInfoOffset + numSections * 8; pos += 8) {
        rel->sectionTable[index].offset = u8tu32(fileBuffer, pos) & 0xFFFFFFFC;
        rel->sectionTable[index].offset |= fileBuffer[pos + 3] & 1; // execFlag
        rel->sectionTable[index].secLength = u8tu32(fileBuffer, pos + 4);

        DEBUG_PRINT(("|0x%08x|%6d|0x%08x|\n", rel->sectionTable[index].offset & 0xFFFFFFFC, rel->sectionTable[index].offset & 1, rel->sectionTable[index].secLength));
        if (rel->sectionTable[index].secLength > 0) {
            rel->elfSectionTable[rel->realNumSections] = rel->sectionTable[index];
            rel->realNumSections++;
        }
        index++;
    }

    DEBUG_PRINT(("----------IMP TABLE----------\n"));
    DEBUG_PRINT(("|-Module #-|Offset from REL|\n"));
    u32 impOffset = swap32(rel->hdr.impOffset);
    u32 impSize = swap32(rel->hdr.impSize);
    rel->impTable = (imp *)malloc(impSize);
    index = 0;
    u32 largestModuleNum = 0;
    for (u32 pos = impOffset; pos < impOffset + impSize; pos += 8) {
        rel->impTable[index].moduleNum = u8tu32(fileBuffer, pos);
        rel->impTable[index].offset = u8tu32(fileBuffer, pos + 4);
        DEBUG_PRINT(("|0x%08x|     0x%08x|\n", rel->impTable[index].moduleNum, rel->impTable[index].offset));
        if (rel->impTable[index].moduleNum > largestModuleNum) {
            largestModuleNum = rel->impTable[index].moduleNum;
        }
        index++;
    }
    
    rel->relocs = (reloc **)malloc((largestModuleNum + 1) * sizeof(reloc *));
    for (u32 i = 0; i < impSize / sizeof(imp); i++) {
        imp currImp = rel->impTable[i];
        u32 relOffset = currImp.offset;
        reloc *tempRelocs = malloc(length); // I know this is too large but whatever
        index = 0;
        for (int pos = relOffset; pos < length; pos += 8) {
            tempRelocs[index].offset = fileBuffer[pos] << 8 | fileBuffer[pos + 1];
            tempRelocs[index].type = fileBuffer[pos + 2];
            tempRelocs[index].section = fileBuffer[pos + 3];
            tempRelocs[index].addend = u8tu32(fileBuffer, pos + 4);
            if (tempRelocs[index].type == R_DOLPHIN_END) {
                index++;
                break;
            }
            index++;
        }
        DEBUG_PRINT(("%d relocs for relocating against module %d\n", index, currImp.moduleNum));

        rel->relocs[currImp.moduleNum] = (reloc *)calloc(index, sizeof(reloc));
        memcpy(rel->relocs[currImp.moduleNum], tempRelocs, index * sizeof(reloc));
        free(tempRelocs);
    }
    DEBUG_PRINT(("impOffs %04x\n", length));// - swap32(rel->hdr.impOffset));
    rel->data = malloc(length);// - swap32(rel->hdr.impOffset));
    memcpy(rel->data, fileBuffer, length);// - swap32(rel->hdr.impOffset));
    rel->fileSize = length;// - swap32(rel->hdr.impOffset);
}

elfFile *relToElf(relFile *rel, int index) {
    relFile thisRel = rel[index];
    elfFile *outELF = calloc(1, sizeof(elfFile));
    elfHeader *outELFHeader = &(outELF->hdr);
    *(u32 *)(&outELFHeader->e_ident) = swap32(0x7F454C46);
    outELFHeader->e_ident[4] = 1; // 32-bit
    outELFHeader->e_ident[5] = 2; // big endian
    outELFHeader->e_ident[6] = 1; // version
    outELFHeader->e_type = swap16(1); // executable file (we do the relocation in here)
    outELFHeader->e_machine = swap16(0x14); // PowerPC
    outELFHeader->e_version = swap16(1); // version
    outELFHeader->e_entry = swap32(0); // no entrypoint, since this is just additional code
    outELFHeader->e_phoff = swap32(0x40); // hardcoded since this never changes
    outELFHeader->e_ehsize = swap16(0x34); // header size
    outELFHeader->e_phentsize = swap16(sizeof(elfPrgHeader));
    outELFHeader->e_phnum = swap16(1); // number of program headers
    outELFHeader->e_shentsize = swap16(sizeof(elfSecHeader));
    outELFHeader->e_shnum = swap16(thisRel.realNumSections + 2);
    outELF->prgHdr = malloc(sizeof(elfPrgHeader));
    elfPrgHeader *outELFPrgHeader = outELF->prgHdr;
    outELFPrgHeader->p_type = swap32(1); // PT_LOAD
    outELFPrgHeader->p_offset = swap32(0);
    outELFPrgHeader->p_vaddr = swap32(relocOffsets[swap32(thisRel.hdr.id)]);
    outELFPrgHeader->p_paddr = swap32(relocOffsets[swap32(thisRel.hdr.id)]);
    outELFPrgHeader->p_filesz = swap32(thisRel.fileSize);
    outELFPrgHeader->p_memsz = swap32(thisRel.fileSize);

    outELF->sectionData = thisRel.data;

    elfSecHeader *secHeaders = calloc((thisRel.realNumSections + 2), sizeof(elfSecHeader));
    outELFHeader->e_shstrndx = swap16(swap16(outELFHeader->e_shnum) - 1);
    char **secNameBuf = calloc((thisRel.realNumSections + 2), sizeof(char *));
    elfSecHeader *nullHdr = &secHeaders[0];
    nullHdr->sh_name = swap32(0);
    nullHdr->sh_type = swap32(SHT_NULL);
    nullHdr->sh_flags = swap32(0);
    nullHdr->sh_addr = swap32(0);
    nullHdr->sh_offset = swap32(0);
    nullHdr->sh_size = swap32(0);
    nullHdr->sh_link = swap32(SHN_UNDEF);
    nullHdr->sh_info = swap32(0);
    nullHdr->sh_addralign = swap32(0);
    nullHdr->sh_entsize = swap32(0);
    secNameBuf[0] = calloc(0x20, sizeof(char));
    strcpy(secNameBuf[0], "");

    elfSecHeader *shrtrtab = &secHeaders[swap16(outELFHeader->e_shnum) - 1];
    shrtrtab->sh_name = swap32(outELFHeader->e_shstrndx);
    shrtrtab->sh_type = swap32(SHT_STRTAB);
    shrtrtab->sh_flags = swap32(0);
    shrtrtab->sh_addr = swap32(0);
    shrtrtab->sh_offset = swap32(0);
    shrtrtab->sh_size = swap32(0);
    shrtrtab->sh_link = swap32(SHN_UNDEF);
    shrtrtab->sh_info = swap32(0);
    shrtrtab->sh_addralign = swap32(0);
    shrtrtab->sh_entsize = swap32(0);
    secNameBuf[swap16(outELFHeader->e_shnum) - 1] = calloc(0x20, sizeof(char));
    strcpy(secNameBuf[swap16(outELFHeader->e_shnum) - 1], ".shrtrtab");

    for (int i = 0; i < thisRel.realNumSections; i++) {
        secNameBuf[i + 1] = calloc(0x20, sizeof(char));
        char *currSecName = secNameBuf[i + 1];
        elfSecHeader *tmpHdr = &secHeaders[i + 1];
        if (thisRel.elfSectionTable[i].offset & 1) {
            sprintf(currSecName, ".text.%s", thisRel.fName);
            tmpHdr->sh_type = swap32(SHT_PROGBITS);
            tmpHdr->sh_flags = swap32(SHF_ALLOC | SHF_EXECINSTR);
        } else {
            if (thisRel.elfSectionTable[i].offset == 0) {
                sprintf(currSecName, ".bss.%s", thisRel.fName);
                tmpHdr->sh_type = swap32(SHT_NOBITS);
                tmpHdr->sh_flags = swap32(SHF_ALLOC | SHF_WRITE);
                tmpHdr->sh_addr = swap32(relocOffsets[swap32(thisRel.hdr.id)] + swap32(thisRel.hdr.fixSize));
                tmpHdr->sh_offset = thisRel.hdr.fixSize;
                tmpHdr->sh_size = swap32(thisRel.elfSectionTable[i].secLength);
            } else {
                sprintf(currSecName, ".data.%d.%s", i, thisRel.fName);
                tmpHdr->sh_type = swap32(SHT_PROGBITS);
                tmpHdr->sh_flags = swap32(SHF_ALLOC | SHF_WRITE);
            }
        }
        //sh_name will come later
        if (thisRel.elfSectionTable[i].offset > 0) { // not bss
            tmpHdr->sh_addr = swap32(relocOffsets[swap32(thisRel.hdr.id)] + thisRel.elfSectionTable[i].offset);
            tmpHdr->sh_offset = swap32(thisRel.elfSectionTable[i].offset); // the file begin offset will be added later
            tmpHdr->sh_size = swap32(thisRel.elfSectionTable[i].secLength);
        }
    }
    outELF->secNames = secNameBuf;
    outELF->secHdrs = secHeaders;

    return outELF;
}

void writeElf(elfFile *elf, FILE *file) {
    u8 *buf = malloc(swap32(elf->prgHdr->p_filesz));
    fseek(file, 0x40, SEEK_SET); // program header
    memcpy(buf, &(elf->prgHdr), sizeof(elfPrgHeader));
    fwrite(buf, sizeof(elfPrgHeader), 1, file);

    u32 dataBegin = align(ftell(file));
    fseek(file, dataBegin, SEEK_SET);
    memcpy(buf, elf->sectionData, swap32(elf->prgHdr->p_filesz));
    fwrite(buf, sizeof(u8), swap32(elf->prgHdr->p_filesz), file);
    
    u32 secNameStart = align(ftell(file));
    fseek(file, secNameStart, SEEK_SET);
    // section names
    char secNameBuf[0x400];
    int len = 0; // start after the null header
    for (int i = 0; i < swap16(elf->hdr.e_shnum); i++) {
        DEBUG_PRINT(("section name -> %s\n", elf->secNames[i]));
        elf->secHdrs[i].sh_name = swap32(len);
        strcpy(secNameBuf + len, elf->secNames[i]);
        len += strlen(elf->secNames[i]) + 1;
    }
    elf->secHdrs[swap16(elf->hdr.e_shnum) - 1].sh_offset = swap32(secNameStart);
    elf->secHdrs[swap16(elf->hdr.e_shnum) - 1].sh_size = swap32(len);
    fwrite(secNameBuf, sizeof(char), len, file);

    u32 sectionHdrBegin = align(ftell(file));
    fseek(file, sectionHdrBegin, SEEK_SET);
    for (int i = 0; i < swap16(elf->hdr.e_shnum); i++) {
        if (i > 0 && i < swap16(elf->hdr.e_shnum) - 1 && elf->secHdrs[i].sh_offset > 0) {
            elf->secHdrs[i].sh_offset = swap32(swap32(elf->secHdrs[i].sh_offset) + dataBegin);
        }
        memcpy(buf, &(elf->secHdrs[i]), sizeof(elfSecHeader));
        fwrite(buf, sizeof(elfSecHeader), 1, file);
    }

    elf->hdr.e_shoff = swap32(sectionHdrBegin);

    // we saved the header for last since some information can only be determined
    // after most things are serialized already
    fseek(file, 0, SEEK_SET);
    memcpy(buf, &(elf->hdr), sizeof(elfHeader));
    fwrite(buf, sizeof(elfHeader), 1, file);
    free(buf);
}

void relocateRelFile(relFile *rel, int index) {
    relFile *thisRel = &(rel[index]);
    DEBUG_PRINT(("applying relocations to module %d...\n", swap32(thisRel->hdr.id)));
    u32 currRelocPointer = 0;
    u32 thisRelocStart = relocOffsets[swap32(thisRel->hdr.id)];
    section *secInfoTable = thisRel->sectionTable;

    for (u32 i = 0; i < swap32(thisRel->hdr.impSize / sizeof(imp)); i++) {
        imp currImp = thisRel->impTable[i];
        DEBUG_PRINT(("module number %d...\n", currImp.moduleNum));
        index = 0;
        while (1) {
            reloc currReloc = thisRel->relocs[currImp.moduleNum][index];
            
            currRelocPointer += currReloc.offset;

            //DEBUG_PRINT(("|%03d|0x%04x|0x%02x|0x%08x|0x%04x\n", currReloc.type, currReloc.offset, currReloc.section, currReloc.addend, currRelocPointer));
            if (currReloc.type == R_DOLPHIN_END) {
                DEBUG_PRINT(("Found R_DOLPHIN_END, stopping.\n"));
                break;
            }
            
            if (currReloc.type == R_DOLPHIN_SECTION) {
                currRelocPointer = secInfoTable[currReloc.section].offset;
                DEBUG_PRINT(("using section %d -> %08x\n", currReloc.section, currRelocPointer));
            }
            

            if (currReloc.type < R_DOLPHIN_NOP) {
                u32 relocSymbol = relocOffsets[currImp.moduleNum] + currReloc.addend;
                if (currImp.moduleNum > 0) {
                    relocSymbol += rel[currImp.moduleNum].sectionTable[currReloc.section].offset;
                }
                u32 valToWrite;
                switch (currReloc.type) {
                    case R_PPC_ADDR32:
                        *(u32 *)(&thisRel->data[currRelocPointer]) = swap32(relocSymbol);
                        break;
                    case R_PPC_ADDR24:
                        *(u32 *)(&thisRel->data[currRelocPointer]) &= swap32(0xFC000003);
                        *(u32 *)(&thisRel->data[currRelocPointer]) |= swap32(((relocSymbol >> 2) << 2) & 0x3FFFFFC);
                        break;
                    case R_PPC_ADDR16:
                        if (relocSymbol >> 16 != 0) break;
                        *(u16 *)(&thisRel->data[currRelocPointer]) = swap16(relocSymbol);
                        break;
                    case R_PPC_ADDR16_LO:
                        *(u16 *)(&thisRel->data[currRelocPointer]) = swap16(relocSymbol & 0xFFFF);
                        break;
                    case R_PPC_ADDR16_HI:
                        *(u16 *)(&thisRel->data[currRelocPointer]) = swap16((relocSymbol >> 16) & 0xFFFF);
                        break;
                    case R_PPC_ADDR16_HA:
                        valToWrite = ((relocSymbol >> 16) + ((relocSymbol & 0x8000) ? 1 : 0)) & 0xFFFF;
                        *(u16 *)(&thisRel->data[currRelocPointer]) = swap16(valToWrite);
                        break;
                    case R_PPC_ADDR14:
                    case R_PPC_ADDR14_BRTAKEN:
                    case R_PPC_ADDR14_BRNTAKEN:
                        *(u32 *)(&thisRel->data[currRelocPointer]) &= swap32(0xFFFF0003);
                        *(u32 *)(&thisRel->data[currRelocPointer]) = swap32(((relocSymbol >> 2) << 2) & 0xFFFC);
                        break;
                    case R_PPC_REL24:
                        valToWrite = ((relocSymbol - (thisRelocStart + currRelocPointer)) >> 2) << 2;
                        *(u32 *)(&thisRel->data[currRelocPointer]) &= swap32(0xFC000003);
                        *(u32 *)(&thisRel->data[currRelocPointer]) |= swap32(valToWrite & 0x3FFFFFC);
                        break;
                    case R_PPC_REL14:
                    case R_PPC_REL14_BRTAKEN:
                    case R_PPC_REL14_BRNTAKEN:
                        valToWrite = ((thisRelocStart + currRelocPointer - relocSymbol) >> 2) << 2;
                        *(u32 *)(&thisRel->data[currRelocPointer]) &= swap32(0xFFFF0003);
                        *(u32 *)(&thisRel->data[currRelocPointer]) = swap32(valToWrite & 0xFFFC);
                        break;
                    default:
                        printf("unknown relocation type %d\n", currReloc.type);
                }
            }

            index++;
        }
    }
}
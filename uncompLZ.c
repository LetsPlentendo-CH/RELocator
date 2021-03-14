// Part of RELocator, written by RootCubed 2020

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define u8 unsigned char
#define u16 unsigned short
#define u32 unsigned int

#define s8 char
#define s16 short
#define s32 int

u8 *fileBuffer;
u8 *outBuffer;

void UncompressLZ(u8*, u8*, u32);

int main(int argc, char ** argv) {
    if (strcmp(argv[1], "") == 0 || strcmp(argv[2], "") == 0) {
        printf("Please specify an input and an output file.\n");
        return -1;
    }
    printf("opening %s\n", argv[1]);
    FILE *relFile = fopen(argv[1], "rb");
    fseek(relFile, 0, SEEK_END);
    long size = ftell(relFile);
    printf("size 0x%04lx\n", size);
    rewind(relFile);
    fileBuffer = (u8 *) malloc(size);
    fread(fileBuffer, 1, size, relFile);
    fclose(relFile);
    // uncompressed size
    u32 uncompSize = (*(u32*) fileBuffer) >> 8;
    printf("uncompressed 0x%04x\n", uncompSize);
    outBuffer = (u8 *) malloc(uncompSize);

    UncompressLZ(fileBuffer, outBuffer, size);
    free(fileBuffer);
    printf("Done.\n\n");
    FILE *outFile = fopen(argv[2], "wb");
    fwrite(outBuffer, 1, uncompSize, outFile);
    fclose(outFile);
    free(outBuffer);
}

void UncompressLZ(u8* src, u8* dst, u32 regSize) {
    u32 currPos = 4;
    u32 writeTo = 0;
    u32 bytesWritten = 0;

    u32 uncompressedSize = src[1] | (src[2] << 8) | (src[3] << 16);

    while (bytesWritten < uncompressedSize) {
        u8 flags = src[currPos];
        currPos++;
        for (int i = 0; i < 8; i++) {
            if ((flags & 0x80) == 0) { // regular copy
                dst[writeTo] = src[currPos];
                writeTo++;
                currPos++;
                bytesWritten++;
            } else { // compressed! (get from other location)
                u32 duplLength = (src[currPos] >> 4);
                if (duplLength == 1) {
                    duplLength = (src[currPos] & 0xF) << 12;
                    currPos++;
                    duplLength |= (src[currPos] << 4);
                    currPos++;
                    duplLength |= (src[currPos] >> 4);

                    duplLength += 0xFF + 0xF + 3;
                } else if (duplLength == 0) {
                    duplLength |= (src[currPos] << 4);
                    currPos++;
                    duplLength |= (src[currPos] >> 4);

                    duplLength += 0xF + 2;
                } else {
                    duplLength += 1;
                }
                s32 offset = (src[currPos] & 0xF) << 8;
                currPos++;
                offset = (offset | src[currPos]) + 1;
                currPos++;
                u32 pos = writeTo - offset;

                for (int i = 0; i < duplLength; i++) {
                    dst[writeTo + i] = dst[pos + i];
                }
                writeTo += duplLength;
                bytesWritten += duplLength;
            }
            flags <<= 1;
            if (bytesWritten == uncompressedSize) break;
            if (currPos >= regSize) return;
        }
    }
}
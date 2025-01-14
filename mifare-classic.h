#ifndef MIFARE_CLASSIC_H
#define MIFARE_CLASSIC_H

#ifndef COMMON_H
#include "common.h"
#endif

BOOL mifare_classic_get_sector_bytes(LONG sector);

extern const BYTE sectorBlocks[16];
extern const BYTE KEY_A_DEFAULT[6];
extern const BYTE KEY_B_DEFAULT[6];
extern const BYTE KEY_A_NDEF_SECTOR0[6];
extern const BYTE KEY_B_NDEF_SECTOR0[6];
extern const BYTE KEY_A_NDEF_SECTOR115[6];
extern const BYTE KEY_B_NDEF_SECTOR115[6];
extern const BYTE ACCESS_BITS_UNINITALIZED[];
extern const BYTE ACCESS_BITS_NDEF_SECTOR0[];
extern const BYTE ACCESS_BITS_NDEF_SECTOR115[];

#endif

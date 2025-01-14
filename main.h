#ifndef MAIN_H
#define MAIN_H

#ifndef COMMON_H
#include "common.h"
#endif

// general functions
LONG getAvailableReaders(SCARDCONTEXT hContext, char *mszReaders, DWORD *dwReaders);
LONG connectToReader(SCARDCONTEXT hContext, const char *reader, SCARDHANDLE *hCard, DWORD *dwActiveProtocol, BOOL directConnect);
LONG executeApdu(SCARDHANDLE hCard, BYTE *pbSendBuffer, DWORD dwSendLength, BYTE *pbRecvBuffer, DWORD *dwRecvLength);
LONG disableBuzzer(SCARDCONTEXT hContext, const char *reader, SCARDHANDLE *hCard, DWORD *dwActiveProtocol, BYTE *pbRecvBuffer, DWORD *dwRecvLength);
void disconnectReader(SCARDHANDLE hCard, SCARDCONTEXT hContext);

// interact with tags
LONG getUID(SCARDHANDLE hCard, BYTE *pbRecvBuffer, DWORD *dwRecvLength, BOOL printResult);

// helper functions
BOOL containsSubstring(const char *string, const char *substring);
void printHex(LPCBYTE pbData, DWORD cbData);

#endif
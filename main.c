// Tested on macOS Sequoia, windows 10, windows 11

// windows instructions:
    // 0. install the windows sdk (e.g. via visual studio or via download)
    // 1. install latest llvm to get clang (https://github.com/llvm/llvm-project/releases/latest)
    // 2. Compile with (you might need to replace paths): clang.exe *.c -I"C:\Program Files (x86)\Windows Kits\10\Include\10.0.22621.0\um" -I"C:\Program Files (x86)\Windows Kits\10\Include\10.0.22621.0\shared" -L"C:\Program Files (x86)\Windows Kits\10\Include\10.0.22621.0\um\x64" -lwinscard -o main.exe
    // 3. Run with: main
// macOS instructions:
    // 0. There is no need to install the official drivers for 122U (neither for windows nor for macOS)
    // 1. Ensure pcscd is running: sudo launchctl enable system/com.apple.pcscd , then reboot
    // 2. Compile with: make
    // 3. Run with: ./main

#include "main.h"
//#include "mifare-classic.h"


LONG getAvailableReaders(SCARDCONTEXT hContext, char *mszReaders, DWORD *dwReaders) {
    LONG lRet = SCardListReaders(hContext, NULL, mszReaders, dwReaders);
    if (lRet != SCARD_S_SUCCESS) {
        if (lRet == (int32_t)0x8010002E) {
            printf("Failed to list readers: Are you sure your smart card reader is connected and turned on?\n");
        } else {
            printf("Failed to list readers: 0x%X\n", (unsigned int)lRet);
        }
    }

    return lRet;
}

LONG connectToReader(SCARDCONTEXT hContext, const char *reader, SCARDHANDLE *hCard, DWORD *dwActiveProtocol, BOOL directConnect) {
    LONG lRet;

    if (directConnect) {
        // direct communication with reader (no present tag required)
        lRet = SCardConnect(hContext, reader, SCARD_SHARE_DIRECT, SCARD_PROTOCOL_T1, hCard, dwActiveProtocol);
    } else {
        // T1 = block transmission (works), T0 = character transmission (did not work when testing), Tx = T0 | T1 (works)
        // https://learn.microsoft.com/en-us/openspecs/windows_protocols/ms-rdpesc/41673567-2710-4e86-be87-7b6f46fe10af
        lRet = SCardConnect(hContext, reader, SCARD_SHARE_SHARED, SCARD_PROTOCOL_T1, hCard, dwActiveProtocol);
    }

    return lRet;
}

LONG executeApdu(SCARDHANDLE hCard, BYTE *pbSendBuffer, DWORD dwSendLength, BYTE *pbRecvBuffer, DWORD *dwRecvLength) {  
    LONG lRet = SCardTransmit(hCard, SCARD_PCI_T1, pbSendBuffer, dwSendLength, NULL, pbRecvBuffer, dwRecvLength);
    // also print reply
    if (lRet == SCARD_S_SUCCESS) {
        // print which command you sent
        printf("> ");
        printHex(pbSendBuffer, dwSendLength);

        // print what you received
        printf("< ");
        printHex(pbRecvBuffer, *dwRecvLength);

    } else {
        printf("%08x\n", lRet);
    }   

    return lRet;
}

LONG disableBuzzer(SCARDCONTEXT hContext, const char *reader, SCARDHANDLE *hCard, DWORD *dwActiveProtocol, BYTE *pbRecvBuffer, DWORD *dwRecvLength) {
    LONG lRet = connectToReader(hContext, reader, hCard, dwActiveProtocol, TRUE);
    if (lRet != SCARD_S_SUCCESS) {
        printf("Failed to connect: 0x%x\n", (unsigned int)lRet);
        return 1;
    }


    // Define ACR122U command to disable the buzzer sound (really annoying sound) [section: 'Set buzzer output during card detection']
    BYTE pbSendBuffer[] = { 0xFF, 0x00, 0x52, 0x00, 0x00 };
    DWORD cbRecvLength = 16;

    LONG result = SCardControl(*hCard, SCARD_CTL_CODE(3500), pbSendBuffer, sizeof(pbSendBuffer), pbRecvBuffer, *dwRecvLength, &cbRecvLength); //  3500 escape code defined by microsoft, 2079 escape code defined by ACS, i tried both and only 3500 works on every OS

    return result;
}

void disconnectReader(SCARDHANDLE hCard, SCARDCONTEXT hContext) {
    SCardDisconnect(hCard, SCARD_LEAVE_CARD);
    SCardReleaseContext(hContext);
}

// -------------------- General Functions that interact with various tags ----------------------------------

LONG getUID(SCARDHANDLE hCard, BYTE *pbRecvBuffer, DWORD *dwRecvLength, BOOL printResult) {
    // Define the GET UID APDU command (PC/SC standard for many cards)
    BYTE pbSendBuffer[] = { 0xFF, 0xCA, 0x00, 0x00, 0x00 };
    LONG lRet = executeApdu(hCard, pbSendBuffer, sizeof(pbSendBuffer), pbRecvBuffer, dwRecvLength);

    // ensure 90 00 success is returned by acr122u ()
    if (!((pbRecvBuffer[4] == 0x90 && pbRecvBuffer[5] == 0x00) ||   // UID of length 4: single UID
         (pbRecvBuffer[7] == 0x90 && pbRecvBuffer[8] == 0x00)  ||   // UID of length 7: double UID
         (pbRecvBuffer[10] == 0x90 && pbRecvBuffer[11] == 0x00)     // UID of length 10: oh baby a triple oh yeah UID
         )) {      
        lRet = ACR_90_00_FAILURE;
    }


    if ((lRet == SCARD_S_SUCCESS) && printResult) {
        // success: now print UID
        printf("UID of detected NFC tag: ");
        for (DWORD i = 0; i < *dwRecvLength - 2; i++) { // -2 because no need to print success code 90 00
            printf("%02X ", pbRecvBuffer[i]);
        }
        printf("\n");
    }

    return lRet;
}

// supports 14443A and 14443B-3/4
LONG getATS_14443A(SCARDHANDLE hCard, BYTE *pbRecvBuffer, DWORD *dwRecvLength) {
    BYTE pbSendBuffer[] = { 0xFF, 0xCA, 0x01, 0x00, 0x00 };
    LONG lRet = executeApdu(hCard, pbSendBuffer, sizeof(pbSendBuffer), pbRecvBuffer, dwRecvLength);

    if ((pbRecvBuffer[0] == 0x6A) && (pbRecvBuffer[1] == 0x81)) {
        printf("Accessing ATS of this tag is not supported!\n");
    }

    return lRet;
}

LONG getStatus(SCARDHANDLE *hCard, char *mszReaders, DWORD dwState, DWORD dwReaders, DWORD *dwActiveProtocol, BYTE *pbRecvBuffer, DWORD *dwRecvLength, BOOL printResult) {
    LONG lRet = SCardStatus(*hCard, mszReaders, &dwReaders, &dwState, dwActiveProtocol, pbRecvBuffer, dwRecvLength);
    if ((lRet == SCARD_S_SUCCESS) && printResult) {
        // success: now print status
        printf("Status of detected NFC tag: ");
        for (DWORD i = 0; i < *dwRecvLength - 2; i++) { // -2 because no need to print success code 90 00
            printf("%02X ", pbRecvBuffer[i]);
        }
        printf("\n");

        if (sizeof(pbRecvBuffer) < 8) return 1;

        // determine which kind of tag this is
        BYTE tagIdentifyingByte1 = pbRecvBuffer[13];
        BYTE tagIdentifyingByte2 = pbRecvBuffer[14];
        if ((tagIdentifyingByte1 == 0x00) &&  (tagIdentifyingByte2 == 0x01)) {
            printf("Identified tag as: Mifare Classic 1k\n");
        } else if ((pbRecvBuffer[13] == 0x00) && (pbRecvBuffer[14] == 0x02)) {
            printf("Identified tag as: Mifare Classic 4k\n");
        } else if ((pbRecvBuffer[13] == 0x00) && (pbRecvBuffer[14] == 0x03)) {
            printf("Identified tag as: Mifare Ultralight or NTAG2xx\n");
        } else if ((pbRecvBuffer[13] == 0x00) && (pbRecvBuffer[14] == 0x26)) {
            printf("Identified tag as: Mifare Mini\n");
        } else if ((pbRecvBuffer[13] == 0xF0) && (pbRecvBuffer[14] == 0x04)) {
            printf("Identified tag as: Topaz/Jewel\n");
        } else if ((pbRecvBuffer[13] == 0xF0) && (pbRecvBuffer[14] == 0x11)) {
            printf("Identified tag as: FeliCa 212K\n");
        } else if ((pbRecvBuffer[13] == 0xF0) && (pbRecvBuffer[14] == 0x12)) {
            printf("Identified tag as: FeliCa 424K\n");
        } else if (pbRecvBuffer[13] == 0xFF) {
            printf("Identified tag as: UNKNOWN TAG\n");
        } else {
            printf("Failed to identify the tag\n");
        }
    }


    return lRet;
}


// -------------------- Helper functions -------------------------------------------------------
BOOL containsSubstring(const char *string, const char *substring) {
    // Edge case: if substring is empty, it's always considered a match
    if (*substring == '\0') {
        return TRUE;
    }

    // Iterate through the string
    for (const char *s = string; *s != '\0'; ++s) {
        // Check if the current character matches the first character of substring
        if (*s == *substring) {
            const char *str_ptr = s;
            const char *sub_ptr = substring;

            // Compare the substring starting from here
            while (*sub_ptr != '\0' && *str_ptr == *sub_ptr) {
                str_ptr++;
                sub_ptr++;
            }

            // If the entire substring matches, return 1 (true)
            if (*sub_ptr == '\0') {
                return TRUE;
            }
        }
    }

    // Return 0 (false) if the substring was not found
    return FALSE;
}

// printHex prints bytes as hex
void printHex(LPCBYTE pbData, DWORD cbData) {
    for (DWORD i = 0; i < cbData; i++) {
        printf("%02x ", pbData[i]);
    }
    printf("\n");
}

// -------------------------------------------------------

int main(void) {
    SCARDCONTEXT hContext;
    SCARDHANDLE hCard = 0;
    DWORD dwActiveProtocol;
    char mszReaders[1024];
    DWORD dwReaders = sizeof(mszReaders);
    LONG lRet;

    BYTE pbRecvBuffer[256];
    DWORD dwRecvLength = sizeof(pbRecvBuffer);

    BYTE pbRecvBufferLarge[2048];
    DWORD dwRecvLengthLarge = sizeof(pbRecvBufferLarge);
    DWORD dwState = SCARD_POWERED; // TODO: can u dynamically request the actual state somehow?

    // Establish context
    lRet = SCardEstablishContext(SCARD_SCOPE_SYSTEM, NULL, NULL, &hContext);
    if (lRet != SCARD_S_SUCCESS) {
        printf("Failed to establish context: 0x%X\n", (unsigned int)lRet);
        return 1;
    }

    // Get available readers (you might have multiple smart card readers connected)
    lRet = getAvailableReaders(hContext, mszReaders, &dwReaders);
    if (lRet != SCARD_S_SUCCESS) {
        SCardReleaseContext(hContext);
        return 1;
    }

    // Print connected readers and select the first one
    printf("Available Smart Card Readers:\n");
    char *reader = mszReaders;
    if (*reader == '\0') {
        printf("No readers found.\n");
        SCardReleaseContext(hContext);
        return 1;
    }
    printf("- %s\n", reader);
    // ensure you connected to ACR122U, this code is only tested with that reader
    const char *substring = "ACR122";
    BOOL isCorrectReader = containsSubstring(reader, substring);
    if (!isCorrectReader) {
        printf("Your reader does not seem to be an ACR122U, cancelling program execution!");
        disconnectReader(hCard, hContext);
        return 1;
    }

    // Turn off buzzer of ACR122U
    lRet = disableBuzzer(hContext, reader, &hCard, &dwActiveProtocol, pbRecvBuffer, &dwRecvLength);
    if (lRet == 0) {
        printf("Disabled buzzer of reader\n");
        SCardDisconnect(hCard, SCARD_LEAVE_CARD);
    } else {
        if (lRet == (int32_t)0x80100016) {
            printf("Failed to disable buzzer of ACR122U: SCARD_E_NOT_TRANSACTED - An attempt was made to end a nonexistent transaction.\n");
        } else {
            printf("Failed to disable buzzer of ACR122U: 0x%x\n", (unsigned int)lRet);
        }
        
    }

    // Connect to the first reader
    lRet = connectToReader(hContext, reader, &hCard, &dwActiveProtocol, FALSE);
    BOOL didPrintWarningAlready = FALSE;
    while (lRet != SCARD_S_SUCCESS) {
        if ((lRet == (int32_t)SCARD_E_NO_SMARTCARD) || (lRet == (int32_t)SCARD_W_REMOVED_CARD)) {
            if (!didPrintWarningAlready) {
                printf("Failed to connect: Please now hold an NFC tag near the reader...\n");
                didPrintWarningAlready = TRUE;
            }
        } else if (lRet == (int32_t)SCARD_E_TIMEOUT) {
            if (!didPrintWarningAlready) {
                printf("Failed to connect: Please now hold an NFC tag near the reader...\n");
                didPrintWarningAlready = TRUE;
            }
        } else {
            if (!didPrintWarningAlready) {
                printf("Failed to connect: 0x%x\n", (unsigned int)lRet);
                didPrintWarningAlready = TRUE;
            }
        }
        
        // wait a bit and retry (maybe user is not holding a tag near the reader yet)
        SLEEP(50); // milliseconds (don't put this value too low, it never worked for me with 1 ms)
        lRet = connectToReader(hContext, reader, &hCard, &dwActiveProtocol, FALSE);
    }
    printf("Connected to reader: %s\nDetected NFC tag\n", reader);

    // -------------- Interact with tag ---------------------------

    // Get UID of detected tag
    lRet = getUID(hCard, pbRecvBuffer, &dwRecvLength, TRUE);
    if (lRet != SCARD_S_SUCCESS) {
        if (!(lRet == ACR_90_00_FAILURE)) {
            printf("Failed to get UID of tag: 0x%x\n", (unsigned int)lRet);
        } else {
            printf("Failed to get UID of tag: ACR122U did not return the expected 90 00 return code!\n");
        }
        
        disconnectReader(hCard, hContext);
        return 1;
    }


    lRet = getATS_14443A(hCard, pbRecvBuffer, &dwRecvLength);
    if (lRet != SCARD_S_SUCCESS) {
        printf("Failed to get ATR of tag: 0x%x\n", (unsigned int)lRet);
        disconnectReader(hCard, hContext);
        return 1;
    }

    lRet = getStatus(&hCard, mszReaders, dwState, dwReaders, &dwActiveProtocol, pbRecvBufferLarge, &dwRecvLengthLarge, TRUE);
    if (lRet != SCARD_S_SUCCESS) {
        printf("Failed to get status of tag: 0x%x\n", (unsigned int)lRet);
        disconnectReader(hCard, hContext);
        return 1;
    }

    // TODO: why does the ATS thing never work?
    // TODO: why can't it distinguish between Ultralight and NTAG?

    // -------------- End of: Interact with tag --------------------

    // Clean up
    disconnectReader(hCard, hContext);
    return 0;
}

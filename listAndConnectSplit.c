// Tested on macOS Sequoia, windows 10, windows 11

// windows instructions:
	// 0. install the windows sdk (e.g. via visual studio or via download)
	// 1. install latest llvm to get clang (https://github.com/llvm/llvm-project/releases/latest)
	// 2. Compile with (you might need to replace paths): clang.exe listAndConnectSplit.c -I"C:\Program Files (x86)\Windows Kits\10\Include\10.0.22621.0\um" -I"C:\Program Files (x86)\Windows Kits\10\Include\10.0.22621.0\shared" -L"C:\Program Files (x86)\Windows Kits\10\Include\10.0.22621.0\um\x64" -lwinscard -o listAndConnectSplit.exe

// macOS instructions:
	// 0. There is no need to install the official drivers for 122U (neither for windows nor for macOS)
	// 1. Ensure pcscd is running: sudo launchctl enable system/com.apple.pcscd , then reboot
	// 2. Compile with: clang listAndConnectSplit.c -framework PCSC -o listAndConnectSplit
	// 3. Run with: ./listAndConnect

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// _WIN64 also is defined as _WIN32
#ifdef _WIN32
#include <stdint.h>
#include <windows.h> // contains Sleep
#include <winscard.h>
#include <wtypes.h>
#define SLEEP(milliseconds) Sleep(milliseconds)

#elif __APPLE__
#include <PCSC/winscard.h>
#include <PCSC/wintypes.h>

// Nice cross-platform Sleep doesn't exist, instead call usleep and convert microsecond result into milliseconds to match window's Sleep()
#include <unistd.h> // contains usleep
#define SLEEP(ms) usleep((ms) * 1000)

#ifndef FALSE
#define FALSE 0
#endif

#ifndef TRUE
#define TRUE 1
#endif

#ifndef LONG
#define LONG int32_t
#endif

#ifndef DWORD
#define DWORD uint32_t
#endif

#ifndef SCARD_PROTOCOL_Tx
#define SCARD_PROTOCOL_Tx SCARD_PROTOCOL_T0
#endif

#ifndef SCARD_CTL_CODE
#define SCARD_CTL_CODE(code) (0x42000000 + (code)) // https://web.archive.org/web/20171027125417/https://pcsclite.alioth.debian.org/api/reader_8h.html
#endif

#endif

// general functions
LONG getAvailableReaders(SCARDCONTEXT hContext, char *mszReaders, DWORD *dwReaders);
LONG connectToReader(SCARDCONTEXT hContext, const char *reader, SCARDHANDLE *hCard, DWORD *dwActiveProtocol);
LONG executeApdu(SCARDHANDLE hCard, BYTE *pbSendBuffer, DWORD dwSendLength, BYTE *pbRecvBuffer, DWORD *dwRecvLength);
LONG disableBuzzer(SCARDHANDLE hCard);
void disconnectReader(SCARDHANDLE hCard, SCARDCONTEXT hContext);

// interact with tags
LONG readUID(SCARDHANDLE hCard, BYTE *pbRecvBuffer, DWORD *dwRecvLength);

// helper functions
BOOL containsSubstring(const char *string, const char *substring);

// --------------------------------------- Functions --------------------------------------

LONG getAvailableReaders(SCARDCONTEXT hContext, char *mszReaders, DWORD *dwReaders) {
    LONG lRet = SCardListReaders(hContext, NULL, mszReaders, dwReaders);
    if (lRet != SCARD_S_SUCCESS) {
        printf("Failed to list readers: 0x%X\n", (unsigned int)lRet);
    }
    return lRet;
}

LONG connectToReader(SCARDCONTEXT hContext, const char *reader, SCARDHANDLE *hCard, DWORD *dwActiveProtocol) {
    LONG lRet = SCardConnect(hContext, reader, SCARD_SHARE_SHARED, SCARD_PROTOCOL_T0 | SCARD_PROTOCOL_T1, hCard, dwActiveProtocol);

    return lRet;
}

LONG executeApdu(SCARDHANDLE hCard, BYTE *pbSendBuffer, DWORD dwSendLength, BYTE *pbRecvBuffer, DWORD *dwRecvLength) {  
    LONG lRet = SCardTransmit(hCard, SCARD_PCI_T1, pbSendBuffer, dwSendLength, NULL, pbRecvBuffer, dwRecvLength);
    return lRet;
}

LONG disableBuzzer(SCARDHANDLE hCard) {
    // Ensure the card is connected and active
    if (hCard == 0) {
        printf("Reader not connected anymore!\n");
        return SCARD_E_NOT_TRANSACTED;
    }

    // Define ACR122U command to disable the buzzer sound (really annoying sound)
    BYTE pbSendBuffer[] = { 0xFF, 0x00, 0x52, 0x00, 0x00 };
    DWORD cbRecvLength = 7;

    #ifdef _WIN32
    LONG result = SCardControl(hCard, SCARD_CTL_CODE(3500), pbSendBuffer, sizeof(pbSendBuffer), NULL, 0, &cbRecvLength); //  3500 escape code defined by microsoft
    #elif __APPLE__
    LONG result = SCardControl(hCard, SCARD_CTL_CODE(2079), pbSendBuffer, sizeof(pbSendBuffer), NULL, 0, &cbRecvLength); //  2079 escape code defined by ACS
    // Problem: macOS either pretends it did work but it didn't actually disable the buzzer OR it throws SCARD_E_NOT_TRANSACTED (tried both 2079 and 3500)
    #endif

    return result;
}

void disconnectReader(SCARDHANDLE hCard, SCARDCONTEXT hContext) {
    SCardDisconnect(hCard, SCARD_LEAVE_CARD);
    SCardReleaseContext(hContext);
}

// -------------------- Functions that interact with NFC tags ----------------------------------

LONG readUID(SCARDHANDLE hCard, BYTE *pbRecvBuffer, DWORD *dwRecvLength) {
    // Define the GET UID APDU command (PC/SC standard for many cards)
    BYTE pbSendBuffer[] = { 0xFF, 0xCA, 0x00, 0x00, 0x00 };
    return executeApdu(hCard, pbSendBuffer, sizeof(pbSendBuffer), pbRecvBuffer, dwRecvLength);
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

int main(void) {
    SCARDCONTEXT hContext;
    SCARDHANDLE hCard;
    DWORD dwActiveProtocol;
    char mszReaders[1024];
    DWORD dwReaders = sizeof(mszReaders);
    LONG lRet;

    BYTE pbRecvBuffer[256];
    DWORD dwRecvLength = sizeof(pbRecvBuffer);

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

    // Connect to the first reader
    lRet = connectToReader(hContext, reader, &hCard, &dwActiveProtocol);
    BOOL didPrintWarningAlready = FALSE;
    while (lRet != SCARD_S_SUCCESS) {
        if ((lRet == SCARD_E_NO_SMARTCARD) || (lRet == SCARD_W_REMOVED_CARD)) {
            if (!didPrintWarningAlready) {
                printf("Failed to connect: Please now hold an NFC tag near the reader...\n");
                didPrintWarningAlready = TRUE;
            }
        } else if (lRet == SCARD_E_TIMEOUT) {
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
        lRet = connectToReader(hContext, reader, &hCard, &dwActiveProtocol);
    }
    printf("Connected to reader: %s\nDetected NFC tag\n", reader);

    // Turn off buzzer of ACR122U (i could not get this to work on macOS, i always get: SCARD_E_NOT_TRANSACTED)
    //#ifdef _WIN32
    lRet = disableBuzzer(hCard);
    if (lRet == 0) {
        printf("Disabled buzzer of reader\n");
    } else {
        if (lRet == 0x80100016) {
            printf("Failed to disable buzzer of ACR122U: SCARD_E_NOT_TRANSACTED - An attempt was made to end a nonexistent transaction.\n");
        } else {
            printf("Failed to disable buzzer of ACR122U: 0x%x\n", (unsigned int)lRet);
        }
        
    }
    //#endif
    
    // -------------- Interact with tag ---------------------------

    // Get UID of detected tag
    lRet = readUID(hCard, pbRecvBuffer, &dwRecvLength);
    if (lRet != SCARD_S_SUCCESS) {
        printf("Failed to get UID of tag: 0x%x\n", (unsigned int)lRet);
        disconnectReader(hCard, hContext);
        return 1;
    }

    printf("UID of detected NFC tag: ");
    for (DWORD i = 0; i < dwRecvLength - 2; i++) { // -2 because no need to print success code 90 00
        printf("%02X ", pbRecvBuffer[i]);
    }
    printf("\n");
    
    // -------------- End of: Interact with tag --------------------

    // Clean up
    disconnectReader(hCard, hContext);
    return 0;
}

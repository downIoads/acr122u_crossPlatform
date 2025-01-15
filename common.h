#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef ACR_90_00_FAILURE
#define ACR_90_00_FAILURE ((LONG)0x13371337) // made up to signal that expected 90 00 was not returned by the reader
#endif

// _WIN64 also is defined as _WIN32
#ifdef _WIN32
#include <stdint.h>
#include <windows.h> // contains Sleep
#include <winscard.h>
#include <wtypes.h>
#define SLEEP(milliseconds) Sleep(milliseconds)

#define LONG int32_t // for some reason on windows clang wants me to use "08lx" and on macOS "08x"


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

#ifndef BYTE
#define BYTE unsigned char
#endif

#ifndef DWORD
#define DWORD uint32_t
#endif

#ifndef LONG
#define LONG int32_t
#endif


#ifndef SCARD_CTL_CODE
#define SCARD_CTL_CODE(code) (0x42000000 + (code)) // https://web.archive.org/web/20171027125417/https://pcsclite.alioth.debian.org/api/reader_8h.html
#endif

#endif // __APPLE__

#endif // COMMON_H

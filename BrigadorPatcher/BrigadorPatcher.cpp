// BrigadorPatcher.cpp : This file contains the 'main' function. Program execution begins and ends there.
//
#include "windows.h"
#include <iostream>
#include <fstream>
#include <string>

using namespace std;

// Unpatched 
// brigador.exe + 3EEF9 | BB 00000032 | mov ebx, 32000000 |
// brigador.exe + 3EEFE | B8 00000040 | mov eax, 40000000 |
// brigador.exe + 3F1AC | 41:B8 00001000 | mov r8d, 100000 |
// brigador.exe + 3F1F3 | BA 00000002 | mov edx, 2000000 |
// brigador.exe + 3F204 | BA 00000002 | mov edx, 2000000 |
// brigador.exe + 3F20E | B8 00000038 | mov eax, 38000000 |
// brigador.exe + 3F21E | 41 : B8 0000c024 | mov r8d, 24c00000 |
// brigador.exe + 3F3A6 | 41:B8 00000002 | mov r8d, 2000000 |
// brigador.exe + 3F9C6 | 41 : B8 00002000 | mov r8d, 200000 |

// Patched 
// brigador.exe + 3EEF9 | BB 00000064 | mov ebx, 64000000 |
// brigador.exe + 3EEFE | B8 00000080 | mov eax, 80000000 |
// brigador.exe + 3F1AC | 41:B8 00002000 | mov r8d, 200000 |
// brigador.exe + 3F1F3 | BA 00000004 | mov edx, 4000000 |
// brigador.exe + 3F204 | BA 00000004 | mov edx, 4000000 |
// brigador.exe + 3F20E | B8 00000070 | mov eax, 70000000 |
// brigador.exe + 3F21E | 41 : B8 00008049 | mov r8d, 49800000 |
// brigador.exe + 3F3A6 | 41:B8 00000004 | mov r8d, 4000000 |
// brigador.exe + 3F9C6 | 41 : B8 00004000 | mov r8d, 400000 |

#define operationsToPatch 9
#define maxOpBytes 2
#define constantLength 4
#define rdataRawOffset 0x400
#define rdataVirtualOffset 0x1000


uint64_t virtualOffsetsToPatch[operationsToPatch] = {
    0x3EEF9 - rdataVirtualOffset + rdataRawOffset,
    0x3EEFE - rdataVirtualOffset + rdataRawOffset,
    0x3F1AC - rdataVirtualOffset + rdataRawOffset,
    0x3F1F3 - rdataVirtualOffset + rdataRawOffset,
    0x3F204 - rdataVirtualOffset + rdataRawOffset,
    0x3F20E - rdataVirtualOffset + rdataRawOffset,
    0x3F21E - rdataVirtualOffset + rdataRawOffset,
    0x3F3A6 - rdataVirtualOffset + rdataRawOffset,
    0x3F9C6 - rdataVirtualOffset + rdataRawOffset
};

unsigned char opCodes[operationsToPatch][2]{
    {0xBB, 0},
    {0xB8, 0},
    {0x41, 0xB8},
    {0xBA, 0},
    {0xBA, 0},
    {0xB8, 0},
    {0x41, 0xB8},
    {0x41, 0xB8},
    {0x41, 0xB8} 
};

unsigned char opCodeLengths[operationsToPatch]{
    1,
    1,
    2,
    1,
    1,
    1,
    2,
    2,
    2
};

uint32_t defaultConstants[operationsToPatch]{
    0x32000000,
    0x40000000,
    0x00100000,
    0x02000000,
    0x02000000,
    0x38000000,
    0x24c00000,
    0x02000000,
    0x00200000
};

uint32_t patchedConstants[operationsToPatch]{
    defaultConstants[0] * 2,
    defaultConstants[1] * 2,
    defaultConstants[2] * 2,
    defaultConstants[3] * 2,
    defaultConstants[4] * 2,
    defaultConstants[5] * 2,
    defaultConstants[6] * 2,
    defaultConstants[7] * 2,
    defaultConstants[8] * 2,
};


int patcher(uint32_t* expected, uint32_t* desired) {
    unsigned char readBuf[maxOpBytes + constantLength];
    unsigned char expectedBuf[maxOpBytes + constantLength];
    unsigned char desiredBuf[maxOpBytes + constantLength];
    unsigned char curChar;
    unsigned char expectedChar;

    fstream file;
    file.open("./brigador.exe", std::fstream::in | std::fstream::out | std::ios::binary);
    if (file.is_open()) {
        for (int i = 0; i < operationsToPatch; i++) {
            printf("Address: %#016x\n", virtualOffsetsToPatch[i]);
            //Setup expected op
            memcpy(expectedBuf, opCodes[i], opCodeLengths[i]);
            memcpy(expectedBuf + opCodeLengths[i], &expected[i], 4);

            //Check if bytes look as expected
            file.seekg(virtualOffsetsToPatch[i], ios_base::beg);
            file.get((char*)readBuf, opCodeLengths[i] + constantLength + 1);

            for (int n = 0; n < opCodeLengths[i] + constantLength; n++) {
                curChar = readBuf[n];
                expectedChar = expectedBuf[n];
                if (curChar != expectedChar) {
                    printf("\n%d, %#02x, %#02x\n", n, curChar, expectedChar);
                    return (unsigned char)curChar;
                }
                printf("\n%d, %#02x, %#02x\n", n, curChar, expectedChar);
            }
        }
        for (int i = 0; i < operationsToPatch; i++) {
            //Setup new op
            memcpy(desiredBuf, opCodes[i], opCodeLengths[i]);
            memcpy(desiredBuf + opCodeLengths[i], &desired[i], 4);
            //Write bytes
            file.seekg(virtualOffsetsToPatch[i], ios_base::beg);
            file.write((char*)desiredBuf, opCodeLengths[i] + constantLength);
        }
        file.close();
        return 1;
    }
    else {
        return -1;
    }
}

void patcherError(int error) {
    switch (error) {
    case -1:
        cout << "Could not open file. Press ENTER to exit.\n";
        break;
    case -2:
        cout << "Executable contents were not as expected.\n";
        break;
    default:
        //printf("\n%#08x\n", error);
        cout << "Something unexpected occurred. Press ENTER to exit.\n";
        
    }
}
#define BUFSIZE MAX_PATH

int main()
{
    cout << "Brigador Max Packfile Patcher:\n\n";

    //cout << "Start with \"2\" to double the allocated memory, increase it to \"3\", \"4\", etc in case genpack.bat is still crashing\n";
    //cout << "No max is set so be sure you are not exceeding your hardware's capacity and crashing for that reason instead\n";
    //cout << "Multiplier: ";
    while (true) {
        cout << "Input \"0\" to unpatch.\n";
        cout << "Input \"1\" to patch.\n";
        string line;
        getline(cin, line);
        int shouldPatch = stoi(line);
        if (shouldPatch != 0 && shouldPatch != 1) {
            cout << "Invalid input. Try again\n";
            continue;
        }
        else {
            int patchOut;
            if (shouldPatch == 0) {
                patchOut = patcher(patchedConstants, defaultConstants);
                if (patchOut == 1) {
                    cout << "Exececutable unpatched. Press ENTER to exit.\n";
                }
                else {
                    patcherError(patchOut);
                }
            }
            else if (shouldPatch == 1) {
                patchOut = patcher(defaultConstants, patchedConstants);
                if (patchOut == 1) {
                    cout << "Exececutable patched. Press ENTER to exit.\n";
                }
                else {
                    patcherError(patchOut);
                }
            }
            getline(cin, line);
            break;
        }
    }

}

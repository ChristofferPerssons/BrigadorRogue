#include "pch.h"
#include "windows.h"
#include "stdlib.h"
#include "stdio.h"
#include "fstream"
#include "iostream"
#include <string> 
#include "TlHelp32.h"
#include <cstdio>
#include <stdint.h>
#include <Psapi.h>
#include <random>
#include "utils.h"

//#define DEBUG

using namespace std;

int fileToBytes(char* name, char* buffer) {
    // Open file
    ifstream file(name, ios::binary); // and since you want bytes rather than
    // characters, strongly consider opening the
    // File in binary mode with std::ios_base::binary
    // Get length of file
    // Move to the end to determine the file size
    file.seekg(0, ios::end);
    streamsize size = file.tellg();
    file.seekg(0, ios::beg);  // Move back to the beginning

    // Read file
    file.read(buffer, size);

    //ofstream file2("FILEREADCOPY", ios::binary);
    //file2.write(buffer, size);

    file.close();

    return 1;
}

void _SetOtherThreadsSuspended(bool suspend)
{
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (hSnapshot != INVALID_HANDLE_VALUE)
    {
        THREADENTRY32 te;
        te.dwSize = sizeof(THREADENTRY32);
        if (Thread32First(hSnapshot, &te))
        {
            do
            {
                if (te.dwSize >= (FIELD_OFFSET(THREADENTRY32, th32OwnerProcessID) + sizeof(DWORD))
                    && te.th32OwnerProcessID == GetCurrentProcessId()
                    && te.th32ThreadID != GetCurrentThreadId())
                {

                    HANDLE thread = ::OpenThread(THREAD_ALL_ACCESS, FALSE, te.th32ThreadID);
                    if (thread != NULL)
                    {
                        if (suspend)
                        {
                            SuspendThread(thread);
                        }
                        else
                        {
                            ResumeThread(thread);
                        }
                        CloseHandle(thread);
                    }
                }
            } while (Thread32Next(hSnapshot, &te));
        }
    }
}

uint64_t GetBaseModuleForProcess()
{
    HANDLE process = GetCurrentProcess();
    HMODULE processModules[1024];
    DWORD numBytesWrittenInModuleArray = 0;
    EnumProcessModules(process, processModules, sizeof(HMODULE) * 1024, &numBytesWrittenInModuleArray);

    DWORD numRemoteModules = numBytesWrittenInModuleArray / sizeof(HMODULE);
    CHAR processName[256];
    GetModuleFileNameExA(process, NULL, processName, 256); //a null module handle gets the process name
    _strlwr_s(processName, 256);

    HMODULE module = 0; //An HMODULE is the DLL's base address 

    for (DWORD i = 0; i < numRemoteModules; ++i)
    {
        CHAR moduleName[256];
        CHAR absoluteModuleName[256];
        GetModuleFileNameExA(process, processModules[i], moduleName, 256);

        _fullpath(absoluteModuleName, moduleName, 256);
        _strlwr_s(absoluteModuleName, 256);

        if (strcmp(processName, absoluteModuleName) == 0)
        {
            module = processModules[i];
            break;
        }
    }

    return (uint64_t)module;
}

void deployExecutableASM(struct asmHook* asms) {
    char byteBuffer[MAX_PATCH_SIZE];
    char adressBytes[8];
    uint64_t asmSize = asms->fileSize - asms->bytesToStrip;

    //Get asm file bytes
    fileToBytes(asms->asmFilename, byteBuffer);


    //Allocate area for executable code
    DWORD old_protect = 0;
    LPVOID executable_area = VirtualAlloc(NULL, MAX_PATCH_SIZE, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (executable_area == NULL) {
        return;
    }

    asms->hookTarget = (uint64_t)executable_area;

    //Set asm return adress
    uint64_t returnAdress = GetBaseModuleForProcess() + asms->hookStartOffset + asms->overwrittenBytes;
    memcpy(adressBytes, &returnAdress, 8);
    memset(&byteBuffer[asms->fileSize - (8 + 3 + asms->numberOfWritableBytes)], 0x00, 8);
    memcpy(&byteBuffer[asms->fileSize - (8 + 3 + asms->numberOfWritableBytes)], adressBytes, 8);

    //Set asm adresses that require base addresses for absolute jumps
    uint64_t replacementAddress = 0;
    for (int i = 0; asms->addressesToReplaceInASM[i] != 0 || i >= sizeof(asms->addressesToReplaceInASM); i++) {
        replacementAddress = GetBaseModuleForProcess() + asms->replacementOffsets[i];
        memcpy(adressBytes, &replacementAddress, 8);
        memset(&byteBuffer[asms->addressesToReplaceInASM[i]], 0x00, 8);
        memcpy(&byteBuffer[asms->addressesToReplaceInASM[i]], adressBytes, 8);
    }

    for (int i = 0; asms->externalAddressesToReplaceInASM[i] != 0 || i >= sizeof(asms->externalAddressesToReplaceInASM); i++) {
        replacementAddress = asms->externalReplacementValues[i];
        memcpy(adressBytes, &replacementAddress, 8);
        memset(&byteBuffer[asms->externalAddressesToReplaceInASM[i]], 0x00, 8);
        memcpy(&byteBuffer[asms->externalAddressesToReplaceInASM[i]], adressBytes, 8);
    }

    //Write executable asm code
    memcpy((void*)(asms->hookTarget), &(byteBuffer[asms->bytesToStrip]), asmSize);

    //Get hook adress bytes
    memcpy(adressBytes, &asms->hookTarget, 8);
    //char* adressBytes2 = (char*)&executable_area;

    //Construct hook jump 
    char jmpOverwrite[] = JMP64R11;
    memcpy(jmpOverwrite + 2, &asms->hookTarget, sizeof(asms->hookTarget));

    //Insert hook jmp
    DWORD old_protect2;
    void* hookAdress = (void*)(GetBaseModuleForProcess() + asms->hookStartOffset);

    VirtualProtect(hookAdress, MAX_PATCH_SIZE, PAGE_EXECUTE_READWRITE, &old_protect2);
    memset(hookAdress, 0x90, asms->overwrittenBytes);

    memcpy(hookAdress, jmpOverwrite, sizeof(jmpOverwrite));

    ofstream debugFile("DebugFile", ios::binary);
    debugFile.write(&(byteBuffer[asms->bytesToStrip]), asmSize);
    debugFile.write(jmpOverwrite, sizeof(jmpOverwrite));


    asms->isDeployed = true;
    //int(*f)() = (int(*)()) executable_area;
    //f();

    // Note: RAII this in C++. Restore old flags, free memory.
    VirtualProtect(hookAdress, MAX_PATCH_SIZE, old_protect2, &old_protect2);
    //VirtualFree(hookAdress, MAX_PATCH_SIZE, MEM_RELEASE);

    return;
}

void writeBytesToDeployedAsm(struct asmHook* asms, uint64_t input, uint64_t index, unsigned char bytes) {
    //_SetOtherThreadsSuspended(true);
    uint64_t asmSize = asms->fileSize - asms->bytesToStrip;
    memcpy((void*)(asms->hookTarget + asmSize - asms->numberOfWritableBytes + index), &input, bytes);
    //_SetOtherThreadsSuspended(false);
    return;
}

unsigned char readByteFromDeployedAsm(struct asmHook* asms, uint64_t index) {
    //_SetOtherThreadsSuspended(true);
    uint64_t asmSize = asms->fileSize - asms->bytesToStrip;
    unsigned char ret = *(unsigned char*)(asms->hookTarget + asmSize - asms->numberOfWritableBytes + index);
    //_SetOtherThreadsSuspended(false);
    return ret;
}

void DebugBox(LPCSTR lpText) {
#ifdef DEBUG
   MessageBoxA(NULL, lpText, "Debug", MB_OK);
#endif // DEBUG
}
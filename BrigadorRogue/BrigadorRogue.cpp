// dllmain.cpp : Defines the entry point for the DLL application.
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

using namespace std;

const enum states {
    MainMenu = 0x2,
    Campaign = 0x3,
    Freelance = 0x4,
    Aquisitions = 0x5,
    Settings = 0x7, 
    Credits = 0x6,
    languageSelect = 0xA, 
    AfterLoadLevel = 0x8,
    InGame = 0xE,
    PauseMenu = 0x10,
    InGameSettings = 0x11,
    FreelancerChooseDistrict = 0xC, 
    LoseScreen = 0x0B
};

#define MAX_PATCH_SIZE 1024

//Struct requires ONE register to be available and overwritten without being pushed to allow for absolute jumps (because they require a register, not a constant, I believe)
struct asmHook {
    char asmFilename[MAX_PATH]; //Should be placed in the same folder as this dll.
    uint64_t fileSize; // total file size.
    uint64_t bytesToStrip; //prefix bytes to strip. Should be 120 unless the fasm (asm) prologue is non-standard.
    uint64_t hookStartOffset; //baseAdress+hookStartOffset = the adress at which we begin overwrite of main process code
    uint64_t overwrittenBytes; //Should be byte size of all completely and partially overwritten operation combined. This should ensure that after overwrittenBytes+extraOverwritten a valid instruction can be executed.
    uint64_t hookTarget = NULL; //Must be set after memory has been alocated. Start adress of asm function in memory
    uint64_t numberOfWritableBytes; //Always coded to be placed contiguously at end of asm file
    //jump back adress should always be overwritten starting at "8 (jump adress) + 2 (jmp register opcode) + numberOfWritableBytes" number of bytes from end of file.
    BOOL isDeployed = false;
    uint64_t addressesToReplaceInASM[64]; //Start byte to replace with baseAddress+replacementOffset
    uint64_t replacementOffsets[64]; //Offsets to be used in combination with with same index in addressesToReplaceInASM
    uint64_t externalAddressesToReplaceInASM[64]; //Start byte to replace with below values according to index
    uint64_t externalReplacementValues[64]; //Values meant to replace values in the asm. Currently used for string addresses for example
};


int fileToBytes (char* name, char* buffer) {
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


//#define JMP64RAX { {0x49}, { 0xba }, { 0x00 }, { 0x00 }, { 0x00 }, { 0x00 }, { 0x00 }, { 0x00 }, { 0x00 }, { 0x00 }, { 0xff }, { 0xe0 } }
// 
//Using this overwrites r10 from hook function. Keep this in mind
//#define JMP64R10 { {0x49}, { 0xb8 }, { 0x00 }, { 0x00 }, { 0x00 }, { 0x00 }, { 0x00 }, { 0x00 }, { 0x00 }, { 0x00 }, { 0x41 }, { 0xff }, { 0xe2 } }

//Using this overwrites r11 from hook function. 13 bytes necessary. Keep this in mind
#define JMP64R11 { {0x49}, { 0xbb }, { 0x00 }, { 0x00 }, { 0x00 }, { 0x00 }, { 0x00 }, { 0x00 }, { 0x00 }, { 0x00 }, { 0x41 }, { 0xff }, { 0xe3 } }

void deployExecutableASM(struct asmHook *asms) {
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
    for (int i = 0; asms->addressesToReplaceInASM[i] != 0 || i>=sizeof(asms->addressesToReplaceInASM); i++) {
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
    memcpy(jmpOverwrite+2, &asms->hookTarget, sizeof(asms->hookTarget));

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

void writeByteToDeployedAsm(struct asmHook *asms, unsigned char input, uint64_t index) {
    _SetOtherThreadsSuspended(true);
    uint64_t asmSize = asms->fileSize - asms->bytesToStrip;
    memset((void*)(asms->hookTarget + asmSize - asms->numberOfWritableBytes + index), input, 1);
    _SetOtherThreadsSuspended(false);
    return;
}

unsigned char readByteFromDeployedAsm(struct asmHook* asms, uint64_t index) {
    _SetOtherThreadsSuspended(true);
    uint64_t asmSize = asms->fileSize - asms->bytesToStrip;
    unsigned char ret = *(unsigned char*)(asms->hookTarget + asmSize - asms->numberOfWritableBytes + index);
    _SetOtherThreadsSuspended(false);
    return ret;
}


/*Redo the asm code for this one if you want to use it.Currently uses rax for the absolute jump which it should not do due to calling conventions
struct asmHook alterState {
    "alterState",
    186,
    120,
    0x5cd54,
    18,
    NULL,
    1,
    false
};*/

asmHook setState {
    "setState",
    188,
    120,
    0x75483,
    18,
    NULL,
    1,
    false,
    {},
    {},
    {},
    {}
};

//Used to fetch index of added button pressed
const enum buttons {
    M_Repair
    , M_PosOvercharge
    , M_PosForwardSpeed
    , P_PosCapacity
    , P_PosFireRate
    , P_PosProjectilesNegAccuracy
    , P_PosPropMult
    , S_PosCapacity
    , S_PosFireRate
    , S_PosProjectilesNegAccuracy
    , S_PosPropMult
};

//Must correspond to buttons enum
const char* addedButtonStrings[] = {
    {"Mech: Repair"}
    , {"Mech: +Max Overcharge"}
    , {"Mech: +Forward Speed"}
    , {"Primary: +Capacity"}
    , {"Primary: +Fire Rate"} 
    , {"Primary: +Projectiles, -Accuracy"}
    , {"Primary: +Structure Damage"}
    , {"Secondary: +Capacity"}
    , {"Secondary: +Fire Rate"} 
    , {"Secondary: +Projectiles, -Accuracy"}
    , {"Secondary: +Structure Damage"}
};

#define addedButtons sizeof(addedButtonStrings) / sizeof(addedButtonStrings[0])

asmHook addButtonsChooseDistrict {
    "addButtonsChooseDistrictV3",
    294,
    120,
    0x68e28,
    16,
    NULL,
    26,
    false,
    { {132} },
    { {0x68d12} }, // True loop start at 0x68d12
    { {270}, {278} }, //stringArrayAddres, buttonsToAdd
    { {(uint64_t)addedButtonStrings}, {addedButtons} }
};

// Uses set string in createUIButtonChooseDistrictOrupdateSomeUI2 from addButtonsChooseDistrict if it is not null
asmHook createUIButtonUseSetString {
    "createUIButtonUseSetString",
    173,
    120,
    0x754d0,
    14,
    NULL,
    8,
    false,
    {},
    {},
    { {165}}, //stringToPrintAddressLocation
    { {0x0}}  //Must be set before deploying. Should be address to location where addButtonsChooseDistrict stores the address of the nextStringToPrintAddress
};

void applyPatches(void) {
    _SetOtherThreadsSuspended(true);

    deployExecutableASM(&setState);
    deployExecutableASM(&addButtonsChooseDistrict);
    uint64_t nextStringToPrintAddress = (addButtonsChooseDistrict.hookTarget + (addButtonsChooseDistrict.fileSize - addButtonsChooseDistrict.bytesToStrip) - addButtonsChooseDistrict.numberOfWritableBytes + 18);
    createUIButtonUseSetString.externalReplacementValues[0] = nextStringToPrintAddress;
    deployExecutableASM(&createUIButtonUseSetString);
    //MessageBoxA(NULL, "Patches injected", "Patches injected", MB_OK);

    _SetOtherThreadsSuspended(false);
}

void freePatches(void) {
    VirtualFree((LPVOID)setState.hookTarget, 0, MEM_RELEASE);
    VirtualFree((LPVOID)addButtonsChooseDistrict.hookTarget, 0, MEM_RELEASE);
    VirtualFree((LPVOID)createUIButtonUseSetString.hookTarget, 0, MEM_RELEASE);
}

//thruple. Currently used as: offset, 4byte default value, 4byte modded value
struct uthruple {
    uint64_t fst;
    uint32_t snd;
    uint32_t thd;
};

//During the game loop the value of Base+RootOffset is often contained within r14. 
// This value is the key to finding pointer paths. Most (if not all) paths begin with this value.
#define rootOffset 0x4fdc18
#define keyAddress (GetBaseModuleForProcess() + rootOffset)

//MechOffset and weapons offsets are correlated by mechOffset + 0x18 = primaryWeaponOffset
#define mechOffset 0x2d00
#define mechLegsOffset 0x80
#define primaryWeaponOffset (0x2d18+(0x0<<0x5))
#define secondaryWeaponOffset  (0x2d18+(0x1<<0x5))
#define bulletOffset 0x540

//Arrays below use a tuple to store: Offset, Default Value, Modded Value

//Mech
const enum mechVarsIdx {
    MaxOverchargeIdx,
};

#define fetchDeployedMechAddress  *(uint64_t*)(*(uint64_t*)keyAddress + mechOffset)
uint64_t deployedMechAddress = 0;
uthruple deployedMechOffsetsAndVars[] = {
    {0x10b8, 0x0, 0x0} //Max Overcharge
};
#define mechVars sizeof(deployedMechOffsetsAndVars) / sizeof(deployedMechOffsetsAndVars[0])

//MechLegs
const enum mechLegsVarsIdx {
    MaxForwardSpeedIdx,
};

#define fetchDeployedMechLegsAddress  *(uint64_t*)(*(uint64_t*)(*(uint64_t*)keyAddress + mechOffset)+mechLegsOffset)
uint64_t deployedMechLegsAddress = 0;
uthruple deployedMechLegsOffsetsAndVars[] = {
    {0x354, 0x0, 0x0} //Max Forward Speed
};
#define mechLegsVars sizeof(deployedMechLegsOffsetsAndVars) / sizeof (deployedMechLegsOffsetsAndVars[0])

//Weapons
const enum weaponVarIdx {
    CapacityIdx,
    CooldownIdx,
    ShotCountIdx,
    AccuracyIdx
};

#define fetchDeployedPrimaryWeaponAddress  *(uint64_t*)(*(uint64_t*)keyAddress + primaryWeaponOffset)
uint64_t deployedPrimaryWeaponAddress = 0;
uthruple deployedPrimaryWeaponOffsetsAndVars[] = {
    {0x3d8 + 0x20, 0x0, 0x0} //Capacity
    ,{0x3d8 + 0x0, 0x0, 0x0} //Cooldown
    ,{0x3d8 + 0x28, 0x0, 0x0} //Shot Count
    ,{0x3d8 + 0x10, 0x0, 0x0} //Accuracy Cone Width
};
#define primaryWeaponVars sizeof(deployedPrimaryWeaponOffsetsAndVars) / sizeof(deployedPrimaryWeaponOffsetsAndVars[0])

#define fetchDeployedSecondaryWeaponAddress  *(uint64_t*)(*(uint64_t*)keyAddress + secondaryWeaponOffset)
uint64_t deployedSecondaryWeaponAddress = 0;
uthruple deployedSecondaryWeaponOffsetsAndVars[] = {
    {0x3d8 + 0x20, 0x0, 0x0} //Capacity
    ,{0x3d8 + 0x0, 0x0, 0x0} //Cooldown
    ,{0x3d8 + 0x28, 0x0, 0x0} //Shot Count
    ,{0x3d8 + 0x10, 0x0, 0x0} //Accuracy Cone Width
};
#define secondaryWeaponVars sizeof(deployedSecondaryWeaponOffsetsAndVars) / sizeof(deployedSecondaryWeaponOffsetsAndVars[0])

//Bullets
const enum bulletVars {
    PropMultIdx
};

#define fetchDeployedPrimaryBulletAddress  *(uint64_t*)(*(uint64_t*)(*(uint64_t*)keyAddress + primaryWeaponOffset)+bulletOffset)
uint64_t deployedPrimaryBulletAddress = 0;
uthruple deployedPrimaryBulletOffsetsAndVars[] = {
    {0x24, 0x0, 0x0} //prop multiplier
};
#define primaryBulletVars sizeof(deployedPrimaryBulletOffsetsAndVars) / sizeof(deployedPrimaryBulletOffsetsAndVars[0])

#define fetchDeployedSecondaryBulletAddress *(uint64_t*)(*(uint64_t*)(*(uint64_t*)keyAddress + secondaryWeaponOffset)+bulletOffset)
uint64_t deployedSecondaryBulletAddress = 0;
uthruple deployedSecondaryBulletOffsetsAndVars[] = {
    {0x24, 0x0, 0x0} //prop multiplier
};
#define secondaryBulletVars sizeof(deployedSecondaryBulletOffsetsAndVars) / sizeof(deployedSecondaryBulletOffsetsAndVars[0])

//Copies stored weapon var values to game memory
void setWeaponVars() {
    char buffer[256];
    //Should refactor but w/e
    for (int i = 0; i < mechVars; i++) {
        //snprintf(buffer, 100, "%#016x, %#016x", (uint32_t*)(deployedMechAddress + deployedMechOffsetsAndVars[i].fst), deployedMechOffsetsAndVars[i].thd);
        //MessageBoxA(NULL, buffer, "ALIVE", MB_OK);
        *(uint32_t*)(deployedMechAddress + deployedMechOffsetsAndVars[i].fst) = deployedMechOffsetsAndVars[i].thd;
    }
    for (int i = 0; i < mechLegsVars; i++) {
        *(uint32_t*)(deployedMechLegsAddress + deployedMechLegsOffsetsAndVars[i].fst) = deployedMechLegsOffsetsAndVars[i].thd;
    }
    for (int i = 0; i < primaryWeaponVars; i++) {
        *(uint32_t*)(deployedPrimaryWeaponAddress + deployedPrimaryWeaponOffsetsAndVars[i].fst) = deployedPrimaryWeaponOffsetsAndVars[i].thd;
    }
    for (int i = 0; i < secondaryWeaponVars; i++) {
        *(uint32_t*)(deployedSecondaryWeaponAddress + deployedSecondaryWeaponOffsetsAndVars[i].fst) = deployedSecondaryWeaponOffsetsAndVars[i].thd;
    }
    for (int i = 0; i < primaryBulletVars; i++) {
        *(uint32_t*)(deployedPrimaryBulletAddress + deployedPrimaryBulletOffsetsAndVars[i].fst) = deployedPrimaryBulletOffsetsAndVars[i].thd;
    }
    for (int i = 0; i < secondaryBulletVars; i++) {
        *(uint32_t*)(deployedSecondaryBulletAddress + deployedSecondaryBulletOffsetsAndVars[i].fst) = deployedSecondaryBulletOffsetsAndVars[i].thd;
    }
}

#define fetchCurrentState *(uint32_t*)(*(uint64_t*)(*(uint64_t*)keyAddress + 0x2918) + 0x4)

//Resets weapon var structs in case of weapon change or game state is in a menu where upgrades should reset
void resetWeaponVars(){
    char buffer[256];
    //snprintf(buffer, 100, "%#016x", fetchDeployedPrimaryWeaponAddress);
    //MessageBoxA(NULL, buffer, "ALIVE", MB_OK);
    uint64_t curDeployedMechAddress = fetchDeployedMechAddress;
    uint64_t curDeployedMechLegsAddress = fetchDeployedMechLegsAddress;
    uint64_t curDeployedPrimaryWeaponAddress = fetchDeployedPrimaryWeaponAddress;
    uint64_t curDeployedSecondaryWeaponAddress = fetchDeployedSecondaryWeaponAddress;
    uint64_t curDeployedPrimaryBulletAddress = fetchDeployedPrimaryBulletAddress;
    uint64_t curDeployedSecondaryBulletAddress = fetchDeployedSecondaryBulletAddress;

    states currentState = (states)fetchCurrentState;
    bool shouldReset = (
        currentState == MainMenu || 
        currentState == Campaign || 
        currentState == Freelance || 
        currentState == Aquisitions || 
        currentState == LoseScreen
        );
    
    //Should refactor but w/e
    //Reset Mech Vars and set new defaults
    if (curDeployedMechAddress != deployedMechAddress || shouldReset) {
        for (int i = 0; i < mechVars; i++) {
            if (deployedMechAddress != NULL) {
                *(uint32_t*)(deployedMechAddress + deployedMechOffsetsAndVars[i].fst) = deployedMechOffsetsAndVars[i].snd;
            }
            deployedMechOffsetsAndVars[i].snd = *(uint32_t*)(curDeployedMechAddress + deployedMechOffsetsAndVars[i].fst);
            deployedMechOffsetsAndVars[i].thd = deployedMechOffsetsAndVars[i].snd;
        }
        deployedMechAddress = curDeployedMechAddress;
    }

    //Reset Mech Legs Vars and set new defaults
    if (curDeployedMechLegsAddress != deployedMechLegsAddress || shouldReset) {
        for (int i = 0; i < mechLegsVars; i++) {
            if (deployedMechLegsAddress != NULL) {
                *(uint32_t*)(deployedMechLegsAddress + deployedMechLegsOffsetsAndVars[i].fst) = deployedMechLegsOffsetsAndVars[i].snd;
            }
            deployedMechLegsOffsetsAndVars[i].snd = *(uint32_t*)(curDeployedMechLegsAddress + deployedMechLegsOffsetsAndVars[i].fst);
            deployedMechLegsOffsetsAndVars[i].thd = deployedMechLegsOffsetsAndVars[i].snd;
        }
        deployedMechLegsAddress = curDeployedMechLegsAddress;
    }

    //Reset Primary Weapon Vars and set new defaults
    if (curDeployedPrimaryWeaponAddress != deployedPrimaryWeaponAddress || shouldReset) {
        for (int i = 0; i < primaryWeaponVars; i++) {
            if (deployedPrimaryWeaponAddress != NULL) {
                *(uint32_t*)(deployedPrimaryWeaponAddress + deployedPrimaryWeaponOffsetsAndVars[i].fst) = deployedPrimaryWeaponOffsetsAndVars[i].snd;
            }
            deployedPrimaryWeaponOffsetsAndVars[i].snd = *(uint32_t*)(curDeployedPrimaryWeaponAddress + deployedPrimaryWeaponOffsetsAndVars[i].fst);
            deployedPrimaryWeaponOffsetsAndVars[i].thd = deployedPrimaryWeaponOffsetsAndVars[i].snd;
        }
        deployedPrimaryWeaponAddress = curDeployedPrimaryWeaponAddress;
    }

    //Reset Secondary Weapon Vars and set new defaults
    if (curDeployedSecondaryWeaponAddress != deployedSecondaryWeaponAddress) {
        for (int i = 0; i < secondaryWeaponVars; i++) {
            if (deployedSecondaryWeaponAddress != NULL) {
                *(uint32_t*)(deployedSecondaryWeaponAddress + deployedSecondaryWeaponOffsetsAndVars[i].fst) = deployedSecondaryWeaponOffsetsAndVars[i].snd;
            }
            deployedSecondaryWeaponOffsetsAndVars[i].snd = *(uint32_t*)(curDeployedSecondaryWeaponAddress + deployedSecondaryWeaponOffsetsAndVars[i].fst);
            deployedSecondaryWeaponOffsetsAndVars[i].thd = deployedSecondaryWeaponOffsetsAndVars[i].snd;
        }
        deployedSecondaryWeaponAddress = curDeployedSecondaryWeaponAddress;
    }

    //Reset Primary Bullet Vars and set new defaults
    if (curDeployedPrimaryBulletAddress != deployedPrimaryBulletAddress) {
        for (int i = 0; i < primaryBulletVars; i++) {
            if (deployedPrimaryBulletAddress != NULL) {
                *(uint32_t*)(deployedPrimaryBulletAddress + deployedPrimaryBulletOffsetsAndVars[i].fst) = deployedPrimaryBulletOffsetsAndVars[i].snd;
            }
            deployedPrimaryBulletOffsetsAndVars[i].snd = *(uint32_t*)(curDeployedPrimaryBulletAddress + deployedPrimaryBulletOffsetsAndVars[i].fst);
            deployedPrimaryBulletOffsetsAndVars[i].thd = deployedPrimaryBulletOffsetsAndVars[i].snd;
        }
        deployedPrimaryBulletAddress = curDeployedPrimaryBulletAddress;
    }

    //Reset Secondary Bullet Vars and set new defaults
    if (curDeployedSecondaryBulletAddress != deployedSecondaryBulletAddress) {
        for (int i = 0; i < secondaryBulletVars; i++) {
            if (deployedSecondaryBulletAddress != NULL) {
                *(uint32_t*)(deployedSecondaryBulletAddress + deployedSecondaryBulletOffsetsAndVars[i].fst) = deployedSecondaryBulletOffsetsAndVars[i].snd;
            }
            deployedSecondaryBulletOffsetsAndVars[i].snd = *(uint32_t*)(curDeployedSecondaryBulletAddress + deployedSecondaryBulletOffsetsAndVars[i].fst);
            deployedSecondaryBulletOffsetsAndVars[i].thd = deployedSecondaryBulletOffsetsAndVars[i].snd;
        }
        deployedSecondaryBulletAddress = curDeployedSecondaryBulletAddress;
    }
}

//Returns True on valid and successful subtraction of in run money.
bool subtractMoney(double subtractAmount) {
    uint64_t moneyBase = GetBaseModuleForProcess() + 0x4fdea0;

    double* dVar2 = (double*)(moneyBase + 0xa4b8);
    double* dVar1 = (double*)(moneyBase + 0xa4c0);
    if (*dVar2 <= 0.0) {
        *dVar2 = 0.0;
    }
    double* dVar4 = (double*)(moneyBase + 0xa4c8);
    if (*dVar1 <= 0.0) {
        *dVar1 = 0.0;
    }
    if (*dVar4 <= 0.0) {
        *dVar4 = 0.0;
    }
    double* dVar3 = (double*)(moneyBase + 0xa4d0);
    if (*dVar3 <= 0.0) {
        *dVar3 = 0.0;
    }

    double currentMoney = *dVar1 + *dVar2 + *dVar4 + *dVar3;
    if (currentMoney >= subtractAmount) {
        if (*dVar1 >= subtractAmount) {
            *dVar1 -= subtractAmount;
            return true;
        }
        else {
            subtractAmount -= *dVar1;
            *dVar1 = 0;
        }

        if (*dVar2 >= subtractAmount) {
            *dVar2 -= subtractAmount;
            return true;
        }
        else {
            subtractAmount -= *dVar2;
            *dVar2 = 0;
        }

        if (*dVar3 >= subtractAmount) {
            *dVar3 -= subtractAmount;
            return true;
        }
        else {
            subtractAmount -= *dVar3;
            *dVar3 = 0;
        }

        if (*dVar4 >= subtractAmount) {
            *dVar4 -= subtractAmount;
            return true;
        }
        else {
            subtractAmount -= *dVar4;
            *dVar4 = 0;
        }
        return true;
    }
    else {
        return false;
    }
}

#define offsetUsedToFetchPlayerAddress 0x2ba0
#define addressUsedToFetchPlayerAddress *(uint64_t*)(*(uint64_t*)keyAddress + offsetUsedToFetchPlayerAddress)

//#define getPlayerAddressFunctionOffset 0x1542f0
//#define getPlayerAddressFunction GetBaseModuleForProcess() + getPlayerAddressFunctionOffset
//Function created by combining functions brigador.exe+0x1542f0 and brigador.exe+0x16580
uint64_t getPlayerAddress() {
    long long* entry = (long long*)addressUsedToFetchPlayerAddress;

    //Check for uninitialized player
    if ((long long*)addressUsedToFetchPlayerAddress == NULL || (long long**)(*entry + 0x1b8c8) == NULL || (int**)(*entry + 0x1b8f8) == NULL) {
        return 0;
    }

    long long* param_1 = *(long long**)(*entry + 0x1b8c8);

    int param_2 = 0;
    int* param_3 = *(int**)(*entry + 0x1b8f8);


    long long lVar1;
    long long lVar2;
    unsigned long long uVar3;
    unsigned long long uVar4;

    uVar3 = (unsigned long long)*param_3;
    if (*param_3 == param_2) {
        lVar1 = *param_1;
        if (uVar3 < (unsigned long long)((param_1[1] - lVar1) / 0x30)) {
            uVar4 = (unsigned long long)param_3[1];
            lVar2 = *(long long*)(lVar1 + uVar3 * 0x30);
            if (((uVar4 < (unsigned long long)(*(long long*)(lVar1 + 8 + uVar3 * 0x30) - lVar2 >> 4)) &&
                (*(int*)(lVar2 + uVar4 * 0x10) == param_3[2])) &&
                (*(long long*)(lVar2 + 8 + uVar4 * 0x10) != 0)) {
                //MessageBoxA(NULL, "Found Player Address", "ALIVE", MB_OK);
                return *(uint64_t*)(lVar2 + 8 + uVar4 * 0x10);
            }
        }
    }
    //MessageBoxA(NULL, "Could not find player address", "ALIVE", MB_OK);
    return 0;
}

#define getPlayerHealthAddress ((*(uint64_t*)(getPlayerAddress()+0x2b8) + 0x78)+0x4)
uint64_t getPlayerHealth() {
    uint64_t playerAddress = getPlayerAddress();
    if (playerAddress != NULL) {
        return getPlayerHealthAddress;
    }
    return 0;
}


float playerHealthToAdd = 0;
void handlePlayerHealth() {
    states currentState = (states)fetchCurrentState;
    if (playerHealthToAdd > 0 && currentState == InGame) {
        if (getPlayerHealth() != 0) {
            *(float*)getPlayerHealthAddress += playerHealthToAdd;
            playerHealthToAdd = 0;
        }
    }
}

#define upgradeCost 500000
void handlePressedButton(buttons buttonToHandle) {
    float refloat;
    switch (buttonToHandle) {
    case M_Repair:
        if (subtractMoney(upgradeCost)) {
            playerHealthToAdd += 50;
        }
        break;
    case M_PosOvercharge:
        if (subtractMoney(upgradeCost)) {
            refloat = reinterpret_cast<float&>(deployedMechOffsetsAndVars[MaxOverchargeIdx].thd);
            refloat *= 1.1;
            deployedMechOffsetsAndVars[MaxOverchargeIdx].thd = reinterpret_cast<uint32_t&>(refloat);
        }
        break;
    case M_PosForwardSpeed:
        if (subtractMoney(upgradeCost)) {
            refloat = reinterpret_cast<float&>(deployedMechLegsOffsetsAndVars[MaxForwardSpeedIdx].thd);
            refloat *= 1.1;
            deployedMechLegsOffsetsAndVars[MaxForwardSpeedIdx].thd = reinterpret_cast<uint32_t&>(refloat);
        }
        break;
    case P_PosCapacity: // Primary: +Capacity
        if (subtractMoney(upgradeCost)) {
            deployedPrimaryWeaponOffsetsAndVars[CapacityIdx].thd = (uint32_t)(deployedPrimaryWeaponOffsetsAndVars[CapacityIdx].thd * 1.2);
        }
        break;
    case P_PosFireRate: // Primary: +Fire Rate
        if (subtractMoney(upgradeCost)) {
            refloat = reinterpret_cast<float&>(deployedPrimaryWeaponOffsetsAndVars[CooldownIdx].thd);
            refloat *= 0.9;
            deployedPrimaryWeaponOffsetsAndVars[CooldownIdx].thd = reinterpret_cast<uint32_t&>(refloat);
        }
        break;
    case P_PosProjectilesNegAccuracy: // Primary: +Projectiles, -Accuracy
        if (subtractMoney(upgradeCost)) {
            deployedPrimaryWeaponOffsetsAndVars[ShotCountIdx].thd = (uint32_t)((int)deployedPrimaryWeaponOffsetsAndVars[ShotCountIdx].thd + 1);
            refloat = reinterpret_cast<float&>(deployedPrimaryWeaponOffsetsAndVars[AccuracyIdx].thd);
            refloat += 0.017; //1 degree added to guarantee accuracy is degraded even if it was previously 0.
            refloat *= 1.1;
            deployedPrimaryWeaponOffsetsAndVars[AccuracyIdx].thd = reinterpret_cast<uint32_t&>(refloat);
        }
        break;
    case P_PosPropMult: // Primary: +Structure Damage
        if (subtractMoney(upgradeCost)) {
            refloat = reinterpret_cast<float&>(deployedPrimaryBulletOffsetsAndVars[PropMultIdx].thd);
            refloat *= 1.1;
            deployedPrimaryBulletOffsetsAndVars[PropMultIdx].thd = reinterpret_cast<uint32_t&>(refloat);
        }
        break;
    case S_PosCapacity: // Secondary: +Capacity
        if (subtractMoney(upgradeCost)) {
            deployedSecondaryWeaponOffsetsAndVars[CapacityIdx].thd = (uint32_t)(deployedSecondaryWeaponOffsetsAndVars[CapacityIdx].thd * 1.2);
        }
        break;
    case S_PosFireRate: // Secondary: +Fire Rate
        if (subtractMoney(upgradeCost)) {
            refloat = reinterpret_cast<float&>(deployedSecondaryWeaponOffsetsAndVars[CooldownIdx].thd);
            refloat *= 0.9;
            deployedSecondaryWeaponOffsetsAndVars[CooldownIdx].thd = reinterpret_cast<uint32_t&>(refloat);
        }
        break;
    case S_PosProjectilesNegAccuracy: // Secondary: +Projectiles, -Accuracy
        if (subtractMoney(upgradeCost)) {
            deployedSecondaryWeaponOffsetsAndVars[ShotCountIdx].thd = (uint32_t)((int)deployedSecondaryWeaponOffsetsAndVars[ShotCountIdx].thd + 1);
            refloat = reinterpret_cast<float&>(deployedSecondaryWeaponOffsetsAndVars[AccuracyIdx].thd);
            refloat += 0.017; //1 degree added to guarantee accuracy is degraded even if it was previously 0.
            refloat *= 1.1;
            deployedSecondaryWeaponOffsetsAndVars[AccuracyIdx].thd = reinterpret_cast<uint32_t&>(refloat);
        }
        break;
    case S_PosPropMult: // Secondary: +Structure Damage
        if (subtractMoney(upgradeCost)) {
            refloat = reinterpret_cast<float&>(deployedSecondaryBulletOffsetsAndVars[PropMultIdx].thd);
            refloat *= 1.1;
            deployedSecondaryBulletOffsetsAndVars[PropMultIdx].thd = reinterpret_cast<uint32_t&>(refloat);
        }
        break;
    default:
        MessageBoxA(NULL, "Error: Undefined button pressed. ", "ALIVE", MB_OK);
        break;
    }
}

#define districtItemSize 0x2110

//Handles a player's action on the choose district menu in freelancer by observing the state of the currently selected button
void handleChooseDistrictMenu() {
    if (addButtonsChooseDistrict.isDeployed) {
        _SetOtherThreadsSuspended(true);

        //Update playerHealth to the modded value
        handlePlayerHealth();

        //Reset modded weapon vars if changes to weapon addresses or certain states are observed.
        resetWeaponVars();

        char buffer[256];

        uint64_t chooseDistrictMenuStruct = (*(uint64_t*)(*(uint64_t*)keyAddress + 0x2918) + 0x128 + 0x3e * 0x88);
        uint32_t chooseDistrictMenuIndex = (*(uint32_t*)chooseDistrictMenuStruct) & 0xffff;   

        //Fetch which modded button is pressed and should be handled
        uint64_t selectedDistrictItemAddress = *(uint64_t*)(*(uint64_t*)(*(uint64_t*)keyAddress + 0x2918) + 0x128 + (0x3e * 0x88) + 0x18);
        //int buttonToHandle = -1;

        //Unknown (and therefore explicitly injected) button handling
        if (selectedDistrictItemAddress != NULL && *(uint64_t*)selectedDistrictItemAddress == NULL) {
            //Find closest original item in district button array to conclude which added
            // button is pressed and should be handled. Done by going backwards from selected 
            // button and seing how many added buttons we actually have to pass through before
            // we get to an original button.
            uint64_t closestOriginalDistrictItemAddress = selectedDistrictItemAddress;
            int distanceFromOriginalItem = 0;
            while (*(uint64_t*)closestOriginalDistrictItemAddress == NULL) {
                distanceFromOriginalItem++;
                closestOriginalDistrictItemAddress = (selectedDistrictItemAddress - (distanceFromOriginalItem * districtItemSize));
            }
            buttons buttonToHandle = (buttons)(distanceFromOriginalItem - 1);

            handlePressedButton(buttonToHandle);
            
            setWeaponVars();
            memset((void*)chooseDistrictMenuStruct, 0x0 & 0xffff, 2);

        }
        
        if (GetAsyncKeyState(VK_NUMPAD1) & 0x80000) {
            snprintf(buffer, 100, "playerAddress = %#016x", getPlayerAddress());
            MessageBoxA(NULL, buffer, "ALIVE", MB_OK);
            if (getPlayerHealth() != 0) {
                snprintf(buffer, 100, "playerHealthAddress = %#016x", getPlayerHealthAddress);
                MessageBoxA(NULL, buffer, "ALIVE", MB_OK);
                snprintf(buffer, 100, "playerHealth = %f", *(float*)getPlayerHealthAddress);
                MessageBoxA(NULL, buffer, "ALIVE", MB_OK);
                *(float*)getPlayerHealthAddress += 100;
            }

            /*
            //Execute present function in brigador.exe to get the correct money value. 
            typedef double function(uint64_t);
            uint64_t functionAddress = GetBaseModuleForProcess() + 0x595a0;
            double (*getMoney)(uint64_t) = (function*)(functionAddress);
            double money = getMoney(GetBaseModuleForProcess() + 0x4fdea0);
            snprintf(buffer, 100, "Money = %e", money);
            MessageBoxA(NULL, buffer, "ALIVE", MB_OK);
            snprintf(buffer, 100, "Current State = %u", fetchCurrentState);
            MessageBoxA(NULL, buffer, "ALIVE", MB_OK);

            unsigned char originalButtonCount = readByteFromDeployedAsm(&addButtonsChooseDistrict, 0);
            //MessageBoxA(NULL, "ALIVE", "ALIVE", MB_OK);
            snprintf(buffer, 100, "Current index is %u, original button count was %u, %#016x, %#016x", (int)chooseDistrictMenuIndex, (int)originalButtonCount, (int)chooseDistrictMenuStruct, (int)selectedDistrictItemAddress);
            MessageBoxA(NULL, buffer, "ALIVE", MB_OK);
            if (selectedDistrictItemAddress != NULL) {
                snprintf(buffer, 100, "%#016x, %#016x, %#016x", selectedDistrictItemAddress, *(uint64_t*)selectedDistrictItemAddress, chooseDistrictMenuIndex);
                MessageBoxA(NULL, buffer, "ALIVE", MB_OK);
            }  
            */
        }
        
        _SetOtherThreadsSuspended(false);
    }
    return;
}

DWORD WINAPI MainThread(LPVOID param) {
    while (true) {
        /*
        if (GetAsyncKeyState(VK_NUMPAD1) & 0x80000) {
            //MessageBoxA(NULL, "State change", "State change", MB_OK);
            if (setState.isDeployed != 0) {
                writeByteToDeployedAsm(&setState, 0xE, 0);//Ingame
            }
        }
        if (GetAsyncKeyState(VK_NUMPAD2) & 0x80000) {
            //MessageBoxA(NULL, "State change", "State change", MB_OK);
            if (setState.isDeployed != 0) {
                writeByteToDeployedAsm(&setState, 4, 0); //Freelance
            }
        }
        if (GetAsyncKeyState(VK_NUMPAD3) & 0x80000) {
            //MessageBoxA(NULL, "State change", "State change", MB_OK);
            if (setState.isDeployed != 0) {
                writeByteToDeployedAsm(&setState, 0xC, 0);//ChooseDistrict
            }
        }
        if (GetAsyncKeyState(VK_NUMPAD4) & 0x80000) {
            //MessageBoxA(NULL, "State change", "State change", MB_OK);
            if (setState.isDeployed != 0) {
                writeByteToDeployedAsm(&setState, 4, 0);
            }
        }
        if (GetAsyncKeyState(VK_NUMPAD5) & 0x80000) {
            //MessageBoxA(NULL, "State change", "State change", MB_OK);
            if (setState.isDeployed != 0) {
                writeByteToDeployedAsm(&setState, 5, 0);
            }
        }
        if (GetAsyncKeyState(VK_NUMPAD6) & 0x80000) {
            //MessageBoxA(NULL, "State change", "State change", MB_OK);
            if (setState.isDeployed != 0) {
                writeByteToDeployedAsm(&setState, 6, 0);
            }
        }
        if (GetAsyncKeyState(VK_NUMPAD7) & 0x80000) {
            //MessageBoxA(NULL, "State change", "State change", MB_OK);
            if (addButtonsChooseDistrict.isDeployed != 0) {
                writeByteToDeployedAsm(&setState, 7, 0);
            }
        }
        if (GetAsyncKeyState(VK_NUMPAD8) & 0x80000) {
            MessageBoxA(NULL, "ALIVE", "ALIVE", MB_OK);
            //if (setState.isDeployed != 0) {
            //    writeByteToDeployedAsm(&setState, 8, 0);
            //}
        }
        */
        handleChooseDistrictMenu();
        Sleep(100);

    }
}

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        //MessageBoxA(NULL, "DLL injected", "DLL injected", MB_OK);
        applyPatches();
        CreateThread(0, 0, MainThread, hModule, 0, 0);
    case DLL_THREAD_ATTACH:
        break;
    case DLL_THREAD_DETACH:
        break;
    case DLL_PROCESS_DETACH:
        //VirtualProtect(executable_area, asms->numberOfBytes, old_protect, &old_protect);
        //VirtualFree(executable_area, asms->numberOfBytes, MEM_RELEASE);
        freePatches();
        break;
    }
    return TRUE;
}


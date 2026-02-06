#pragma once
//Using this overwrites r11 from hook function. 13 bytes necessary. Keep this in mind
#define JMP64R11 { {0x49}, { 0xbb }, { 0x00 }, { 0x00 }, { 0x00 }, { 0x00 }, { 0x00 }, { 0x00 }, { 0x00 }, { 0x00 }, { 0x41 }, { 0xff }, { 0xe3 } }

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

void _SetOtherThreadsSuspended(bool suspend);

uint64_t GetBaseModuleForProcess();

void deployExecutableASM(struct asmHook* asms);

void writeBytesToDeployedAsm(struct asmHook* asms, uint64_t input, uint64_t index, unsigned char bytes);

unsigned char readByteFromDeployedAsm(struct asmHook* asms, uint64_t index);

void DebugBox(LPCSTR lpText);
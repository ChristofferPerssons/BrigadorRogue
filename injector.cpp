#include "windows.h"
#include "TlHelp32.h"

void protectedRead(void* dst, void* src, int n) {
    DWORD old_protect = 0;
    VirtualProtect(dst, n, PAGE_EXECUTE_READWRITE, &old_protect);
    memcpy(dst, src, n);
    VirtualProtect(dst, n, old_protect, &old_protect);
}

void readBytes(void* read_address, void* read_buffer, int len) {
    protectedRead(read_buffer, read_address, len);
}

void writeBytes(void* destination_address, void* patch, int len) {
    protectedRead(destination_address, patch, len);
}

int getProcId(const wchar_t* target) {
    DWORD pid = 0;
    PROCESSENTRY32 pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32);
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS);
    do {
        if (wcscmp(pe32.szExeFile, target) == 0) {
            CloseHandle(hSnapshot);
            pid = pe32.th32ProcessID;
            break;
        }
    } while (Process32Next(hSnapshot, &pe32));
    CloseHandle(hSnapshot);
    return pid;
}

int main(int argc, char* argv[]) {
    const wchar_t* process = L"brigador.exe";
    int pid = getProcId(process);
    char dll[] = "brigador_rogue.dll";
    char dllPath[MAX_PATH] = { 0 };
    GetFullPathNameA(dll, MAX_PATH, dllPath, NULL);

    HANDLE hProcess = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ);
    LPVOID pszLibFileRemote = VirtualAllocEx(hProcess, NULL, strlen(dllPath) + 1, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    writeBytes(hProcess, pszLibFileRemote, INFINITE);

    HANDLE handleThread = CreateRemoteThread(hProcess, NULL, NULL, (LPTHREAD_START_ROUTINE)LoadLibrary, pszLibFileRemote, NULL, NULL);

    WaitForSingleObject(handleThread, INFINITE);
    CloseHandle(handleThread);
    VirtualFreeEx(hProcess, dllPath, 0, MEM_RELEASE);
    CloseHandle(hProcess);
}

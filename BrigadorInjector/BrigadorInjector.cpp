#include "windows.h"
#include "TlHelp32.h"
#include "iostream"

using namespace std;

void protectedRead(void* dst, void* src, int n) {
    cout << "Come on1";

    DWORD old_protect = 0;
    VirtualProtect(dst, n, PAGE_EXECUTE_READWRITE, &old_protect);
    cout << "Come on2";

    memcpy(dst, src, n);
    cout << "Come on3";

    VirtualProtect(dst, n, old_protect, &old_protect);
    cout << "Come on4";

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
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
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

int main(int argc, TCHAR* argv[]) {
    
    const wchar_t* process = L"brigador.exe";

    STARTUPINFO si;
    PROCESS_INFORMATION pi;

    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    // Start the child process. 
    if (!CreateProcess(process,   // No module name (use command line)
        0,        // Command line
        NULL,           // Process handle not inheritable
        NULL,           // Thread handle not inheritable
        FALSE,          // Set handle inheritance to FALSE
        0,              // No creation flags
        NULL,           // Use parent's environment block
        NULL,           // Use parent's starting directory 
        &si,            // Pointer to STARTUPINFO structure
        &pi)           // Pointer to PROCESS_INFORMATION structure
        )
    {
        printf("CreateProcess failed (%d).\n", GetLastError());
        return -1;
    }
    // Close process and thread handles. 
    CloseHandle(pi.hThread);
    Sleep(4000);

    char dll[] = "BrigadorRogue.dll";
    char dllPath[MAX_PATH] = { 0 };

    GetFullPathNameA(dll, MAX_PATH, dllPath, NULL);
    cout << dllPath << "\n";

    /*
    int pid = getProcId(process);
    cout << pid << "\n";
    HANDLE hProcess = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ, FALSE, pid);
    */
    HANDLE hProcess = pi.hProcess;
    LPVOID pszLibFileRemote = VirtualAllocEx(hProcess, NULL, strlen(dllPath) + 1, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    cout << hProcess << "\n";
    cout << pszLibFileRemote << "\n";

    //writeBytes(hProcess, dllPath, strlen(dllPath) + 1);
    if (pszLibFileRemote == 0) {

        return -1;
    }
    WriteProcessMemory(hProcess, pszLibFileRemote, dllPath, strlen(dllPath) + 1, NULL);

    cout << "Success";
   
    DWORD dwThreadId = 0;

    HANDLE handleThread = CreateRemoteThread(hProcess, NULL, NULL, (LPTHREAD_START_ROUTINE)LoadLibraryA, pszLibFileRemote, NULL, &dwThreadId);
    if (handleThread == 0) {
        return -1;
    }
    WaitForSingleObject(handleThread, INFINITE);
    CloseHandle(handleThread);
    VirtualFreeEx(hProcess, dllPath, 0, MEM_RELEASE);
    CloseHandle(pi.hProcess);
    return (int)handleThread;
}


;---------------------
;  Flat Assembler file
;---------------------
;       140075483 44 89 40      MOV                  dword ptr [RAX + 0x4],next_state
;                 04
;       140075487 48 8b 81      MOV                  RAX,qword ptr [stateStruct + 0x2918]
;                 18 29 00
;                 00
;       14007548e 48 8b 89      MOV                  stateStruct,qword ptr [stateStruct + 0x27d8]
;                 d8 27 00
;                 00
;rcx = stateStruct
;edx = prev_state
;r8d = next_state
;necessary overwritten bytes for abs jmp at 0x75483 = 18
; This code overwrites r11
define hookAdress 0x7FFFFFFFFFFFFFFF
format ELF64 executable 3
segment readable writable executable
entry $
  push rbx
  xor rbx, rbx
  mov rbx, [stateMem]
  test rbx, rbx
  jnz .newState
  jmp .end
  .newState:
  mov r8d, ebx
  .end:
  mov [rax+0x4], r8d
  ;replacements of overwritten detour ops
  mov rax,[rcx + 0x2918]
  mov rcx,[rcx + 0x27d8]
  ;absolute jump
  pop rbx
  mov r11, 0x00
  mov [stateMem], r11
  mov r11, hookAdress
  jmp r11
stateMem:
  db 0x00
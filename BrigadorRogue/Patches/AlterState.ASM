;---------------------
;  Flat Assembler file
;---------------------
;       14005cd54 f3 0f 58      ADDSS                XMM0,dword ptr [RCX + RBX*0x1 + 0x1a4]
;                 84 19 a4
;                 01 00 00
;       14005cd5d f3 0f 11      MOVSS                dword ptr [RCX + RBX*0x1 + 0x1a4],XMM0
;                 84 19 a4
;                 01 00 00
;necessary overwritten bytes = 18
define hookAdress 0x7FFFFFFFFFFFFFFF
format ELF64 executable 3
segment readable writable executable
entry $
  ;replacements of overwritten detour ops
  addss xmm0,[rcx + rbx*0x1 + 0x1a4]
  movss [rcx + rbx*0x1 + 0x1a4], xmm0

  ;Push xmm0
  ;sub     esp, 16
  ;movdqu  dqword [esp], xmm0
  push rbx
;replace constant with adress to alocated memory when injecting this code
  xor rbx, rbx
  mov bl, [stateMem]
  test rbx, rbx
  jnz .newState
;  lea eax,[r13 + -0x1]
  jmp .end
.newState:
;  mov eax, ebx
   mov r13, rbx
.end:
  ;Pop xmm0
  ;movdqu  xmm0, dqword [esp]
  ;add     esp, 16
  pop rbx
  mov rax, 0x00
  mov [stateMem], rax
  mov rax, hookAdress
  jmp rax
stateMem:
  db 0x00
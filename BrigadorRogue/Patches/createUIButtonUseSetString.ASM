;---------------------
;  Flat Assembler file
;---------------------
;
;       1400754d0 4c 89 4c      MOV                  qword ptr [RSP + 0x20],r9
;                 24 20
;       1400754d5 48 83 ec      SUB                  RSP,0x48
;                 48
;       1400754d9 48 8d 44      LEA                  RAX=>Stack[0x28],[RSP + 0x70]
;                 24 70
;
;
;necessary overwritten bytes for abs jmp at 0x754d0 = 14
; This code overwrites r11
define hookAddress 0x7FFFFFFFFFFFFFFF
format ELF64 executable 3
segment readable writable executable
entry $
  ; Test if we should use a set string instead of default
  mov r11, [nextStringToPrintAddress]
  mov r11, [r11]
  test r11, r11
  jz .originalCode
  ; Set r9 to begining address of the string to print
  mov r9, r11
.originalCode:
  ;replacements of overwritten detour ops
  mov qword [rsp+0x20], r9
  sub rsp,0x48
  lea rax, [rsp+0x70]
  ;absolute jump
  mov r11, hookAddress
  jmp r11
nextStringToPrintAddress:
  dq 0x7FFFFFFFFFFFFFFF

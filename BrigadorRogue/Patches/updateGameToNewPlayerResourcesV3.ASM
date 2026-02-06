;---------------------
;  Flat Assembler file
;---------------------
;
;       14006ea6a 49 8b ce      MOV                  stackTop,R14
;       14006ea6d e8 5e 2e      CALL                 FUN_1400f18d0                                    void FUN_1400f18d0(longlong para
;                 08 00
;       14006ea72 e8 49 a4      CALL                 thunk_FUN_140192720                              void thunk_FUN_140192720(void)
;                 06 00
;
;necessary overwritten bytes for abs jmp at 0x6ea6a = 13
; This code overwrites r11
define hookAddress 0x7FFFFFFFFFFFFFFF
define func1 0x7FFFFFFFFFFFFFFF
define func2 0x7FFFFFFFFFFFFFFF
define cmdSave 0x7FFFFFFFFFFFFFFF
define cmdLoad 0x7FFFFFFFFFFFFFFF
define updateDeployed 0x7FFFFFFFFFFFFFFF
format ELF64 executable 3
segment readable writable executable
entry $
.originalCode:
  ;replacements of overwritten detour ops
  mov rcx, r14
  mov r11, func1
  call r11
  mov r11, func2
  call r11
  ; Test if we should reload resources
  mov r11b, byte [shouldReloadResources]
  test r11b, r11b
  jz .absJump
.newCode:
  ; Update Deployed memory with new data
  mov r11, updateDeployed
  call r11
  ; Call cmd_save
  mov r11, cmdSave;0x1401311d0
  call r11
  ; Call cmd_loadsv
  mov r11, cmdLoad;0x140130440
  call r11
  ; Reset shouldReloadResources
  mov byte [shouldReloadResources], 0x0
.absJump:
  ;absolute jump
  mov r11, hookAddress
  jmp r11
shouldReloadResources:
  db 0x00
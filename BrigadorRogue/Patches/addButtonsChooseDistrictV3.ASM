;---------------------
;  Flat Assembler file
;---------------------
;
;       140068e28 ff c6         INC                  ESI
;       140068e2a 48 83 c7      ADD                  RDI,0x38
;                 38
;       140068e2e 49 83 ef      SUB                  R15,0x1
;                 01
;       140068e32 0f 85 da      JNZ                  createUIButtonChooseDistrictLoop
;                 fe ff ff
;
;   createUIButtonChooseDistrictLoop = 0x140068d12
;necessary overwritten bytes for abs jmp at 0x68e28 = 16
; This code overwrites r11
define hookAddress 0x7FFFFFFFFFFFFFFF
define loopAddress 0x7FFFFFFFFFFFFFFF
format ELF64 executable 3
segment readable writable executable
entry $
  ;replacements of overwritten detour ops
  inc esi
  add rdi,0x38
  sub r15,0x1
  ;Load loop address. Must be set explicitly when deploying ASM
  mov r11, loopAddress
  cmp r15,0
  jle .newCode
.loop:
  ;Test if numberOfOriginalButtons should be updated
  push rdi
  mov rdi, [newLoop]
  test dil, dil
  jz .execLoop
;Update numberOfOriginalButtons
  lea rdi,[r15+0x01]
  mov byte [numberOfOriginalButtons], byte dil
  ;set newLoop to false
  mov byte [newLoop], byte 0x00
.execLoop:
  pop rdi
  jmp r11
.newCode:
  ; Alter jump address to skip a condition check
  add r11, 0x12
  ; Save the current index
  push r15
  ; Make the negative (or 0) value positive
  imul r15, -1
  ; load string array address
  mov rdi, [stringArrayAddress]
  ; Calculate the address of the next string to print
  mov r15, [rdi+r15*0x8]
  ; Save next string to print address
  mov [nextStringToPrintAddress], r15
  ; Load the current index
  pop r15
  ; Get how many buttons to add
  mov rdi, [buttonsToAdd]
  imul rdi, -1
  inc rdi
  ; Loop until all buttons are added (Specified by buttonsToAdd)
  cmp r15,rdi
  ; Reset rdi to get the valid value used in the first original loop iteration
  mov rdi,0x0
  jge .loop
  ;set newLoop to true
  mov byte [newLoop], byte 0x01
  ;reset nextStringToPrint
  mov qword [nextStringToPrintAddress], 0x0
  ;absolute jump
  mov r11, hookAddress
  jmp r11
numberOfOriginalButtons:
  db 0x00
newLoop:
  db 0x01
stringArrayAddress:
  dq 0x7FFFFFFFFFFFFFFF
buttonsToAdd:
  dq 0x7FFFFFFFFFFFFFFF
nextStringToPrintAddress:
  dq 0x0000000000000000

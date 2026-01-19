ExitProcess PROTO

.data

.code
main PROC

	sub rsp, 28h   ;reserved the stack area as parameter passing area
	mov rcx, 12345678   ; specify Exit Code
	call ExitProcess

    addss xmm0,[rcx+rbx+000001A4]

	; replaced commandbrigador.exe+5CD54 - F3 0F58 84 19 A4010000  - addss xmm0,[rcx+rbx+000001A4]


main ENDP

END
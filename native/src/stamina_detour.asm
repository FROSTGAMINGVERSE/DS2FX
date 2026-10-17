; Preserve the engine's arguments and return convention. Only the low float in
; XMM1 is filtered. Tail-jump to MinHook's trampoline; original code owns writes.
EXTERN DS2FX_FilterStamina:PROC
EXTERN DS2FX_StaminaOriginal:QWORD
PUBLIC DS2FX_StaminaDetour
.code
DS2FX_StaminaDetour PROC FRAME
    sub rsp, 0C8h
    .allocstack 0C8h
    .endprolog
    mov [rsp+20h], rax
    mov [rsp+28h], rcx
    mov [rsp+30h], rdx
    mov [rsp+38h], r8
    mov [rsp+40h], r9
    mov [rsp+48h], r10
    mov [rsp+50h], r11
    movaps [rsp+60h], xmm0
    movaps [rsp+70h], xmm1
    movaps [rsp+80h], xmm2
    movaps [rsp+90h], xmm3
    movaps [rsp+0A0h], xmm4
    movaps [rsp+0B0h], xmm5
    call DS2FX_FilterStamina
    movss dword ptr [rsp+70h], xmm0
    movaps xmm0, [rsp+60h]
    movaps xmm1, [rsp+70h]
    movaps xmm2, [rsp+80h]
    movaps xmm3, [rsp+90h]
    movaps xmm4, [rsp+0A0h]
    movaps xmm5, [rsp+0B0h]
    mov rax, [rsp+20h]
    mov rcx, [rsp+28h]
    mov rdx, [rsp+30h]
    mov r8, [rsp+38h]
    mov r9, [rsp+40h]
    mov r10, [rsp+48h]
    mov r11, [rsp+50h]
    add rsp, 0C8h
    jmp qword ptr [DS2FX_StaminaOriginal]
DS2FX_StaminaDetour ENDP
END

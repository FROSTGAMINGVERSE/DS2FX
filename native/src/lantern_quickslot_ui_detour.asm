; Inline at50E3AA: RBX=FeItemData reference, EDI=native count(-1 hides).
; Change only EDI. The original native number formatting and visibility follow.
EXTERN DS2FX_LanternQuickslotCountOriginal:QWORD
EXTERN DS2FX_LanternQuickslotCount:PROC
PUBLIC DS2FX_LanternQuickslotCountDetour
.const
ui_safe_mxcsr DD 01F80h
.code
DS2FX_LanternQuickslotCountDetour PROC FRAME
    pushfq
    .allocstack 8
    sub rsp,268h
    .allocstack 268h
    .endprolog
    mov [rsp+220h],rax
    mov [rsp+228h],rcx
    mov [rsp+230h],rdx
    mov [rsp+238h],r8
    mov [rsp+240h],r9
    mov [rsp+248h],r10
    mov [rsp+250h],r11
    fxsave64 [rsp+20h]
    ldmxcsr DWORD PTR [ui_safe_mxcsr]
    mov rcx,rbx
    mov edx,edi
    call DS2FX_LanternQuickslotCount
    mov [rsp+260h],eax
    fxrstor64 [rsp+20h]
    mov edi,[rsp+260h]
    mov rax,[rsp+220h]
    mov rcx,[rsp+228h]
    mov rdx,[rsp+230h]
    mov r8,[rsp+238h]
    mov r9,[rsp+240h]
    mov r10,[rsp+248h]
    mov r11,[rsp+250h]
    add rsp,268h
    popfq
    jmp QWORD PTR [DS2FX_LanternQuickslotCountOriginal]
DS2FX_LanternQuickslotCountDetour ENDP
END

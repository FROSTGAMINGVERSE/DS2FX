; Inline at 14252A: RBX=ChrLightCtrl, RCX=ChrStatus, R8D=native row.
; Change only R8D; trampoline executes the original comparison/branch/setter.
EXTERN DS2FX_LanternLightSelectorOriginal:QWORD
EXTERN DS2FX_SelectLanternLight:PROC
PUBLIC DS2FX_LanternLightSelectorDetour
.const
selector_safe_mxcsr DD 01F80h
.code
DS2FX_LanternLightSelectorDetour PROC FRAME
    pushfq
    .allocstack 8
    ; Inline site RSP is 0 mod16; after pushfq use an 8-mod16 allocation.
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
    ldmxcsr DWORD PTR [selector_safe_mxcsr]
    mov rdx,rcx
    mov rcx,rbx
    call DS2FX_SelectLanternLight
    mov DWORD PTR [rsp+238h],eax
    mov DWORD PTR [rsp+23Ch],0
    fxrstor64 [rsp+20h]
    mov rax,[rsp+220h]
    mov rcx,[rsp+228h]
    mov rdx,[rsp+230h]
    mov r8,[rsp+238h]
    mov r9,[rsp+240h]
    mov r10,[rsp+248h]
    mov r11,[rsp+250h]
    add rsp,268h
    popfq
    jmp QWORD PTR [DS2FX_LanternLightSelectorOriginal]
DS2FX_LanternLightSelectorDetour ENDP
END

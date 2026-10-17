; Read-only shim for ApplySpEffect. The two native arguments are retained for
; the observer, then the complete volatile/FP state is restored before the
; MinHook trampoline executes the original call.
EXTERN DS2FX_LanternBonfireOriginal:QWORD
EXTERN DS2FX_ObserveLanternBonfire:PROC
PUBLIC DS2FX_LanternBonfireDetour
.const
bonfire_callback_mxcsr DD 01F80h
.code
DS2FX_LanternBonfireDetour PROC FRAME
    pushfq
    .allocstack 8
    sub rsp,260h
    .allocstack 260h
    .endprolog
    mov [rsp+220h],rax
    mov [rsp+228h],rcx
    mov [rsp+230h],rdx
    mov [rsp+238h],r8
    mov [rsp+240h],r9
    mov [rsp+248h],r10
    mov [rsp+250h],r11
    fxsave64 [rsp+20h]
    ldmxcsr DWORD PTR [bonfire_callback_mxcsr]
    call DS2FX_ObserveLanternBonfire
    fxrstor64 [rsp+20h]
    mov rax,[rsp+220h]
    mov rcx,[rsp+228h]
    mov rdx,[rsp+230h]
    mov r8,[rsp+238h]
    mov r9,[rsp+240h]
    mov r10,[rsp+248h]
    mov r11,[rsp+250h]
    add rsp,260h
    popfq
    jmp QWORD PTR [DS2FX_LanternBonfireOriginal]
DS2FX_LanternBonfireDetour ENDP
END

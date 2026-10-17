; Exact hash-gated native contract: RCX torch holder, EDX positive item count.
; The complete native body has no stack arguments. Preserve original input state
; before calling it and actual output state around post-call receipt observation.
EXTERN DS2FX_LanternAwardOriginal:QWORD
EXTERN DS2FX_BeginLanternAward:PROC
EXTERN DS2FX_EndLanternAward:PROC
PUBLIC DS2FX_LanternAwardDetour
.const
award_safe_mxcsr DD 01F80h
.code
SAVE_VOLATILE MACRO
    mov [rsp+220h],rax
    mov [rsp+228h],rcx
    mov [rsp+230h],rdx
    mov [rsp+238h],r8
    mov [rsp+240h],r9
    mov [rsp+248h],r10
    mov [rsp+250h],r11
    fxsave64 [rsp+20h]
ENDM
RESTORE_VOLATILE MACRO
    fxrstor64 [rsp+20h]
    mov rax,[rsp+220h]
    mov rcx,[rsp+228h]
    mov rdx,[rsp+230h]
    mov r8,[rsp+238h]
    mov r9,[rsp+240h]
    mov r10,[rsp+248h]
    mov r11,[rsp+250h]
ENDM
DS2FX_LanternAwardDetour PROC FRAME
    pushfq
    .allocstack 8
    sub rsp,2A0h
    .allocstack 2A0h
    .endprolog
    SAVE_VOLATILE
    ldmxcsr DWORD PTR [award_safe_mxcsr]
    lea r8,[rsp+260h]
    call DS2FX_BeginLanternAward
    RESTORE_VOLATILE
    push QWORD PTR [rsp+2A0h]
    popfq
    call QWORD PTR [DS2FX_LanternAwardOriginal]
    pushfq
    pop QWORD PTR [rsp+258h]
    SAVE_VOLATILE
    ldmxcsr DWORD PTR [award_safe_mxcsr]
    lea rcx,[rsp+260h]
    call DS2FX_EndLanternAward
    RESTORE_VOLATILE
    push QWORD PTR [rsp+258h]
    popfq
    lea rsp,[rsp+2A8h]
    ret
DS2FX_LanternAwardDetour ENDP
END

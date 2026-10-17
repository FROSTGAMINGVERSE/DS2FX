; Read-only research shim. Preserve original arguments, flags, volatile GPRs,
; all legacy XMM registers and x87/MXCSR environment before tail-jumping through
; the MinHook trampoline. No guessed game-function C++ signature is used.
EXTERN DS2FX_LanternEffectOriginal:QWORD
EXTERN DS2FX_ObserveLanternEffect:PROC
PUBLIC DS2FX_LanternEffectDetour
.const
effect_callback_mxcsr DD 01F80h
.code
DS2FX_LanternEffectDetour PROC FRAME
    pushfq
    .allocstack 8
    ; Entry RSP is 8 mod 16. pushfq aligns it; keep the allocation a multiple of
    ; 16. Frame: 32-byte shadow + 512-byte FXSAVE + seven GPR saves + padding.
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
    ; Diagnostic NaN validation must not inherit unmasked engine FP exceptions.
    ; The exact incoming FP environment is restored below, including flags.
    ldmxcsr DWORD PTR [effect_callback_mxcsr]
    call DS2FX_ObserveLanternEffect
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
    jmp QWORD PTR [DS2FX_LanternEffectOriginal]
DS2FX_LanternEffectDetour ENDP
END

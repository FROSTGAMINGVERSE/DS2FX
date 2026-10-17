; Exact31A6B0 is the base model draw entry. Preserve all incoming state, then
; tail-jump the original for local/unknown models. No altered return path or
; stack arguments on pass-through; native exceptions unwind exactly as before.
EXTERN DS2FX_LanternModelVisibilityOriginal:QWORD
EXTERN DS2FX_ShouldHideLanternBaseModel:PROC
PUBLIC DS2FX_LanternModelVisibilityDetour
.const
model_visibility_safe_mxcsr DD 01F80h
.code
DS2FX_LanternModelVisibilityDetour PROC FRAME
    pushfq
    .allocstack 8
    sub rsp,280h
    .allocstack 280h
    .endprolog
    mov [rsp+220h],rax
    mov [rsp+228h],rcx
    mov [rsp+230h],rdx
    mov [rsp+238h],r8
    mov [rsp+240h],r9
    mov [rsp+248h],r10
    mov [rsp+250h],r11
    fxsave64 [rsp+20h]
    ldmxcsr DWORD PTR [model_visibility_safe_mxcsr]
    call DS2FX_ShouldHideLanternBaseModel
    mov [rsp+260h],al
    fxrstor64 [rsp+20h]
    mov rax,[rsp+220h]
    mov rcx,[rsp+228h]
    mov rdx,[rsp+230h]
    mov r8,[rsp+238h]
    mov r9,[rsp+240h]
    mov r10,[rsp+248h]
    mov r11,[rsp+250h]
    cmp BYTE PTR [rsp+260h],0
    jne visibility_hide
    push QWORD PTR [rsp+280h]
    popfq
    lea rsp,[rsp+288h]
    jmp QWORD PTR [DS2FX_LanternModelVisibilityOriginal]
visibility_hide:
    push QWORD PTR [rsp+280h]
    popfq
    lea rsp,[rsp+288h]
    ret
DS2FX_LanternModelVisibilityDetour ENDP
END

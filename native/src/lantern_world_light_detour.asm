; Exact native1427F0 consumes RCX=ChrLightCtrl,RDX=float dt pointer, no stackargs.
; Call native first exactly once; preserve its return/volatile registers/FX/flags.
EXTERN DS2FX_LanternWorldLightOriginal:QWORD
EXTERN DS2FX_AfterLanternWorldLight:PROC
PUBLIC DS2FX_LanternWorldLightDetour
.const
world_light_safe_mxcsr DD 01F80h
.code
DS2FX_LanternWorldLightDetour PROC FRAME
    pushfq
    .allocstack 8
    sub rsp,280h
    .allocstack 280h
    .endprolog
    mov [rsp+260h],rcx
    push QWORD PTR [rsp+280h]
    popfq
    call QWORD PTR [DS2FX_LanternWorldLightOriginal]
    pushfq
    pop QWORD PTR [rsp+258h]
    mov [rsp+220h],rax
    mov [rsp+228h],rcx
    mov [rsp+230h],rdx
    mov [rsp+238h],r8
    mov [rsp+240h],r9
    mov [rsp+248h],r10
    mov [rsp+250h],r11
    fxsave64 [rsp+20h]
    ldmxcsr DWORD PTR [world_light_safe_mxcsr]
    mov rcx,[rsp+260h]
    call DS2FX_AfterLanternWorldLight
    fxrstor64 [rsp+20h]
    mov rax,[rsp+220h]
    mov rcx,[rsp+228h]
    mov rdx,[rsp+230h]
    mov r8,[rsp+238h]
    mov r9,[rsp+240h]
    mov r10,[rsp+248h]
    mov r11,[rsp+250h]
    push QWORD PTR [rsp+258h]
    popfq
    lea rsp,[rsp+288h]
    ret
DS2FX_LanternWorldLightDetour ENDP
END

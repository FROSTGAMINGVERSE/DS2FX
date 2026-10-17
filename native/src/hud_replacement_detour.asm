; Preserve the native void draw ABI and full incoming FP/register state.
EXTERN DS2FX_ShouldHideHudPrimitive:PROC
EXTERN DS2FX_HudObjectDrawOriginal:QWORD
PUBLIC DS2FX_HudObjectDrawDetour
EXTERN DS2FX_HudObjectMaskOriginal:QWORD
PUBLIC DS2FX_HudObjectMaskDetour
EXTERN DS2FX_HudTextureOriginal:QWORD
PUBLIC DS2FX_HudTextureDetour
.const
hud_safe_mxcsr DD 01F80h
.code
DS2FX_HudObjectDrawDetour PROC FRAME
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
    ldmxcsr DWORD PTR [hud_safe_mxcsr]
    call DS2FX_ShouldHideHudPrimitive
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
    jne HudObjectDraw_hide
    push QWORD PTR [rsp+280h]
    popfq
    lea rsp,[rsp+288h]
    jmp QWORD PTR [DS2FX_HudObjectDrawOriginal]
HudObjectDraw_hide:
    push QWORD PTR [rsp+280h]
    popfq
    lea rsp,[rsp+288h]
    ret
DS2FX_HudObjectDrawDetour ENDP
DS2FX_HudObjectMaskDetour PROC FRAME
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
    ldmxcsr DWORD PTR [hud_safe_mxcsr]
    call DS2FX_ShouldHideHudPrimitive
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
    jne HudObjectMask_hide
    push QWORD PTR [rsp+280h]
    popfq
    lea rsp,[rsp+288h]
    jmp QWORD PTR [DS2FX_HudObjectMaskOriginal]
HudObjectMask_hide:
    push QWORD PTR [rsp+280h]
    popfq
    lea rsp,[rsp+288h]
    ret
DS2FX_HudObjectMaskDetour ENDP
DS2FX_HudTextureDetour PROC FRAME
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
    ldmxcsr DWORD PTR [hud_safe_mxcsr]
    call DS2FX_ShouldHideHudPrimitive
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
    jne HudTexture_hide
    push QWORD PTR [rsp+280h]
    popfq
    lea rsp,[rsp+288h]
    jmp QWORD PTR [DS2FX_HudTextureOriginal]
HudTexture_hide:
    push QWORD PTR [rsp+280h]
    popfq
    lea rsp,[rsp+288h]
    ret
DS2FX_HudTextureDetour ENDP
END

EXTERN DS2FX_HudFixtureCounters:QWORD
PUBLIC DS2FX_HudFixtureDraw
PUBLIC DS2FX_HudFixtureMask
PUBLIC DS2FX_HudFixtureTexture
PUBLIC DS2FX_HudFixtureCall
.const
hud_fixture_xmm DD 03F800001h,03F800002h,03F800003h,03F800004h
.code
; The three owned targets intentionally have distinct bodies for MinHook.
DS2FX_HudFixtureDraw PROC
    mov [r8],rcx
    mov [r8+8],rdx
    mov [r8+10h],r8
    mov [r8+18h],r9
    mov [r8+28h],rax
    pushfq
    pop QWORD PTR [r8+20h]
    fxsave64 [r8+40h]
    inc QWORD PTR [DS2FX_HudFixtureCounters]
    ret
DS2FX_HudFixtureDraw ENDP
DS2FX_HudFixtureMask PROC
    mov [r8],rcx
    mov [r8+8],rdx
    mov [r8+10h],r8
    mov [r8+18h],r9
    mov [r8+28h],rax
    pushfq
    pop QWORD PTR [r8+20h]
    fxsave64 [r8+40h]
    inc QWORD PTR [DS2FX_HudFixtureCounters+8]
    ret
DS2FX_HudFixtureMask ENDP
DS2FX_HudFixtureTexture PROC
    mov [r8],rcx
    mov [r8+8],rdx
    mov [r8+10h],r8
    mov [r8+18h],r9
    mov [r8+28h],rax
    pushfq
    pop QWORD PTR [r8+20h]
    fxsave64 [r8+40h]
    inc QWORD PTR [DS2FX_HudFixtureCounters+10h]
    ret
DS2FX_HudFixtureTexture ENDP
DS2FX_HudFixtureCall PROC FRAME
    push rbx
    .pushreg rbx
    sub rsp,20h
    .allocstack 20h
    .endprolog
    mov rbx,rcx
    mov rcx,rdx
    mov rdx,01122334455667788h
    mov r9,03122334455667788h
    movups xmm0,XMMWORD PTR [hud_fixture_xmm]
    movaps xmm1,xmm0
    mov rax,04122334455667788h
    cmp rax,rax
    call rbx
    add rsp,20h
    pop rbx
    ret
DS2FX_HudFixtureCall ENDP
END

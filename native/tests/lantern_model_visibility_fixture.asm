PUBLIC DS2FX_ModelVisibilityFixtureTarget
PUBLIC DS2FX_ModelVisibilityFixtureCapture
EXTERN DS2FX_ModelVisibilityFixtureThrow:PROC
.const
visibility_fixture_xmm DD 03F800001h,03F800002h,03F800003h,03F800004h
.code
DS2FX_ModelVisibilityFixtureTarget PROC
    mov [rcx+500h],rax
    mov [rcx+508h],rcx
    mov [rcx+510h],rdx
    mov [rcx+518h],r8
    mov [rcx+520h],r9
    mov [rcx+528h],r10
    mov [rcx+530h],r11
    pushfq
    pop QWORD PTR [rcx+538h]
    fxsave64 [rcx+540h]
    cmp DWORD PTR [rcx+4E4h],0
    jne visibility_fixture_throw
    inc DWORD PTR [rcx+4E0h]
    mov rax,01122334455667788h
    mov r8,02122334455667788h
    mov r9,03122334455667788h
    mov r10,04122334455667788h
    mov r11,05122334455667788h
    movups xmm0,XMMWORD PTR [visibility_fixture_xmm]
    movaps xmm1,xmm0
    movaps xmm2,xmm0
    movaps xmm3,xmm0
    movaps xmm4,xmm0
    movaps xmm5,xmm0
    cmp rax,1
    ret
visibility_fixture_throw:
    jmp DS2FX_ModelVisibilityFixtureThrow
DS2FX_ModelVisibilityFixtureTarget ENDP
DS2FX_ModelVisibilityFixtureCapture PROC FRAME
    push rbx
    .pushreg rbx
    push rsi
    .pushreg rsi
    sub rsp,28h
    .allocstack 28h
    .endprolog
    mov rbx,r8
    mov rsi,rcx
    mov rcx,rdx
    mov rax,07122334455667788h
    mov rdx,06122334455667788h
    mov r8,02122334455667788h
    mov r9,03122334455667788h
    mov r10,04122334455667788h
    mov r11,05122334455667788h
    movups xmm0,XMMWORD PTR [visibility_fixture_xmm]
    movaps xmm1,xmm0
    movaps xmm2,xmm0
    movaps xmm3,xmm0
    movaps xmm4,xmm0
    movaps xmm5,xmm0
    cmp rax,1
    call rsi
    mov [rbx],rax
    mov [rbx+8],rcx
    mov [rbx+10h],rdx
    mov [rbx+18h],r8
    mov [rbx+20h],r9
    mov [rbx+28h],r10
    mov [rbx+30h],r11
    pushfq
    pop QWORD PTR [rbx+38h]
    fxsave64 [rbx+40h]
    add rsp,28h
    pop rsi
    pop rbx
    ret
DS2FX_ModelVisibilityFixtureCapture ENDP
END

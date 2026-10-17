PUBLIC DS2FX_PoseObserverFixtureTarget
PUBLIC DS2FX_PoseObserverFixtureCapture
.const
pose_fixture_xmm DD 03F800001h,03F800002h,03F800003h,03F800004h
.code
DS2FX_PoseObserverFixtureTarget PROC
    inc DWORD PTR [rdx+38h]
    mov rax,[rdx+20h]
    mov DWORD PTR [rax+0C0h],03E800000h
    mov rax,01122334455667788h
    mov r9,03122334455667788h
    mov r10,04122334455667788h
    mov r11,05122334455667788h
    movups xmm0,XMMWORD PTR [pose_fixture_xmm]
    movaps xmm1,xmm0
    movaps xmm2,xmm0
    movaps xmm3,xmm0
    movaps xmm4,xmm0
    movaps xmm5,xmm0
    cmp rax,1
    ret
DS2FX_PoseObserverFixtureTarget ENDP
DS2FX_PoseObserverFixtureCapture PROC FRAME
    push rbx
    .pushreg rbx
    sub rsp,20h
    .allocstack 20h
    .endprolog
    mov rbx,r8
    mov rax,rcx
    mov rcx,06122334455667788h
    mov r8,02122334455667788h
    call rax
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
    add rsp,20h
    pop rbx
    ret
DS2FX_PoseObserverFixtureCapture ENDP
END

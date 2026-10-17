EXTERN DS2FX_AwardFixtureApply:PROC
EXTERN DS2FX_AwardObserved:BYTE
PUBLIC DS2FX_AwardFixtureTarget
PUBLIC DS2FX_AwardInvoke
.const
ALIGN 16
award_return_vector DQ 0123456789ABCDEF0h,0FEDCBA9876543210h
.code
DS2FX_AwardFixtureTarget PROC FRAME
    sub rsp,28h
    .allocstack 28h
    .endprolog
    call DS2FX_AwardFixtureApply
    add rsp,28h
    mov rax,0BADC0FFEE0DDF00Dh
    mov r10,01020304050607080h
    mov r11,0A0B0C0D0E0F01020h
    movdqa xmm0,XMMWORD PTR [award_return_vector]
    movdqa xmm1,xmm0
    movdqa xmm2,xmm0
    movdqa xmm3,xmm0
    movdqa xmm4,xmm0
    movdqa xmm5,xmm0
    push 247h
    popfq
    ret
DS2FX_AwardFixtureTarget ENDP
; Invoke(target, owner, count), preserving the observed native output for C++.
DS2FX_AwardInvoke PROC FRAME
    sub rsp,28h
    .allocstack 28h
    .endprolog
    mov rax,rcx
    mov rcx,rdx
    mov edx,r8d
    call rax
    mov QWORD PTR [DS2FX_AwardObserved],rax
    mov QWORD PTR [DS2FX_AwardObserved+8],r10
    mov QWORD PTR [DS2FX_AwardObserved+16],r11
    pushfq
    pop QWORD PTR [DS2FX_AwardObserved+24]
    movdqu XMMWORD PTR [DS2FX_AwardObserved+32],xmm0
    movdqu XMMWORD PTR [DS2FX_AwardObserved+48],xmm1
    movdqu XMMWORD PTR [DS2FX_AwardObserved+64],xmm2
    movdqu XMMWORD PTR [DS2FX_AwardObserved+80],xmm3
    movdqu XMMWORD PTR [DS2FX_AwardObserved+96],xmm4
    movdqu XMMWORD PTR [DS2FX_AwardObserved+112],xmm5
    add rsp,28h
    ret
DS2FX_AwardInvoke ENDP
END

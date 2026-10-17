EXTERN DS2FX_EffectObserved:BYTE
PUBLIC DS2FX_EffectFixtureTarget
PUBLIC DS2FX_EffectInvoke
.const
ALIGN 16
effect_vector DQ 0123456789ABCDEF0h, 0FEDCBA9876543210h
effect_mxcsr DD 05F80h
effect_x87 DW 077Fh
.code
; An owned detour target whose first instructions preserve all tested state.
; There is no game code in this fixture.
DS2FX_EffectFixtureTarget PROC
    ; Same instruction forms/branch distances as the 35-byte native update.
    ; Only owned synthetic state is touched by this fixture.
    movzx eax,BYTE PTR [rcx+30h]
    dec al
    cmp al,1
    jbe fixture_skip_decrement
    movss xmm0,DWORD PTR [rcx+20h]
    subss xmm0,xmm1
    movss DWORD PTR [rcx+20h],xmm0
fixture_skip_decrement:
    addss xmm1,DWORD PTR [rcx+24h]
    movss DWORD PTR [rcx+24h],xmm1
    mov QWORD PTR [DS2FX_EffectObserved+0],rax
    mov QWORD PTR [DS2FX_EffectObserved+8],rcx
    mov QWORD PTR [DS2FX_EffectObserved+16],rdx
    mov QWORD PTR [DS2FX_EffectObserved+24],r8
    mov QWORD PTR [DS2FX_EffectObserved+32],r9
    mov QWORD PTR [DS2FX_EffectObserved+40],r10
    mov QWORD PTR [DS2FX_EffectObserved+48],r11
    pushfq
    pop QWORD PTR [DS2FX_EffectObserved+56]
    mov rax,[rsp+28h]
    mov QWORD PTR [DS2FX_EffectObserved+64],rax
    mov rax,[rsp+30h]
    mov QWORD PTR [DS2FX_EffectObserved+72],rax
    movdqu XMMWORD PTR [DS2FX_EffectObserved+80],xmm0
    movdqu XMMWORD PTR [DS2FX_EffectObserved+96],xmm1
    movdqu XMMWORD PTR [DS2FX_EffectObserved+112],xmm2
    movdqu XMMWORD PTR [DS2FX_EffectObserved+128],xmm3
    movdqu XMMWORD PTR [DS2FX_EffectObserved+144],xmm4
    movdqu XMMWORD PTR [DS2FX_EffectObserved+160],xmm5
    movdqu XMMWORD PTR [DS2FX_EffectObserved+176],xmm6
    movdqu XMMWORD PTR [DS2FX_EffectObserved+192],xmm7
    movdqu XMMWORD PTR [DS2FX_EffectObserved+208],xmm8
    movdqu XMMWORD PTR [DS2FX_EffectObserved+224],xmm9
    movdqu XMMWORD PTR [DS2FX_EffectObserved+240],xmm10
    movdqu XMMWORD PTR [DS2FX_EffectObserved+256],xmm11
    movdqu XMMWORD PTR [DS2FX_EffectObserved+272],xmm12
    movdqu XMMWORD PTR [DS2FX_EffectObserved+288],xmm13
    movdqu XMMWORD PTR [DS2FX_EffectObserved+304],xmm14
    movdqu XMMWORD PTR [DS2FX_EffectObserved+320],xmm15
    stmxcsr DWORD PTR [DS2FX_EffectObserved+336]
    fnstcw WORD PTR [DS2FX_EffectObserved+340]
    mov rax,0BADC0FFEE0DDF00Dh
    ret
DS2FX_EffectFixtureTarget ENDP

; uint64 Invoke(void* target, uintptr manager, const float* update)
DS2FX_EffectInvoke PROC FRAME
    push rbx
    .pushreg rbx
    sub rsp,260h
    .allocstack 260h
    .endprolog
    fxsave64 [rsp+40h]
    mov rbx,rcx
    mov rcx,rdx
    mov rdx,r8
    mov r8,08877665544332211h
    mov r9,09988776655443322h
    mov rax,055AA55AA55AA55AAh
    mov [rsp+20h],rax
    mov rax,0AA55AA55AA55AA55h
    mov [rsp+28h],rax
    movdqa xmm0,XMMWORD PTR [effect_vector]
    movdqa xmm1,xmm0
    movdqa xmm2,xmm0
    movdqa xmm3,xmm0
    movdqa xmm4,xmm0
    movdqa xmm5,xmm0
    movdqa xmm6,xmm0
    movdqa xmm7,xmm0
    movdqa xmm8,xmm0
    movdqa xmm9,xmm0
    movdqa xmm10,xmm0
    movdqa xmm11,xmm0
    movdqa xmm12,xmm0
    movdqa xmm13,xmm0
    movdqa xmm14,xmm0
    movdqa xmm15,xmm0
    ldmxcsr DWORD PTR [effect_mxcsr]
    fninit
    fldcw WORD PTR [effect_x87]
    mov r10,01020304050607080h
    mov r11,0A0B0C0D0E0F01020h
    mov rax,0CAFEFACE12345678h
    push 247h
    popfq
    call rbx
    fxrstor64 [rsp+40h]
    add rsp,260h
    pop rbx
    ret
DS2FX_EffectInvoke ENDP
END

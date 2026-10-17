PUBLIC DS2FX_SelectorFixtureInvoke
PUBLIC DS2FX_SelectorFixturePatch
PUBLIC DS2FX_BonfireChargeFixture
.code
; RCX=controller RDX=flags R8D=native row, matching the native inline site.
DS2FX_SelectorFixtureInvoke PROC FRAME
    push rbx
    .pushreg rbx
    sub rsp,20h
    .allocstack 20h
    .endprolog
    mov rbx,rcx
    mov rcx,rdx
    or eax,-1
DS2FX_SelectorFixturePatch LABEL BYTE
    cmp r8d,DWORD PTR [rbx+1Ch]
    je fixture_unchanged
    lea edx,[rax+4]
    mov rcx,rbx
    nop
    nop
    nop
    nop
    nop
    mov DWORD PTR [rbx+1Ch],r8d
    inc DWORD PTR [rbx+2D8h]
fixture_unchanged:
    mov eax,r8d
    add rsp,20h
    pop rbx
    ret
DS2FX_SelectorFixtureInvoke ENDP
DS2FX_BonfireChargeFixture PROC
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    ret
DS2FX_BonfireChargeFixture ENDP
END

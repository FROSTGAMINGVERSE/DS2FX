PUBLIC DS2FX_UIFixtureInvoke
PUBLIC DS2FX_UIFixturePatch
PUBLIC DS2FX_UIFixtureIcon
PUBLIC DS2FX_UIFixtureBonfire
.code
DS2FX_UIFixtureInvoke PROC FRAME
    push rbx
    .pushreg rbx
    push rdi
    .pushreg rdi
    sub rsp,28h
    .allocstack 28h
    .endprolog
    mov rbx,rcx
    mov edi,edx
DS2FX_UIFixturePatch LABEL BYTE
    lea rcx,[rsp+20h]
    mov edx,edi
    mov eax,edx
    add rsp,28h
    pop rdi
    pop rbx
    ret
DS2FX_UIFixtureInvoke ENDP
DS2FX_UIFixtureIcon PROC
    REPT 16
    nop
    ENDM
    mov DWORD PTR [rcx+188h],6
    mov DWORD PTR [rcx+18Ch],60425001
    mov QWORD PTR [rcx+1B0h],1
    ret
DS2FX_UIFixtureIcon ENDP
DS2FX_UIFixtureBonfire PROC
    REPT 16
    nop
    ENDM
    ret
DS2FX_UIFixtureBonfire ENDP
END

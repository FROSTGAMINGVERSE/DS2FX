; Native4E3FA0: RCX=mapper,RDX=destination MdlPose,R8=animation source. No stackargs.
; Run original transform construction first; preserve its actual return/FX/flags.
EXTERN DS2FX_LanternPoseObserverOriginal:QWORD
EXTERN DS2FX_AfterLanternPoseObserver:PROC
PUBLIC DS2FX_LanternPoseObserverDetour
.const
pose_observer_safe_mxcsr DD 01F80h
.code
DS2FX_LanternPoseObserverDetour PROC FRAME
    pushfq
    .allocstack 8
    sub rsp,280h
    .allocstack 280h
    .endprolog
    mov [rsp+260h],rdx
    push QWORD PTR [rsp+280h]
    popfq
    call QWORD PTR [DS2FX_LanternPoseObserverOriginal]
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
    ldmxcsr DWORD PTR [pose_observer_safe_mxcsr]
    mov rcx,[rsp+260h]
    call DS2FX_AfterLanternPoseObserver
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
DS2FX_LanternPoseObserverDetour ENDP
END


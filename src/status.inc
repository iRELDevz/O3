CONFIG_LIMIT equ 524288
VALUE_SIZE   equ 112

section .rdata
notRead          db "Tidak terbaca",0
notSet           db "Belum diatur",0
notFound         db "Tidak terdeteksi",0
notRunning       db "Belum berjalan",0
running          db "Berjalan",0
multiplePlayers  db "Beberapa proses",0
toggleOn         db "ON",0
toggleOff        db "OFF",0
normalPriority   db "Normal",0
abovePriority    db "AboveNormal",0
belowPriority    db "BelowNormal",0
highPriority     db "High",0
idlePriority     db "Idle",0
realPriority     db "Realtime",0
msiName          db "MSI App Player",0
blueName         db "BlueStacks 5",0
playerName       db "HD-Player.exe",0
msiDirectory     db '\BlueStacks_msi5\',0
blueDirectory    db '\BlueStacks_nxt\',0
msiConfig        db '\BlueStacks_msi5\bluestacks.conf',0
blueConfig       db '\BlueStacks_nxt\bluestacks.conf',0
programDataVar   db "ProgramData",0
defaultData      db 'C:\ProgramData',0
instanceStem     db "bst.instance.",0
firstFpsField    db ".max_fps=",0
maxFpsField      db "max_fps=",0
highFpsField     db "enable_high_fps=",0
vsyncField       db "enable_vsync=",0
widthField       db "fb_width=",0
heightField      db "fb_height=",0
dpiField         db "dpi=",0
gameDvrKey       db 'Software\Microsoft\Windows\CurrentVersion\GameDVR',0
captureValue     db "AppCaptureEnabled",0
gameBarKey       db 'Software\Microsoft\GameBar',0
gameModeValue    db "AutoGameModeEnabled",0
fmtProfile       db "%s / %s",0
fmtMemory        db "%u%% terpakai",0
fmtLimit         db "%u FPS",0
fmtResolution    db "%u x %u",0
fmtResolutionDpi db "%u x %u / %u DPI",0

section .bss
valueProfile     resb VALUE_SIZE
valueProcess     resb VALUE_SIZE
valueMemory      resb VALUE_SIZE
valueHigh        resb VALUE_SIZE
valueVsync       resb VALUE_SIZE
valueLimit       resb VALUE_SIZE
valueResolution  resb VALUE_SIZE
valueResolutionShort resb VALUE_SIZE
valuePower       resb VALUE_SIZE
valuePriority    resb VALUE_SIZE
valueCapture     resb VALUE_SIZE
valueGameMode    resb VALUE_SIZE
playerCount      resd 1
playerFamily     resd 1
playerPid        resd 1
playerEntry      resb 304
memoryStatus     resb 64
playerImage      resb 512
configPath       resb 512
configForProbe   resb 512
configData       resb CONFIG_LIMIT+1
instanceName     resb 64
instancePrefix   resb 82
wantedKey        resb 112
powerWideName    resw 128

section .text
%macro store_text 2
    lea     rcx, [%1]
    lea     rdx, [%2]
    call    lstrcpyA
%endmacro

scan_device:
    enter_frame
    mov     byte [configForProbe], 0
    mov     byte [instanceName], 0
    store_text valueProfile, notFound
    store_text valueProcess, notRunning
    store_text valueMemory, notRead
    store_text valueHigh, notRead
    store_text valueVsync, notRead
    store_text valueLimit, notRead
    store_text valueResolution, notRead
    store_text valueResolutionShort, notRead
    store_text valuePower, notRead
    store_text valuePriority, notRunning
    call    scan_process
    call    scan_memory
    call    scan_power
    lea     rcx, [gameDvrKey]
    lea     rdx, [captureValue]
    lea     r8, [valueCapture]
    call    registry_toggle
    lea     rcx, [gameBarKey]
    lea     rdx, [gameModeValue]
    lea     r8, [valueGameMode]
    call    registry_toggle
    call    scan_config
    leave_frame

scan_memory:
    enter_frame
    mov     dword [memoryStatus], 64
    lea     rcx, [memoryStatus]
    call    GlobalMemoryStatusEx
    test    eax, eax
    jz      .done
    lea     rcx, [valueMemory]
    lea     rdx, [fmtMemory]
    mov     r8d, [memoryStatus+4]
    call    wsprintfA
.done:
    leave_frame

scan_process:
    enter_frame
    mov     dword [playerCount], 0
    mov     dword [playerFamily], 0
    mov     ecx, 2
    xor     edx, edx
    call    CreateToolhelp32Snapshot
    cmp     rax, -1
    je      .unknown
    mov     [rsp+96], rax
    mov     dword [playerEntry], 304
    mov     rcx, rax
    lea     rdx, [playerEntry]
    call    Process32First
.next:
    test    eax, eax
    jz      .finish
    lea     rcx, [playerEntry+44]
    lea     rdx, [playerName]
    call    lstrcmpiA
    test    eax, eax
    jnz     .advance
    inc     dword [playerCount]
    mov     eax, [playerEntry+8]
    mov     [playerPid], eax
.advance:
    mov     rcx, [rsp+96]
    lea     rdx, [playerEntry]
    call    Process32Next
    jmp     .next
.finish:
    mov     rcx, [rsp+96]
    call    CloseHandle
    cmp     dword [playerCount], 1
    jb      .done
    ja      .multiple
    store_text valueProcess, running
    store_text valuePriority, notRead
    mov     ecx, 0x1000
    xor     edx, edx
    mov     r8d, [playerPid]
    call    OpenProcess
    test    rax, rax
    jz      .done
    mov     [rsp+96], rax
    mov     rcx, rax
    call    GetPriorityClass
    lea     rdx, [notRead]
    cmp     eax, 0x20
    jne     .above
    lea     rdx, [normalPriority]
    jmp     .save_priority
.above:
    cmp     eax, 0x8000
    jne     .below
    lea     rdx, [abovePriority]
    jmp     .save_priority
.below:
    cmp     eax, 0x4000
    jne     .high
    lea     rdx, [belowPriority]
    jmp     .save_priority
.high:
    cmp     eax, 0x80
    jne     .idle
    lea     rdx, [highPriority]
    jmp     .save_priority
.idle:
    cmp     eax, 0x40
    jne     .realtime
    lea     rdx, [idlePriority]
    jmp     .save_priority
.realtime:
    cmp     eax, 0x100
    jne     .save_priority
    lea     rdx, [realPriority]
.save_priority:
    lea     rcx, [valuePriority]
    call    lstrcpyA
    mov     dword [rsp+104], 512
    mov     rcx, [rsp+96]
    xor     edx, edx
    lea     r8, [playerImage]
    lea     r9, [rsp+104]
    call    QueryFullProcessImageNameA
    test    eax, eax
    jz      .close_player
    lea     rcx, [playerImage]
    lea     rdx, [msiDirectory]
    call    StrStrIA
    test    rax, rax
    jz      .blue
    mov     dword [playerFamily], 1
    jmp     .close_player
.blue:
    lea     rcx, [playerImage]
    lea     rdx, [blueDirectory]
    call    StrStrIA
    test    rax, rax
    jz      .close_player
    mov     dword [playerFamily], 2
.close_player:
    mov     rcx, [rsp+96]
    call    CloseHandle
    jmp     .done
.multiple:
    store_text valueProcess, multiplePlayers
    store_text valuePriority, multiplePlayers
    jmp     .done
.unknown:
    store_text valueProcess, notRead
    store_text valuePriority, notRead
.done:
    leave_frame

scan_power:
    enter_frame
    xor     ecx, ecx
    lea     rdx, [rsp+96]
    call    PowerGetActiveScheme
    test    eax, eax
    jnz     .done
    mov     dword [rsp+104], 256
    lea     rax, [powerWideName]
    mov     [rsp+32], rax
    lea     rax, [rsp+104]
    mov     [rsp+40], rax
    xor     ecx, ecx
    mov     rdx, [rsp+96]
    xor     r8d, r8d
    xor     r9d, r9d
    call    PowerReadFriendlyName
    test    eax, eax
    jnz     .free
    lea     rax, [valuePower]
    mov     [rsp+32], rax
    mov     qword [rsp+40], VALUE_SIZE
    mov     qword [rsp+48], 0
    mov     qword [rsp+56], 0
    xor     ecx, ecx
    xor     edx, edx
    lea     r8, [powerWideName]
    mov     r9d, -1
    call    WideCharToMultiByte
    test    eax, eax
    jnz     .free
    store_text valuePower, notRead
.free:
    mov     rcx, [rsp+96]
    call    LocalFree
.done:
    leave_frame

registry_toggle:
    enter_frame
    mov     [rsp+96], r8
    mov     r8, rdx
    mov     rdx, rcx
    mov     rcx, 0xffffffff80000001
    mov     r9d, 0x18
    mov     qword [rsp+32], 0
    lea     rax, [rsp+104]
    mov     [rsp+40], rax
    mov     dword [rsp+108], 4
    lea     rax, [rsp+108]
    mov     [rsp+48], rax
    call    RegGetValueA
    lea     rdx, [notSet]
    cmp     eax, 2
    je      .save
    lea     rdx, [notRead]
    test    eax, eax
    jnz     .save
    mov     eax, [rsp+104]
    call    toggle_text
.save:
    mov     rcx, [rsp+96]
    call    lstrcpyA
    leave_frame

toggle_text:
    lea     rdx, [toggleOff]
    test    eax, eax
    jz      .done
    lea     rdx, [toggleOn]
    cmp     eax, 1
    je      .done
    lea     rdx, [notRead]
.done:
    ret

scan_config:
    enter_frame
    cmp     dword [playerCount], 1
    ja      .ambiguous
    cmp     dword [playerFamily], 2
    je      .blue
    lea     rcx, [msiConfig]
    call    read_config
    test    eax, eax
    jnz     .msi_read
    cmp     dword [playerFamily], 1
    je      .done
.blue:
    lea     rcx, [blueConfig]
    call    read_config
    test    eax, eax
    jz      .done
    lea     rax, [blueName]
    jmp     .loaded
.msi_read:
    lea     rax, [msiName]
.loaded:
    mov     [rsp+96], rax
    store_text configForProbe, configPath
    call    find_instance
    test    eax, eax
    jz      .instance_missing
    lea     rcx, [valueProfile]
    lea     rdx, [fmtProfile]
    mov     r8, [rsp+96]
    lea     r9, [instanceName]
    call    wsprintfA

    lea     rcx, [highFpsField]
    call    config_number
    call    toggle_text
    lea     rcx, [valueHigh]
    call    lstrcpyA
    lea     rcx, [vsyncField]
    call    config_number
    call    toggle_text
    lea     rcx, [valueVsync]
    call    lstrcpyA
    lea     rcx, [maxFpsField]
    call    config_number
    test    eax, eax
    jle     .resolution
    mov     r8d, eax
    lea     rcx, [valueLimit]
    lea     rdx, [fmtLimit]
    call    wsprintfA
.resolution:
    lea     rcx, [widthField]
    call    config_number
    mov     [rsp+104], eax
    lea     rcx, [heightField]
    call    config_number
    mov     [rsp+108], eax
    cmp     dword [rsp+104], 0
    jle     .done
    test    eax, eax
    jle     .done
    lea     rcx, [valueResolutionShort]
    lea     rdx, [fmtResolution]
    mov     r8d, [rsp+104]
    mov     r9d, [rsp+108]
    call    wsprintfA
    lea     rcx, [dpiField]
    call    config_number
    lea     rdx, [fmtResolution]
    test    eax, eax
    jle     .write_resolution
    mov     [rsp+32], rax
    lea     rdx, [fmtResolutionDpi]
.write_resolution:
    lea     rcx, [valueResolution]
    mov     r8d, [rsp+104]
    mov     r9d, [rsp+108]
    call    wsprintfA
    jmp     .done
.ambiguous:
    store_text valueProfile, multiplePlayers
    jmp     .done
.instance_missing:
    lea     rcx, [valueProfile]
    mov     rdx, [rsp+96]
    call    lstrcpyA
.done:
    leave_frame

read_config:
    enter_frame
    mov     [rsp+96], rcx
    lea     rcx, [programDataVar]
    lea     rdx, [configPath]
    mov     r8d, 400
    call    GetEnvironmentVariableA
    test    eax, eax
    jz      .default_path
    cmp     eax, 400
    jb      .append
.default_path:
    store_text configPath, defaultData
.append:
    lea     rcx, [configPath]
    mov     rdx, [rsp+96]
    call    lstrcatA
    lea     rcx, [configPath]
    mov     edx, 0x80000000
    mov     r8d, 7
    xor     r9d, r9d
    mov     qword [rsp+32], 3
    mov     qword [rsp+40], 0x80
    mov     qword [rsp+48], 0
    call    CreateFileA
    cmp     rax, -1
    je      .fail
    mov     [rsp+96], rax
    mov     dword [rsp+112], 0
    mov     rcx, rax
    lea     rdx, [rsp+104]
    call    GetFileSizeEx
    test    eax, eax
    jz      .close
    mov     r8, [rsp+104]
    test    r8, r8
    jle     .close
    cmp     r8, CONFIG_LIMIT
    ja      .close
    mov     rcx, [rsp+96]
    lea     rdx, [configData]
    lea     r9, [rsp+112]
    mov     qword [rsp+32], 0
    call    ReadFile
    test    eax, eax
    jz      .failed_read
    mov     eax, [rsp+112]
    cmp     rax, [rsp+104]
    je      .terminate
.failed_read:
    mov     dword [rsp+112], 0
.terminate:
    lea     rdx, [configData]
    mov     eax, [rsp+112]
    mov     byte [rdx+rax], 0
.close:
    mov     rcx, [rsp+96]
    call    CloseHandle
    mov     eax, [rsp+112]
    leave_frame
.fail:
    xor     eax, eax
    leave_frame

ascii_prefix:
    mov     rax, rcx
.byte:
    mov     cl, [rdx]
    test    cl, cl
    jz      .done
    cmp     [rax], cl
    jne     .no_match
    inc     rax
    inc     rdx
    jmp     .byte
.no_match:
    xor     eax, eax
.done:
    ret

next_line:
    mov     rax, rcx
.byte:
    mov     dl, [rax]
    test    dl, dl
    jz      .eof
    inc     rax
    cmp     dl, 10
    jne     .byte
    ret
.eof:
    xor     eax, eax
    ret

find_instance:
    enter_frame
    lea     rax, [configData]
    cmp     byte [rax], 0xef
    jne     .line
    cmp     word [rax+1], 0xbfbb
    jne     .line
    add     rax, 3
.line:
    mov     [rsp+96], rax
    mov     rcx, rax
    lea     rdx, [instanceStem]
    call    ascii_prefix
    test    rax, rax
    jz      .advance
    mov     [rsp+104], rax
    mov     edx, 0
.name:
    cmp     edx, 63
    jae     .advance
    mov     cl, [rax+rdx]
    cmp     cl, '.'
    je      .suffix
    cmp     cl, 32
    jbe     .advance
    inc     edx
    jmp     .name
.suffix:
    test    edx, edx
    jz      .advance
    mov     [rsp+112], edx
    lea     rcx, [rax+rdx]
    lea     rdx, [firstFpsField]
    call    ascii_prefix
    test    rax, rax
    jz      .advance
    mov     rdx, [rsp+104]
    lea     rcx, [instanceName]
    mov     r8d, [rsp+112]
    xor     eax, eax
.copy_name:
    mov     r9b, [rdx+rax]
    mov     [rcx+rax], r9b
    inc     eax
    cmp     eax, r8d
    jb      .copy_name
    mov     byte [rcx+rax], 0
    store_text instancePrefix, instanceStem
    lea     rcx, [instancePrefix]
    lea     rdx, [instanceName]
    call    lstrcatA
    lea     rcx, [instancePrefix]
    call    lstrlenA
    lea     rdx, [instancePrefix]
    mov     word [rdx+rax], '.'
    mov     eax, 1
    leave_frame
.advance:
    mov     rcx, [rsp+96]
    call    next_line
    test    rax, rax
    jnz     .line
    leave_frame

config_number:
    enter_frame
    mov     [rsp+96], rcx
    store_text wantedKey, instancePrefix
    lea     rcx, [wantedKey]
    mov     rdx, [rsp+96]
    call    lstrcatA
    lea     rax, [configData]
    cmp     byte [rax], 0xef
    jne     .line
    cmp     word [rax+1], 0xbfbb
    jne     .line
    add     rax, 3
.line:
    mov     [rsp+96], rax
    mov     rcx, rax
    lea     rdx, [wantedKey]
    call    ascii_prefix
    test    rax, rax
    jnz     .parse
    mov     rcx, [rsp+96]
    call    next_line
    test    rax, rax
    jnz     .line
    jmp     .unavailable
.parse:
    xor     r8d, r8d
    cmp     byte [rax], '"'
    jne     .first_digit
    mov     r8d, 1
    inc     rax
.first_digit:
    movzx   ecx, byte [rax]
    sub     ecx, '0'
    cmp     ecx, 9
    ja      .unavailable
    xor     edx, edx
.digit:
    imul    edx, 10
    add     edx, ecx
    cmp     edx, 999999
    ja      .unavailable
    inc     rax
    movzx   ecx, byte [rax]
    sub     ecx, '0'
    cmp     ecx, 9
    jbe     .digit
    test    r8d, r8d
    jz      .end_value
    cmp     byte [rax], '"'
    jne     .unavailable
    inc     rax
.end_value:
    cmp     byte [rax], 32
    ja      .unavailable
    mov     eax, edx
    leave_frame
.unavailable:
    mov     eax, -1
    leave_frame

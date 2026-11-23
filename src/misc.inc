extern GetSystemInfo, GetPhysicallyInstalledSystemMemory, GetLogicalDrives, GetDriveTypeA, GetDiskFreeSpaceExA
extern GetProcAddress, GetCurrentProcess, OpenProcessToken, LookupPrivilegeValueA, AdjustTokenPrivileges
extern SetPriorityClass, SetProcessPriorityBoost, SetProcessInformation, WriteFile, CopyFileA, MoveFileExA
extern RegDeleteKeyValueA, RegDeleteKeyA, PowerSetActiveScheme, PowerDuplicateScheme
extern EnumDisplaySettingsA, ChangeDisplaySettingsA, GetSystemCpuSetInformation, SetProcessDefaultCpuSets
extern GetProcessWorkingSetSizeEx, SetProcessWorkingSetSizeEx, OpenSCManagerA, OpenServiceA, ControlService, CloseServiceHandle
extern WlanOpenHandle, WlanCloseHandle, WlanEnumInterfaces, WlanSetInterface, WlanFreeMemory
extern PowerReadACValueIndex, PowerReadDCValueIndex, PowerWriteACValueIndex, PowerWriteDCValueIndex

HKEY_LM        equ -2147483646
HKEY_CU        equ -2147483647
REG_ITEM_SIZE  equ 40
POWER_ITEMS    equ 7
SLE_REGS       equ 20
GPU_REGS       equ 3
SESSION_REGS   equ 1
SLE_CONFS      equ 4
FEATURES       equ 3
CONF_SIZE      equ 524288+64

%macro enter4 0
    push    rbx
    push    rsi
    push    rdi
    push    r12
    sub     rsp, 0x88
%endmacro

%macro leave4 0
    add     rsp, 0x88
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
%endmacro

%macro guid 11
    dd %1
    dw %2, %3
    db %4, %5, %6, %7, %8, %9, %10, %11
%endmacro

%macro reg_item 6
    dq %1, %2, %3
    dd %4, %6
    dq %5
%endmacro

%macro power_item 3
    dq %1, %2
    dd %3, 0
%endmacro

section .rdata
miscTab         db "MISC",0
miscHeading     db "MISC | INFO PC, SPECIAL FEATURE DAN LISENSI",0
specialHeading  db "SPECIAL FEATURE",0
creditLine      db "O3 / OffOrOn dibuat oleh iRELDevz  |  github.com/iRELDevz/O3",0
licenseLine     db "MIT License + Sale Restriction: bebas dipakai dan diubah, tidak boleh dijual tanpa izin. jsmn.h (c) Serge Zaitsev, MIT.",0
licenseButtonText db "Lihat lisensi",0
licenseTitle    db "Lisensi O3 / OffOrOn",0
licenseText     db "MIT License with Sale Restriction (O3 / OffOrOn)",13,10,"Copyright (c) 2026 iRELDevz",13,10,13,10
                db "Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated "
                db "documentation files (the ",34,"Software",34,"), to deal in the Software without restriction, including without "
                db "limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the "
                db "Software, subject to the following conditions:",13,10,13,10
                db "The above copyright notice and this permission notice shall be included in all copies or substantial portions "
                db "of the Software.",13,10,13,10
                db "SALE RESTRICTION. The right to sell is NOT granted, except to (a) persons with written permission from the owner "
                db "of the official repository, and (b) contributors whose pull request was merged into the official repository. "
                db "This restriction must be kept in all copies and modified versions.",13,10,13,10
                db "THE SOFTWARE IS PROVIDED ",34,"AS IS",34,", WITHOUT WARRANTY OF ANY KIND. IT CHANGES WINDOWS SETTINGS; USE IT AT "
                db "YOUR OWN RISK.",13,10,13,10
                db "src/vendor/jsmn.h is Copyright (c) 2010 Serge Zaitsev, plain MIT License.",13,10
                db "Kontak izin: https://github.com/iRELDevz",0

hwLabelCpu      db "CPU",0
hwLabelGpu0     db "GPU0",0
hwLabelGpu1     db "GPU1",0
hwLabelBoard    db "MOTHERBOARD",0
hwLabelRam      db "TOTAL RAM",0
hwLabelDisk     db "TOTAL MEMORY (STORAGE)",0
hwLabels        dq hwLabelCpu, hwLabelGpu0, hwLabelGpu1, hwLabelBoard, hwLabelRam, hwLabelDisk
hwValues        dq hwCpu, hwGpu0, hwGpu1, hwBoard, hwRam, hwDisk

spTitle0        db "O3SLE - Super Latency Engine",0
spTitle2        db "Anti-BugMouse",0
spTitle3        db "GPU Offload",0
spDetail0       db "Power, timer 0.5 ms, P-core, prioritas CPU/GPU, refresh max, WiFi, update dan sync ditahan selama main.",0
spDetail2       db "Mouse acceleration OFF, popup Sticky/Filter/Toggle Keys tidak muncul.",0
spDetail3       db "GPU dedicated, dekode ASTC di GPU, HAGS ON. Emulator harus tertutup.",0
spEffect0       db "Selalu: High performance, core parking OFF, CPU min 100%, turbo agresif, USB/PCIe/WiFi tanpa hemat daya, timer global, scheduler foreground, "
                db "Game DVR/Delivery Optimization OFF, refresh rate max, animasi OFF, keyboard repeat cepat, HD-Player fullscreen opt OFF + DPI, QoS DSCP 46, "
                db "scan Defender saat idle, emulator: core, FPS, V-Sync OFF. Saat HD-Player jalan: timer 0.5 ms, High, P-core, EcoQoS OFF, GPU High, RAM dikunci, "
                db "standby dibersihkan, aplikasi latar ke E-core, OneDrive/Drive dibekukan, update Windows dihentikan, WiFi scan OFF, notifikasi OFF. OFF = nilai lama.",0
spEffect2       db "Jika aktif: Enhance pointer precision OFF, gerakan mouse 1:1 dan aim tidak loncat saat mouse digerakkan cepat. Shift 5x atau ditahan "
                db "tidak memunculkan popup Sticky/Filter/Toggle Keys. Kecepatan pointer tidak berubah. Jika dimatikan: setelan lama dikembalikan.",0
spEffect3       db "Jika aktif: HD-Player dipaksa memakai GPU dedicated (bukan iGPU), dekode tekstur ASTC pindah ke GPU (CPU lebih lega, stutter efek berkurang), "
                db "optimasi game berjendela ON, Hardware-accelerated GPU scheduling ON (berlaku setelah restart). Emulator harus tertutup. Jika dimatikan: nilai lama dikembalikan.",0
spTitles        dq spTitle0, spTitle2, spTitle3
spDetails       dq spDetail0, spDetail2, spDetail3
spEffects       dq spEffect0, spEffect2, spEffect3
featOn          dq feat_sle_on, feat_mouse_on, feat_gpu_on
featOff         dq feat_sle_off, feat_mouse_off, feat_gpu_off

msgSleOn        db "200 - O3SLE aktif. Setelan permanen sudah diterapkan. Sisanya otomatis jalan saat HD-Player dibuka (O3 harus tetap terbuka).",0
msgSlePending   db "202 - O3SLE aktif. Setelan emulator diterapkan setelah HD-Player ditutup.",0
msgSlePartial   db "206 - O3SLE aktif sebagian. Beberapa setelan tidak tersedia di PC ini, sisanya sudah diterapkan.",0
msgSleOff       db "200 - O3SLE dimatikan. Power plan, registry, timer dan prioritas kembali ke nilai sebelumnya.",0
msgMouseOn      db "200 - Anti-BugMouse aktif: mouse 1:1, popup Sticky/Filter/Toggle Keys mati.",0
msgMouseOff     db "200 - Setelan mouse dan aksesibilitas kembali seperti semula.",0
msgGpuOn        db "200 - GPU Offload aktif. HD-Player di GPU dedicated, ASTC hardware. HAGS berlaku setelah restart PC.",0
msgGpuOff       db "200 - GPU Offload dimatikan, nilai lama dikembalikan. HAGS berlaku setelah restart PC.",0
msgEmuRunning   db "409 - Tutup emulator (HD-Player) dulu, lalu coba lagi. Setelan emulator hanya aman diubah saat emulator tertutup.",0
msgNoEmu        db "404 - MSI App Player / BlueStacks 5 tidak terdeteksi.",0
msgNoProfile    db "404 - Profil emulator belum terdeteksi. Klik Pindai ulang lalu coba lagi.",0
msgConfFail     db "500 - bluestacks.conf gagal dibaca atau ditulis. Tidak ada yang diubah.",0

specialKey      db 'Software\OffOrOn\Special',0
specialValue    db "Special",0
fmtBackup       db "%c%u",0
psName          db "ps",0
ptName          db "pt",0
mouseName       db "mouse",0
ntdllName       db "ntdll.dll",0
gdiName         db "gdi32.dll",0
timerProcName   db "NtSetTimerResolution",0
sysInfoProcName db "NtSetSystemInformation",0
gpuPrioProcName db "D3DKMTSetProcessSchedulingPriorityClass",0
profilePriv     db "SeProfileSingleProcessPrivilege",0

msiKey          db 'SOFTWARE\BlueStacks_msi5',0
nxtKey          db 'SOFTWARE\BlueStacks_nxt',0
installValue    db "InstallDir",0
dataValue       db "UserDefinedDir",0
confName        db '\bluestacks.conf',0
bakSuffix       db ".o3bak",0
tmpSuffix       db ".o3tmp",0
fmtConfKey      db 'bst.instance.%s.%s="',0
fmtEmu          db "Emulator: %s  |  %s",0
emuNone         db "Emulator: tidak terdeteksi",0
emuRunText      db "HD-Player berjalan",0
emuStopText     db "HD-Player tertutup",0

gpuKeyFmt       db 'SYSTEM\CurrentControlSet\Control\Class\{4d36e968-e325-11ce-bfc1-08002be10318}\%04u',0
driverDesc      db "DriverDesc",0
qwMemValue      db "HardwareInformation.qwMemorySize",0
memValue        db "HardwareInformation.MemorySize",0
microsoftText   db "Microsoft",0
biosKey         db 'HARDWARE\DESCRIPTION\System\BIOS',0
boardMaker      db "BaseBoardManufacturer",0
boardProduct    db "BaseBoardProduct",0
biosVersionName db "BIOSVersion",0
fmtCpu          db "%s (%u thread)",0
fmtGpu          db "%s  |  %u MB VRAM",0
fmtPlain        db "%s",0
fmtBoard        db "%s %s  |  BIOS %s",0
fmtRam          db "%u GB terpasang  |  terpakai %u.%u dari %u.%u GB",0
fmtDiskTotal    db "Total %u GB  |  ",0
fmtDisk         db "%c: bebas %u/%u GB   ",0

kernelKey       db 'SYSTEM\CurrentControlSet\Control\Session Manager\kernel',0
timerValue      db "GlobalTimerResolutionRequests",0
priorityKey     db 'SYSTEM\CurrentControlSet\Control\PriorityControl',0
separationValue db "Win32PrioritySeparation",0
profileKey      db 'SOFTWARE\Microsoft\Windows NT\CurrentVersion\Multimedia\SystemProfile',0
responsivenessValue db "SystemResponsiveness",0
gameStoreKey    db 'System\GameConfigStore',0
dvrValue        db "GameDVR_Enabled",0
audioKey        db 'Software\Microsoft\Multimedia\Audio',0
duckingValue    db "UserDuckingPreference",0
doKey           db 'SOFTWARE\Policies\Microsoft\Windows\DeliveryOptimization',0
doValue         db "DODownloadMode",0
gpuPrefKey      db 'Software\Microsoft\DirectX\UserGpuPreferences',0
globalValue     db "DirectXUserGlobalSettings",0
graphicsKey     db 'SYSTEM\CurrentControlSet\Control\GraphicsDrivers',0
hagsValue       db "HwSchMode",0
gpuPrefData     db "GpuPreference=2;SwapEffectUpgradeEnable=1;",0
GPU_PREF_LEN    equ $-gpuPrefData
globalData      db "SwapEffectUpgradeEnable=1;VRROptimizeEnable=1;",0
GLOBAL_LEN      equ $-globalData
dwZero          dd 0
dwOne           dd 1
dwTwo           dd 2
dwThree         dd 3
dwTen           dd 10
dw26            dd 0x26

astcKey         db "astc_decoding_mode",0
hardwareText    db "hardware",0
cpusKey         db "cpus",0
maxFpsKey       db "max_fps",0
vsyncKey        db "enable_vsync",0
highFpsKey      db "enable_high_fps",0
zeroText        db "0",0
oneText         db "1",0
fmtUnsigned     db "%u",0
hzName          db "hz",0
uiName          db "ui",0
suspendProcName db "NtSuspendProcess",0
resumeProcName  db "NtResumeProcess",0
layersKey       db 'Software\Microsoft\Windows NT\CurrentVersion\AppCompatFlags\Layers',0
layersData      db "~ DISABLEDXMAXIMIZEDWINDOWEDMODE HIGHDPIAWARE",0
LAYERS_LEN      equ $-layersData
qosKey          db 'SOFTWARE\Policies\Microsoft\Windows\QoS\O3 HD-Player',0
qosTcpKey       db 'SYSTEM\CurrentControlSet\Services\Tcpip\QoS',0
qosNla          db "Do not use NLA",0
qVersion        db "Version",0
qApp            db "Application Name",0
qProto          db "Protocol",0
qLPort          db "Local Port",0
qLIp            db "Local IP",0
qLPre           db "Local IP Prefix Length",0
qRPort          db "Remote Port",0
qRIp            db "Remote IP",0
qRPre           db "Remote IP Prefix Length",0
qDscp           db "DSCP Value",0
qThrottle       db "Throttle Rate",0
sVersion        db "1.0",0
sStar           db "*",0
sDscp           db "46",0
sMinusOne       db "-1",0
defenderKey     db 'SOFTWARE\Policies\Microsoft\Windows Defender\Scan',0
idleValue       db "ScanOnlyIfIdle",0
toastKey        db 'Software\Microsoft\Windows\CurrentVersion\PushNotifications',0
toastValue      db "ToastEnabled",0
svcWu           db "wuauserv",0
svcUso          db "UsoSvc",0
svcBits         db "BITS",0
svcDo           db "DoSvc",0
fOneDrive       db "OneDrive.exe",0
fGDrive         db "GoogleDriveFS.exe",0
fDropbox        db "Dropbox.exe",0
fMega           db "MEGAsync.exe",0
fICloud         db "iCloudDrive.exe",0
eChrome         db "chrome.exe",0
eEdge           db "msedge.exe",0
eFirefox        db "firefox.exe",0
eOpera          db "opera.exe",0
eBrave          db "brave.exe",0
eDiscord        db "Discord.exe",0
eSpotify        db "Spotify.exe",0
eSteam          db "steam.exe",0
eSteamWeb       db "steamwebhelper.exe",0
eEpic           db "EpicGamesLauncher.exe",0
eTelegram       db "Telegram.exe",0
eWhatsApp       db "WhatsApp.exe",0
eTeams          db "ms-teams.exe",0
eZoom           db "Zoom.exe",0

align 8
sleRegs:
    reg_item HKEY_LM, kernelKey, timerValue, 4, dwOne, 4
    reg_item HKEY_LM, priorityKey, separationValue, 4, dw26, 4
    reg_item HKEY_LM, profileKey, responsivenessValue, 4, dwTen, 4
    reg_item HKEY_CU, gameStoreKey, dvrValue, 4, dwZero, 4
    reg_item HKEY_CU, audioKey, duckingValue, 4, dwThree, 4
    reg_item HKEY_LM, doKey, doValue, 4, dwZero, 4
    reg_item HKEY_CU, layersKey, miscPlayer, 1, layersData, LAYERS_LEN
    reg_item HKEY_LM, defenderKey, idleValue, 4, dwOne, 4
    reg_item HKEY_LM, qosTcpKey, qosNla, 1, oneText, 2
    reg_item HKEY_LM, qosKey, qVersion, 1, sVersion, 4
    reg_item HKEY_LM, qosKey, qApp, 1, playerName, 14
    reg_item HKEY_LM, qosKey, qProto, 1, sStar, 2
    reg_item HKEY_LM, qosKey, qLPort, 1, sStar, 2
    reg_item HKEY_LM, qosKey, qLIp, 1, sStar, 2
    reg_item HKEY_LM, qosKey, qLPre, 1, sStar, 2
    reg_item HKEY_LM, qosKey, qRPort, 1, sStar, 2
    reg_item HKEY_LM, qosKey, qRIp, 1, sStar, 2
    reg_item HKEY_LM, qosKey, qRPre, 1, sStar, 2
    reg_item HKEY_LM, qosKey, qDscp, 1, sDscp, 3
    reg_item HKEY_LM, qosKey, qThrottle, 1, sMinusOne, 3
sessionRegs:
    reg_item HKEY_CU, toastKey, toastValue, 4, dwZero, 4
gpuRegs:
    reg_item HKEY_CU, gpuPrefKey, miscPlayer, 1, gpuPrefData, GPU_PREF_LEN
    reg_item HKEY_CU, gpuPrefKey, globalValue, 1, globalData, GLOBAL_LEN
    reg_item HKEY_LM, graphicsKey, hagsValue, 4, dwTwo, 4
sleConf:
    dq cpusKey, cpusText
    dq maxFpsKey, fpsText
    dq vsyncKey, zeroText
    dq highFpsKey, oneText
updateServices: dq svcWu, svcUso, svcBits, svcDo, 0
freezeList:     dq fOneDrive, fGDrive, fDropbox, fMega, fICloud, 0
ecoreList:      dq eChrome, eEdge, eFirefox, eOpera, eBrave, eDiscord, eSpotify, eSteam, eSteamWeb, eEpic, eTelegram, eWhatsApp, eTeams, eZoom, 0
gpuConf:
    dq astcKey, hardwareText
powerItems:
    power_item subProcessor, procMin, 100
    power_item subProcessor, coreParking, 100
    power_item subProcessor, boostMode, 2
    power_item subProcessor, eppSetting, 0
    power_item subUsb, usbSuspend, 0
    power_item subPcie, aspmSetting, 0
    power_item subWifi, wifiPower, 0

highScheme:   guid 0x8c5e7fda, 0xe8bf, 0x4a96, 0x9a, 0x85, 0xa6, 0xe2, 0x3a, 0x8c, 0x63, 0x5c
subProcessor: guid 0x54533251, 0x82be, 0x4824, 0x96, 0xc1, 0x47, 0xb6, 0x0b, 0x74, 0x0d, 0x00
procMin:      guid 0x893dee8e, 0x2bef, 0x41e0, 0x89, 0xc6, 0xb5, 0x5d, 0x09, 0x29, 0x96, 0x4c
coreParking:  guid 0x0cc5b647, 0xc1df, 0x4637, 0x89, 0x1a, 0xde, 0xc3, 0x5c, 0x31, 0x85, 0x83
boostMode:    guid 0xbe337238, 0x0d82, 0x4146, 0xa9, 0x60, 0x4f, 0x37, 0x49, 0xd4, 0x70, 0xc7
eppSetting:   guid 0x36687f9e, 0xe3a5, 0x4dbf, 0xb1, 0xdc, 0x15, 0xeb, 0x38, 0x1c, 0x68, 0x63
subUsb:       guid 0x2a737441, 0x1930, 0x4402, 0x8d, 0x77, 0xb2, 0xbe, 0xbb, 0xa3, 0x08, 0xa3
usbSuspend:   guid 0x48e6b7a6, 0x50f5, 0x4782, 0xa5, 0xd4, 0x53, 0xbb, 0x8f, 0x07, 0xe2, 0x26
subPcie:      guid 0x501a4d13, 0x42af, 0x4429, 0x9f, 0xd1, 0xa8, 0x21, 0x8c, 0x26, 0x8e, 0x20
aspmSetting:  guid 0xee12f906, 0xd277, 0x404b, 0xb6, 0xda, 0xe5, 0xfa, 0x1a, 0x57, 0x6d, 0xf5
subWifi:      guid 0x19cbb8fa, 0x5279, 0x450e, 0x9f, 0xac, 0x8a, 0x3d, 0x5f, 0xed, 0xd0, 0xc1
wifiPower:    guid 0x12bbebe6, 0x58d6, 0x4636, 0x95, 0xbb, 0x32, 0x17, 0xef, 0x86, 0x7c, 0x1a

section .bss
specialBits     resd 1
procSuspend     resq 1
procResume      resq 1
wlanHandle      resq 1
emuPending      resd 1
emuHz           resd 1
pCount          resd 1
eCount          resd 1
pIds            resd 64
eIds            resd 64
frozenCount     resd 1
frozenPids      resd 32
ecoreCount      resd 1
ecorePids       resd 64
uiBuf           resd 6
fpsText         resb 16
cpusText        resb 16
devMode         resb 224
devMode2        resb 224
cpuSetBuf       resb 16384
miscFocus       resd 1
miscMsg         resq 1
miscEmuName     resq 1
procTimer       resq 1
procSysInfo     resq 1
procGpuPrio     resq 1
miscQword       resq 4
miscSize        resd 1
confLen         resd 1
miscPidCount    resd 1
miscPids        resd 8
boostCount      resd 1
boostPids       resd 8
timerOn         resd 1
purgeTick       resd 1
miscScheme      resb 16
miscSchemeOld   resb 16
miscSysInfo     resb 48
miscMemStatus   resb 64
miscEntry       resb 304
miscDriveRoot   resb 4
miscName        resb 16
miscKeyBuf      resb 192
miscTemp        resb 160
miscTemp2       resb 160
miscTemp3       resb 160
miscPlayer      resb 300
miscConfPath    resb 300
miscTempPath    resb 320
miscEmuText     resb 128
miscBuf         resb 1104
hwCpuRaw        resb 64
hwCpu           resb 160
hwGpu0          resb 160
hwGpu1          resb 160
hwBoard         resb 200
hwRam           resb 128
hwDisk          resb 320
hwDiskList      resb 256
specialButtons  resq 4
confA           resb CONF_SIZE
confB           resb CONF_SIZE

section .text

backup_name:
    enter_frame
    mov     r8d, ecx
    mov     r9d, edx
    lea     rcx, [miscName]
    lea     rdx, [fmtBackup]
    call    wsprintfA
    lea     rax, [miscName]
    leave_frame

has_backup:
    enter_frame
    mov     r8, rcx
    mov     rcx, HKEY_CU
    lea     rdx, [specialKey]
    mov     r9d, 0xffff
    mov     qword [rsp+32], 0
    mov     qword [rsp+40], 0
    mov     qword [rsp+48], 0
    call    RegGetValueA
    xor     ecx, ecx
    test    eax, eax
    sete    cl
    mov     eax, ecx
    leave_frame

read_backup:
    enter_frame
    mov     [rsp+40], rdx
    mov     [miscSize], r8d
    lea     rax, [miscSize]
    mov     [rsp+48], rax
    mov     qword [rsp+32], 0
    mov     r8, rcx
    mov     rcx, HKEY_CU
    lea     rdx, [specialKey]
    mov     r9d, 0xffff
    call    RegGetValueA
    xor     ecx, ecx
    test    eax, eax
    sete    cl
    mov     eax, ecx
    leave_frame

write_backup:
    enter_frame
    mov     [rsp+32], rdx
    mov     [rsp+40], r8
    mov     r8, rcx
    mov     rcx, HKEY_CU
    lea     rdx, [specialKey]
    mov     r9d, 3
    call    RegSetKeyValueA
    leave_frame

drop_backup:
    enter_frame
    mov     r8, rcx
    mov     rcx, HKEY_CU
    lea     rdx, [specialKey]
    call    RegDeleteKeyValueA
    leave_frame

reg_apply:
    enter4
    mov     rbx, rcx
    mov     esi, edx
    mov     edi, r8d
    xor     r12d, r12d
    mov     dword [rsp+120], 1
.loop:
    cmp     r12d, esi
    jae     .done
    mov     rax, [rbx+16]
    cmp     byte [rax], 0
    je      .next
    mov     ecx, edi
    mov     edx, r12d
    call    backup_name
    mov     rcx, rax
    call    has_backup
    test    eax, eax
    jnz     .write
    mov     dword [miscSize], 1096
    lea     rax, [miscSize]
    mov     [rsp+48], rax
    lea     rax, [miscBuf+4]
    mov     [rsp+40], rax
    lea     rax, [miscBuf]
    mov     [rsp+32], rax
    mov     rcx, [rbx]
    mov     rdx, [rbx+8]
    mov     r8, [rbx+16]
    mov     r9d, 0x1000ffff
    call    RegGetValueA
    test    eax, eax
    jz      .have_old
    mov     dword [miscBuf], 0
    mov     dword [miscSize], 0
.have_old:
    lea     rcx, [miscName]
    lea     rdx, [miscBuf]
    mov     r8d, [miscSize]
    add     r8d, 4
    call    write_backup
.write:
    mov     rax, [rbx+32]
    mov     [rsp+32], rax
    mov     eax, [rbx+28]
    mov     [rsp+40], rax
    mov     rcx, [rbx]
    mov     rdx, [rbx+8]
    mov     r8, [rbx+16]
    mov     r9d, [rbx+24]
    call    RegSetKeyValueA
    test    eax, eax
    jz      .next
    mov     dword [rsp+120], 0
.next:
    add     rbx, REG_ITEM_SIZE
    inc     r12d
    jmp     .loop
.done:
    mov     eax, [rsp+120]
    leave4

reg_revert:
    enter4
    mov     rbx, rcx
    mov     esi, edx
    mov     edi, r8d
    xor     r12d, r12d
.loop:
    cmp     r12d, esi
    jae     .done
    mov     rax, [rbx+16]
    cmp     byte [rax], 0
    je      .next
    mov     ecx, edi
    mov     edx, r12d
    call    backup_name
    mov     rcx, rax
    lea     rdx, [miscBuf]
    mov     r8d, 1100
    call    read_backup
    test    eax, eax
    jz      .next
    cmp     dword [miscBuf], 0
    jne     .put
    mov     rcx, [rbx]
    mov     rdx, [rbx+8]
    mov     r8, [rbx+16]
    call    RegDeleteKeyValueA
    jmp     .drop
.put:
    lea     rax, [miscBuf+4]
    mov     [rsp+32], rax
    mov     eax, [miscSize]
    sub     eax, 4
    mov     [rsp+40], rax
    mov     rcx, [rbx]
    mov     rdx, [rbx+8]
    mov     r8, [rbx+16]
    mov     r9d, [miscBuf]
    call    RegSetKeyValueA
.drop:
    lea     rcx, [miscName]
    call    drop_backup
.next:
    add     rbx, REG_ITEM_SIZE
    inc     r12d
    jmp     .loop
.done:
    mov     eax, 1
    leave4

power_apply:
    enter4
    xor     ecx, ecx
    lea     rdx, [rsp+96]
    call    PowerGetActiveScheme
    test    eax, eax
    jnz     .fail
    mov     rcx, [rsp+96]
    movups  xmm0, [rcx]
    movups  [miscSchemeOld], xmm0
    call    LocalFree
    lea     rcx, [psName]
    call    has_backup
    test    eax, eax
    jnz     .set_high
    lea     rcx, [psName]
    lea     rdx, [miscSchemeOld]
    mov     r8d, 16
    call    write_backup
.set_high:
    xor     ecx, ecx
    lea     rdx, [highScheme]
    call    PowerSetActiveScheme
    test    eax, eax
    jz      .current
    mov     qword [rsp+96], 0
    xor     ecx, ecx
    lea     rdx, [highScheme]
    lea     r8, [rsp+96]
    call    PowerDuplicateScheme
    test    eax, eax
    jnz     .current
    xor     ecx, ecx
    mov     rdx, [rsp+96]
    call    PowerSetActiveScheme
    mov     rcx, [rsp+96]
    call    LocalFree
.current:
    xor     ecx, ecx
    lea     rdx, [rsp+96]
    call    PowerGetActiveScheme
    test    eax, eax
    jnz     .fail
    mov     rcx, [rsp+96]
    movups  xmm0, [rcx]
    movups  [miscScheme], xmm0
    call    LocalFree
    lea     rcx, [ptName]
    call    has_backup
    test    eax, eax
    jnz     .items
    lea     rcx, [ptName]
    lea     rdx, [miscScheme]
    mov     r8d, 16
    call    write_backup
.items:
    lea     rbx, [powerItems]
    xor     r12d, r12d
.item:
    cmp     r12d, POWER_ITEMS
    jae     .apply
    mov     ecx, 'p'
    mov     edx, r12d
    call    backup_name
    mov     rcx, rax
    call    has_backup
    test    eax, eax
    jnz     .write
    lea     rax, [miscQword]
    mov     [rsp+32], rax
    xor     ecx, ecx
    lea     rdx, [miscScheme]
    mov     r8, [rbx]
    mov     r9, [rbx+8]
    call    PowerReadACValueIndex
    test    eax, eax
    jnz     .next
    lea     rax, [miscQword+4]
    mov     [rsp+32], rax
    xor     ecx, ecx
    lea     rdx, [miscScheme]
    mov     r8, [rbx]
    mov     r9, [rbx+8]
    call    PowerReadDCValueIndex
    test    eax, eax
    jz      .save
    mov     eax, [miscQword]
    mov     [miscQword+4], eax
.save:
    lea     rcx, [miscName]
    lea     rdx, [miscQword]
    mov     r8d, 8
    call    write_backup
.write:
    mov     eax, [rbx+16]
    mov     [rsp+32], rax
    xor     ecx, ecx
    lea     rdx, [miscScheme]
    mov     r8, [rbx]
    mov     r9, [rbx+8]
    call    PowerWriteACValueIndex
    mov     eax, [rbx+16]
    mov     [rsp+32], rax
    xor     ecx, ecx
    lea     rdx, [miscScheme]
    mov     r8, [rbx]
    mov     r9, [rbx+8]
    call    PowerWriteDCValueIndex
.next:
    add     rbx, 24
    inc     r12d
    jmp     .item
.apply:
    xor     ecx, ecx
    lea     rdx, [miscScheme]
    call    PowerSetActiveScheme
    mov     eax, 1
    leave4
.fail:
    xor     eax, eax
    leave4

power_revert:
    enter4
    lea     rcx, [ptName]
    lea     rdx, [miscScheme]
    mov     r8d, 16
    call    read_backup
    test    eax, eax
    jz      .scheme
    lea     rbx, [powerItems]
    xor     r12d, r12d
.item:
    cmp     r12d, POWER_ITEMS
    jae     .apply
    mov     ecx, 'p'
    mov     edx, r12d
    call    backup_name
    mov     rcx, rax
    lea     rdx, [miscQword]
    mov     r8d, 8
    call    read_backup
    test    eax, eax
    jz      .next
    mov     eax, [miscQword]
    mov     [rsp+32], rax
    xor     ecx, ecx
    lea     rdx, [miscScheme]
    mov     r8, [rbx]
    mov     r9, [rbx+8]
    call    PowerWriteACValueIndex
    mov     eax, [miscQword+4]
    mov     [rsp+32], rax
    xor     ecx, ecx
    lea     rdx, [miscScheme]
    mov     r8, [rbx]
    mov     r9, [rbx+8]
    call    PowerWriteDCValueIndex
    lea     rcx, [miscName]
    call    drop_backup
.next:
    add     rbx, 24
    inc     r12d
    jmp     .item
.apply:
    xor     ecx, ecx
    lea     rdx, [miscScheme]
    call    PowerSetActiveScheme
    lea     rcx, [ptName]
    call    drop_backup
.scheme:
    lea     rcx, [psName]
    lea     rdx, [miscSchemeOld]
    mov     r8d, 16
    call    read_backup
    test    eax, eax
    jz      .done
    xor     ecx, ecx
    lea     rdx, [miscSchemeOld]
    call    PowerSetActiveScheme
    lea     rcx, [psName]
    call    drop_backup
.done:
    mov     eax, 1
    leave4

mouse_read:
    enter_frame
    mov     ecx, 3
    xor     edx, edx
    lea     r8, [miscBuf]
    xor     r9d, r9d
    call    SystemParametersInfoA
    mov     dword [miscBuf+12], 8
    mov     ecx, 0x3a
    mov     edx, 8
    lea     r8, [miscBuf+12]
    xor     r9d, r9d
    call    SystemParametersInfoA
    mov     dword [miscBuf+20], 24
    mov     ecx, 0x32
    mov     edx, 24
    lea     r8, [miscBuf+20]
    xor     r9d, r9d
    call    SystemParametersInfoA
    mov     dword [miscBuf+44], 8
    mov     ecx, 0x34
    mov     edx, 8
    lea     r8, [miscBuf+44]
    xor     r9d, r9d
    call    SystemParametersInfoA
    leave_frame

mouse_write:
    enter_frame
    mov     ecx, 4
    xor     edx, edx
    lea     r8, [miscBuf]
    mov     r9d, 3
    call    SystemParametersInfoA
    mov     [rsp+96], eax
    mov     ecx, 0x3b
    mov     edx, 8
    lea     r8, [miscBuf+12]
    mov     r9d, 3
    call    SystemParametersInfoA
    mov     ecx, 0x33
    mov     edx, 24
    lea     r8, [miscBuf+20]
    mov     r9d, 3
    call    SystemParametersInfoA
    mov     ecx, 0x35
    mov     edx, 8
    lea     r8, [miscBuf+44]
    mov     r9d, 3
    call    SystemParametersInfoA
    mov     eax, [rsp+96]
    leave_frame

feat_mouse_on:
    enter_frame
    call    mouse_read
    lea     rcx, [mouseName]
    call    has_backup
    test    eax, eax
    jnz     .set
    lea     rcx, [mouseName]
    lea     rdx, [miscBuf]
    mov     r8d, 52
    call    write_backup
.set:
    xor     eax, eax
    mov     [miscBuf], eax
    mov     [miscBuf+4], eax
    mov     [miscBuf+8], eax
    and     dword [miscBuf+16], ~4
    and     dword [miscBuf+24], ~4
    and     dword [miscBuf+48], ~4
    call    mouse_write
    lea     rdx, [msgMouseOn]
    mov     [miscMsg], rdx
    leave_frame

feat_mouse_off:
    enter_frame
    lea     rcx, [mouseName]
    lea     rdx, [miscBuf]
    mov     r8d, 64
    call    read_backup
    test    eax, eax
    jz      .done
    call    mouse_write
    lea     rcx, [mouseName]
    call    drop_backup
.done:
    lea     rdx, [msgMouseOff]
    mov     [miscMsg], rdx
    mov     eax, 1
    leave_frame

emulator_paths:
    enter_frame
    mov     [rsp+96], rcx
    mov     dword [miscSize], 260
    lea     rax, [miscSize]
    mov     [rsp+48], rax
    lea     rax, [miscPlayer]
    mov     [rsp+40], rax
    mov     qword [rsp+32], 0
    mov     rcx, HKEY_LM
    mov     rdx, [rsp+96]
    lea     r8, [installValue]
    mov     r9d, 2
    call    RegGetValueA
    test    eax, eax
    jnz     .fail
    lea     rcx, [miscPlayer]
    call    lstrlenA
    test    eax, eax
    jz      .fail
    lea     rdx, [miscPlayer]
    cmp     byte [rdx+rax-1], '\'
    je      .exe
    mov     word [rdx+rax], '\'
.exe:
    lea     rcx, [miscPlayer]
    lea     rdx, [playerName]
    call    lstrcatA
    mov     dword [miscSize], 260
    lea     rax, [miscSize]
    mov     [rsp+48], rax
    lea     rax, [miscConfPath]
    mov     [rsp+40], rax
    mov     qword [rsp+32], 0
    mov     rcx, HKEY_LM
    mov     rdx, [rsp+96]
    lea     r8, [dataValue]
    mov     r9d, 2
    call    RegGetValueA
    test    eax, eax
    jnz     .no_conf
    lea     rcx, [miscConfPath]
    lea     rdx, [confName]
    call    lstrcatA
    mov     eax, 1
    leave_frame
.no_conf:
    mov     byte [miscConfPath], 0
    mov     eax, 1
    leave_frame
.fail:
    mov     byte [miscPlayer], 0
    mov     byte [miscConfPath], 0
    xor     eax, eax
    leave_frame

misc_emulator:
    enter_frame
    mov     qword [miscEmuName], 0
    lea     rcx, [msiKey]
    call    emulator_paths
    test    eax, eax
    jz      .nxt
    lea     rax, [msiName]
    mov     [miscEmuName], rax
    leave_frame
.nxt:
    lea     rcx, [nxtKey]
    call    emulator_paths
    test    eax, eax
    jz      .done
    lea     rax, [blueName]
    mov     [miscEmuName], rax
.done:
    leave_frame

misc_scan_players:
    enter4
    mov     dword [miscPidCount], 0
    mov     ecx, 2
    xor     edx, edx
    call    CreateToolhelp32Snapshot
    cmp     rax, -1
    je      .done
    mov     rbx, rax
    mov     dword [miscEntry], 304
    mov     rcx, rbx
    lea     rdx, [miscEntry]
    call    Process32First
    test    eax, eax
    jz      .close
.loop:
    lea     rcx, [miscEntry+44]
    lea     rdx, [playerName]
    call    lstrcmpiA
    test    eax, eax
    jnz     .next
    mov     eax, [miscPidCount]
    cmp     eax, 8
    jae     .next
    mov     edx, [miscEntry+8]
    lea     rcx, [miscPids]
    mov     [rcx+rax*4], edx
    inc     dword [miscPidCount]
.next:
    mov     rcx, rbx
    lea     rdx, [miscEntry]
    call    Process32Next
    test    eax, eax
    jnz     .loop
.close:
    mov     rcx, rbx
    call    CloseHandle
.done:
    leave4

misc_emu_text:
    enter_frame
    call    misc_scan_players
    mov     r8, [miscEmuName]
    test    r8, r8
    jz      .none
    lea     r9, [emuStopText]
    cmp     dword [miscPidCount], 0
    je      .fmt
    lea     r9, [emuRunText]
.fmt:
    lea     rcx, [miscEmuText]
    lea     rdx, [fmtEmu]
    call    wsprintfA
    leave_frame
.none:
    lea     rcx, [miscEmuText]
    lea     rdx, [emuNone]
    call    lstrcpyA
    leave_frame

conf_ready:
    enter_frame
    lea     rax, [msgNoEmu]
    cmp     byte [miscConfPath], 0
    je      .no
    lea     rax, [msgNoProfile]
    cmp     byte [instanceName], 0
    je      .no
    call    misc_scan_players
    lea     rax, [msgEmuRunning]
    cmp     dword [miscPidCount], 0
    jne     .no
    mov     eax, 1
    leave_frame
.no:
    mov     [miscMsg], rax
    xor     eax, eax
    leave_frame

conf_load:
    enter4
    lea     rcx, [miscConfPath]
    mov     edx, 0x80000000
    mov     r8d, 3
    xor     r9d, r9d
    mov     qword [rsp+32], 3
    mov     qword [rsp+40], 0
    mov     qword [rsp+48], 0
    call    CreateFileA
    cmp     rax, -1
    je      .fail
    mov     rbx, rax
    mov     rcx, rbx
    lea     rdx, [miscQword]
    call    GetFileSizeEx
    test    eax, eax
    jz      .close_fail
    cmp     qword [miscQword], 524288
    ja      .close_fail
    mov     rcx, rbx
    lea     rdx, [confA]
    mov     r8d, [miscQword]
    lea     r9, [miscSize]
    mov     qword [rsp+32], 0
    call    ReadFile
    test    eax, eax
    jz      .close_fail
    mov     eax, [miscSize]
    cmp     eax, [miscQword]
    jne     .close_fail
    mov     [confLen], eax
    lea     rdx, [confA]
    mov     byte [rdx+rax], 0
    mov     rcx, rbx
    call    CloseHandle
    mov     eax, 1
    leave4
.close_fail:
    mov     rcx, rbx
    call    CloseHandle
.fail:
    xor     eax, eax
    leave4

conf_find:
    enter_frame
    lea     rcx, [confA]
.again:
    lea     rdx, [miscKeyBuf]
    call    StrStrIA
    test    rax, rax
    jz      .done
    lea     rcx, [confA]
    cmp     rax, rcx
    je      .start
    cmp     byte [rax-1], 10
    je      .start
    lea     rcx, [rax+1]
    jmp     .again
.start:
    mov     [rsp+96], rax
    lea     rcx, [miscKeyBuf]
    call    lstrlenA
    mov     rcx, [rsp+96]
    add     rcx, rax
    xor     edx, edx
.scan:
    mov     al, [rcx+rdx]
    test    al, al
    jz      .bad
    cmp     al, 10
    je      .bad
    cmp     al, '"'
    je      .found
    inc     edx
    cmp     edx, 200
    jb      .scan
.bad:
    xor     eax, eax
    leave_frame
.found:
    mov     rax, rcx
.done:
    leave_frame

conf_set:
    enter4
    mov     rsi, rdx
    mov     r9, rcx
    lea     r8, [instanceName]
    lea     rcx, [miscKeyBuf]
    lea     rdx, [fmtConfKey]
    call    wsprintfA
    call    conf_find
    test    rax, rax
    jz      .fail
    mov     rbx, rax
    mov     r12d, edx
    lea     r8, [miscBuf]
    xor     ecx, ecx
.old:
    cmp     ecx, r12d
    jae     .old_done
    mov     al, [rbx+rcx]
    mov     [r8+rcx], al
    inc     ecx
    jmp     .old
.old_done:
    mov     byte [r8+rcx], 0
    lea     r8, [confA]
    lea     r9, [confB]
    mov     r10, rbx
    sub     r10, r8
    xor     ecx, ecx
.pre:
    cmp     rcx, r10
    jae     .new
    mov     al, [r8+rcx]
    mov     [r9+rcx], al
    inc     rcx
    jmp     .pre
.new:
    mov     rdi, rcx
    xor     edx, edx
.nv:
    mov     al, [rsi+rdx]
    test    al, al
    jz      .tail
    mov     [r9+rdi], al
    inc     rdi
    inc     rdx
    jmp     .nv
.tail:
    mov     rcx, r10
    add     rcx, r12
    mov     r11d, [confLen]
.t:
    cmp     rcx, r11
    jae     .end
    mov     al, [r8+rcx]
    mov     [r9+rdi], al
    inc     rcx
    inc     rdi
    cmp     rdi, CONF_SIZE-2
    jae     .fail
    jmp     .t
.end:
    mov     byte [r9+rdi], 0
    mov     [confLen], edi
    xor     ecx, ecx
.back:
    cmp     rcx, rdi
    ja      .ok
    mov     al, [r9+rcx]
    mov     [r8+rcx], al
    inc     rcx
    jmp     .back
.ok:
    mov     eax, 1
    leave4
.fail:
    xor     eax, eax
    leave4

conf_save:
    enter4
    lea     rcx, [miscTempPath]
    lea     rdx, [miscConfPath]
    call    lstrcpyA
    lea     rcx, [miscTempPath]
    lea     rdx, [bakSuffix]
    call    lstrcatA
    lea     rcx, [miscConfPath]
    lea     rdx, [miscTempPath]
    mov     r8d, 1
    call    CopyFileA
    lea     rcx, [miscTempPath]
    lea     rdx, [miscConfPath]
    call    lstrcpyA
    lea     rcx, [miscTempPath]
    lea     rdx, [tmpSuffix]
    call    lstrcatA
    lea     rcx, [miscTempPath]
    mov     edx, 0x40000000
    xor     r8d, r8d
    xor     r9d, r9d
    mov     qword [rsp+32], 2
    mov     qword [rsp+40], 0x80
    mov     qword [rsp+48], 0
    call    CreateFileA
    cmp     rax, -1
    je      .fail
    mov     rbx, rax
    mov     rcx, rbx
    lea     rdx, [confA]
    mov     r8d, [confLen]
    lea     r9, [miscSize]
    mov     qword [rsp+32], 0
    call    WriteFile
    mov     esi, eax
    mov     rcx, rbx
    call    CloseHandle
    test    esi, esi
    jz      .fail
    mov     eax, [miscSize]
    cmp     eax, [confLen]
    jne     .fail
    lea     rcx, [miscTempPath]
    lea     rdx, [miscConfPath]
    mov     r8d, 9
    call    MoveFileExA
    test    eax, eax
    jz      .fail
    mov     eax, 1
    leave4
.fail:
    xor     eax, eax
    leave4

conf_apply:
    enter4
    mov     rbx, rcx
    mov     esi, edx
    mov     edi, r8d
    call    conf_load
    test    eax, eax
    jz      .fail
    xor     r12d, r12d
.loop:
    cmp     r12d, esi
    jae     .save
    mov     rcx, [rbx]
    mov     rdx, [rbx+8]
    call    conf_set
    test    eax, eax
    jz      .fail
    mov     ecx, edi
    mov     edx, r12d
    call    backup_name
    mov     rcx, rax
    call    has_backup
    test    eax, eax
    jnz     .next
    lea     rcx, [miscBuf]
    call    lstrlenA
    lea     r8d, [eax+1]
    lea     rcx, [miscName]
    lea     rdx, [miscBuf]
    call    write_backup
.next:
    add     rbx, 16
    inc     r12d
    jmp     .loop
.save:
    call    conf_save
    leave4
.fail:
    xor     eax, eax
    leave4

conf_revert:
    enter4
    mov     [rsp+112], rcx
    mov     rbx, rcx
    mov     esi, edx
    mov     edi, r8d
    call    conf_load
    test    eax, eax
    jz      .fail
    mov     dword [rsp+120], 0
    xor     r12d, r12d
.loop:
    cmp     r12d, esi
    jae     .save
    mov     ecx, edi
    mov     edx, r12d
    call    backup_name
    mov     rcx, rax
    lea     rdx, [miscTemp]
    mov     r8d, 96
    call    read_backup
    test    eax, eax
    jz      .next
    mov     rcx, [rbx]
    lea     rdx, [miscTemp]
    call    conf_set
    mov     dword [rsp+120], 1
.next:
    add     rbx, 16
    inc     r12d
    jmp     .loop
.save:
    cmp     dword [rsp+120], 0
    je      .drop
    call    conf_save
    test    eax, eax
    jz      .fail
.drop:
    xor     r12d, r12d
.d:
    cmp     r12d, esi
    jae     .ok
    mov     ecx, edi
    mov     edx, r12d
    call    backup_name
    mov     rcx, rax
    call    drop_backup
    inc     r12d
    jmp     .d
.ok:
    mov     eax, 1
    leave4
.fail:
    xor     eax, eax
    leave4

feat_sle_on:
    enter_frame
    call    power_apply
    mov     [rsp+96], eax
    lea     rcx, [sleRegs]
    mov     edx, SLE_REGS
    mov     r8d, 'S'
    call    reg_apply
    and     eax, [rsp+96]
    mov     [rsp+96], eax
    call    ui_apply
    call    hz_apply
    cmp     eax, 120
    jae     .hz
    mov     eax, 120
.hz:
    mov     [emuHz], eax
    mov     dword [emuPending], 0
    mov     ecx, eax
    call    emu_conf_apply
    test    eax, eax
    jnz     .result
    cmp     dword [miscPidCount], 0
    je      .result
    mov     dword [emuPending], 1
    lea     rdx, [msgSlePending]
    jmp     .msg
.result:
    lea     rdx, [msgSleOn]
    cmp     dword [rsp+96], 0
    jne     .msg
    lea     rdx, [msgSlePartial]
.msg:
    mov     [miscMsg], rdx
    mov     dword [purgeTick], 0
    mov     eax, 1
    leave_frame

feat_sle_off:
    enter_frame
    call    session_stop
    lea     rcx, [sleRegs]
    mov     edx, SLE_REGS
    mov     r8d, 'S'
    call    reg_revert
    mov     rcx, HKEY_LM
    lea     rdx, [qosKey]
    call    RegDeleteKeyA
    call    ui_revert
    call    hz_revert
    call    power_revert
    mov     dword [emuPending], 0
    call    conf_ready
    test    eax, eax
    jz      .later
    lea     rcx, [sleConf]
    mov     edx, SLE_CONFS
    mov     r8d, 'E'
    call    conf_revert
    jmp     .msg
.later:
    cmp     dword [miscPidCount], 0
    je      .msg
    mov     dword [emuPending], 2
.msg:
    lea     rdx, [msgSleOff]
    mov     [miscMsg], rdx
    mov     eax, 1
    leave_frame

feat_gpu_on:
    enter_frame
    call    conf_ready
    test    eax, eax
    jz      .done
    lea     rcx, [gpuConf]
    mov     edx, 1
    mov     r8d, 'G'
    call    conf_apply
    lea     rdx, [msgConfFail]
    mov     [miscMsg], rdx
    test    eax, eax
    jz      .done
    lea     rcx, [gpuRegs]
    mov     edx, GPU_REGS
    mov     r8d, 'R'
    call    reg_apply
    lea     rdx, [msgGpuOn]
    mov     [miscMsg], rdx
    mov     eax, 1
.done:
    leave_frame

feat_gpu_off:
    enter_frame
    call    conf_ready
    test    eax, eax
    jz      .done
    lea     rcx, [gpuConf]
    mov     edx, 1
    mov     r8d, 'G'
    call    conf_revert
    lea     rdx, [msgConfFail]
    mov     [miscMsg], rdx
    test    eax, eax
    jz      .done
    lea     rcx, [gpuRegs]
    mov     edx, GPU_REGS
    mov     r8d, 'R'
    call    reg_revert
    lea     rdx, [msgGpuOff]
    mov     [miscMsg], rdx
    mov     eax, 1
.done:
    leave_frame

save_special:
    enter_frame
    lea     rax, [specialBits]
    mov     [rsp+32], rax
    mov     qword [rsp+40], 4
    mov     rcx, HKEY_CU
    lea     rdx, [viewKey]
    lea     r8, [specialValue]
    mov     r9d, 4
    call    RegSetKeyValueA
    leave_frame

load_special:
    enter_frame
    mov     dword [specialBits], 0
    mov     dword [miscSize], 4
    lea     rax, [miscSize]
    mov     [rsp+48], rax
    lea     rax, [specialBits]
    mov     [rsp+40], rax
    mov     qword [rsp+32], 0
    mov     rcx, HKEY_CU
    lea     rdx, [viewKey]
    lea     r8, [specialValue]
    mov     r9d, 0x10
    call    RegGetValueA
    and     dword [specialBits], 7
    leave_frame

misc_toggle:
    enter_frame
    mov     [miscFocus], ecx
    mov     [rsp+96], ecx
    call    misc_emulator
    mov     ecx, [rsp+96]
    lea     rax, [featOn]
    bt      dword [specialBits], ecx
    jnc     .call
    lea     rax, [featOff]
.call:
    mov     rax, [rax+rcx*8]
    call    rax
    test    eax, eax
    jz      .msg
    mov     ecx, [rsp+96]
    btc     dword [specialBits], ecx
    call    save_special
.msg:
    mov     ecx, [rsp+96]
    lea     rax, [spTitles]
    mov     rax, [rax+rcx*8]
    mov     [applyTitle], rax
    mov     rax, [miscMsg]
    mov     [applyMessage], rax
    call    misc_emu_text
    leave_frame

misc_restore_all:
    enter4
    call    misc_emulator
    xor     ebx, ebx
.loop:
    cmp     ebx, FEATURES
    jae     .save
    bt      dword [specialBits], ebx
    jnc     .next
    lea     rax, [featOff]
    mov     rax, [rax+rbx*8]
    call    rax
    test    eax, eax
    jz      .next
    btr     dword [specialBits], ebx
.next:
    inc     ebx
    jmp     .loop
.save:
    call    save_special
    leave4

misc_boost:
    enter_frame
    mov     [rsp+96], edx
    mov     r8d, ecx
    mov     ecx, 0x3300
    xor     edx, edx
    call    OpenProcess
    test    rax, rax
    jz      .done
    mov     [rsp+104], rax
    mov     rcx, rax
    mov     edx, 0x20
    cmp     dword [rsp+96], 0
    je      .priority
    mov     edx, 0x80
.priority:
    call    SetPriorityClass
    mov     rcx, [rsp+104]
    xor     edx, edx
    call    SetProcessPriorityBoost
    cmp     dword [pCount], 0
    je      .ws
    mov     rcx, [rsp+104]
    xor     edx, edx
    xor     r8d, r8d
    cmp     dword [rsp+96], 0
    je      .sets
    lea     rdx, [pIds]
    mov     r8d, [pCount]
.sets:
    call    SetProcessDefaultCpuSets
.ws:
    mov     rcx, [rsp+104]
    lea     rdx, [rsp+112]
    lea     r8, [rsp+120]
    lea     r9, [rsp+128]
    call    GetProcessWorkingSetSizeEx
    test    eax, eax
    jz      .throttle
    mov     r9d, 10
    cmp     dword [rsp+96], 0
    je      .ws_set
    mov     rax, 256 << 20
    cmp     [rsp+112], rax
    jae     .min_ok
    mov     [rsp+112], rax
.min_ok:
    mov     rax, [rsp+112]
    add     rax, 64 << 20
    cmp     [rsp+120], rax
    jae     .max_ok
    mov     [rsp+120], rax
.max_ok:
    mov     r9d, 9
.ws_set:
    mov     rcx, [rsp+104]
    mov     rdx, [rsp+112]
    mov     r8, [rsp+120]
    call    SetProcessWorkingSetSizeEx
.throttle:
    mov     dword [rsp+112], 1
    xor     eax, eax
    cmp     dword [rsp+96], 0
    je      .mask
    mov     eax, 5
.mask:
    mov     [rsp+116], eax
    mov     dword [rsp+120], 0
    mov     rcx, [rsp+104]
    mov     edx, 4
    lea     r8, [rsp+112]
    mov     r9d, 12
    call    SetProcessInformation
    mov     dword [rsp+112], 5
    mov     rcx, [rsp+104]
    xor     edx, edx
    lea     r8, [rsp+112]
    mov     r9d, 4
    call    SetProcessInformation
    mov     rax, [procGpuPrio]
    test    rax, rax
    jz      .close
    mov     rcx, [rsp+104]
    mov     edx, 2
    cmp     dword [rsp+96], 0
    je      .gpu
    mov     edx, 4
.gpu:
    call    rax
.close:
    mov     rcx, [rsp+104]
    call    CloseHandle
.done:
    leave_frame

misc_timer:
    enter_frame
    mov     [rsp+104], ecx

    mov     dword [rsp+112], 1
    mov     dword [rsp+116], 5
    mov     eax, 5
    test    ecx, ecx
    jz      .state
    xor     eax, eax
.state:
    mov     [rsp+120], eax
    call    GetCurrentProcess
    mov     rcx, rax
    mov     edx, 4
    lea     r8, [rsp+112]
    mov     r9d, 12
    call    SetProcessInformation
    mov     rax, [procTimer]
    test    rax, rax
    jz      .done
    mov     edx, [rsp+104]
    mov     ecx, 5000
    lea     r8, [rsp+96]
    call    rax
.done:
    leave_frame

misc_purge:
    enter_frame
    mov     rax, [procSysInfo]
    test    rax, rax
    jz      .done
    mov     dword [rsp+96], 4
    mov     ecx, 0x50
    lea     rdx, [rsp+96]
    mov     r8d, 4
    call    rax
.done:
    leave_frame

misc_tick:
    enter4
    cmp     dword [activeTab], MISC_TAB
    jne     .session
    call    misc_ram
    call    misc_emu_text
    mov     rcx, [mainWindow]
    call    redraw
.session:
    bt      dword [specialBits], 0
    jnc     .off
    call    misc_scan_players
    cmp     dword [miscPidCount], 0
    je      .off
    cmp     dword [timerOn], 0
    jne     .boost
    call    session_start
.boost:
    xor     ebx, ebx
.pid:
    cmp     ebx, [miscPidCount]
    jae     .copy
    lea     rax, [miscPids]
    mov     edi, [rax+rbx*4]
    lea     rsi, [boostPids]
    xor     ecx, ecx
.find:
    cmp     ecx, [boostCount]
    jae     .new
    cmp     edi, [rsi+rcx*4]
    je      .known
    inc     ecx
    jmp     .find
.new:
    mov     ecx, edi
    mov     edx, 1
    call    misc_boost
.known:
    inc     ebx
    jmp     .pid
.copy:
    mov     ecx, [miscPidCount]
    mov     [boostCount], ecx
    lea     rsi, [miscPids]
    lea     rdi, [boostPids]
    xor     edx, edx
.cp:
    cmp     edx, ecx
    jae     .purge
    mov     eax, [rsi+rdx*4]
    mov     [rdi+rdx*4], eax
    inc     edx
    jmp     .cp
.purge:
    inc     dword [purgeTick]
    cmp     dword [purgeTick], 40
    jb      .done
    mov     dword [purgeTick], 0
    mov     dword [miscMemStatus], 64
    lea     rcx, [miscMemStatus]
    call    GlobalMemoryStatusEx
    cmp     dword [miscMemStatus+4], 75
    jb      .done
    call    misc_purge
    jmp     .done
.off:
    call    session_stop
    call    emu_pending_tick
.done:
    leave4

ui_read:
    enter_frame
    mov     ecx, 0x1042
    xor     edx, edx
    lea     r8, [uiBuf]
    xor     r9d, r9d
    call    SystemParametersInfoA
    mov     ecx, 0x16
    xor     edx, edx
    lea     r8, [uiBuf+4]
    xor     r9d, r9d
    call    SystemParametersInfoA
    mov     ecx, 0x0a
    xor     edx, edx
    lea     r8, [uiBuf+8]
    xor     r9d, r9d
    call    SystemParametersInfoA
    mov     dword [uiBuf+12], 8
    mov     ecx, 0x48
    mov     edx, 8
    lea     r8, [uiBuf+12]
    xor     r9d, r9d
    call    SystemParametersInfoA
    leave_frame

ui_write:
    enter_frame
    mov     ecx, 0x1043
    xor     edx, edx
    mov     r8d, [uiBuf]
    mov     r9d, 3
    call    SystemParametersInfoA
    mov     ecx, 0x17
    mov     edx, [uiBuf+4]
    xor     r8d, r8d
    mov     r9d, 3
    call    SystemParametersInfoA
    mov     ecx, 0x0b
    mov     edx, [uiBuf+8]
    xor     r8d, r8d
    mov     r9d, 3
    call    SystemParametersInfoA
    mov     dword [uiBuf+12], 8
    mov     ecx, 0x49
    mov     edx, 8
    lea     r8, [uiBuf+12]
    mov     r9d, 3
    call    SystemParametersInfoA
    leave_frame

ui_apply:
    enter_frame
    call    ui_read
    lea     rcx, [uiName]
    call    has_backup
    test    eax, eax
    jnz     .set
    lea     rcx, [uiName]
    lea     rdx, [uiBuf]
    mov     r8d, 20
    call    write_backup
.set:
    mov     dword [uiBuf], 0
    mov     dword [uiBuf+4], 0
    mov     dword [uiBuf+8], 31
    mov     dword [uiBuf+16], 0
    call    ui_write
    leave_frame

ui_revert:
    enter_frame
    lea     rcx, [uiName]
    lea     rdx, [uiBuf]
    mov     r8d, 24
    call    read_backup
    test    eax, eax
    jz      .done
    call    ui_write
    lea     rcx, [uiName]
    call    drop_backup
.done:
    leave_frame

display_max:
    enter4
    mov     word [devMode+36], 156
    xor     ecx, ecx
    mov     edx, -1
    lea     r8, [devMode]
    call    EnumDisplaySettingsA
    test    eax, eax
    jz      .fail
    mov     esi, [devMode+120]
    xor     ebx, ebx
.loop:
    mov     word [devMode2+36], 156
    xor     ecx, ecx
    mov     edx, ebx
    lea     r8, [devMode2]
    call    EnumDisplaySettingsA
    test    eax, eax
    jz      .done
    mov     eax, [devMode2+108]
    cmp     eax, [devMode+108]
    jne     .next
    mov     eax, [devMode2+112]
    cmp     eax, [devMode+112]
    jne     .next
    mov     eax, [devMode2+104]
    cmp     eax, [devMode+104]
    jne     .next
    mov     eax, [devMode2+120]
    cmp     eax, esi
    jbe     .next
    mov     esi, eax
.next:
    inc     ebx
    cmp     ebx, 4096
    jb      .loop
.done:
    mov     eax, esi
    leave4
.fail:
    xor     eax, eax
    leave4

set_hz:
    enter_frame
    mov     [devMode+120], ecx
    mov     dword [devMode+40], 0x400000
    lea     rcx, [devMode]
    mov     edx, 1
    call    ChangeDisplaySettingsA
    xor     ecx, ecx
    test    eax, eax
    sete    cl
    mov     eax, ecx
    leave_frame

hz_apply:
    enter_frame
    call    display_max
    mov     [rsp+96], eax
    test    eax, eax
    jz      .done
    cmp     eax, [devMode+120]
    je      .done
    lea     rcx, [hzName]
    call    has_backup
    test    eax, eax
    jnz     .set
    lea     rcx, [hzName]
    lea     rdx, [devMode+120]
    mov     r8d, 4
    call    write_backup
.set:
    mov     ecx, [rsp+96]
    call    set_hz
.done:
    mov     eax, [rsp+96]
    cmp     eax, 60
    jae     .ret
    mov     eax, 60
.ret:
    leave_frame

hz_revert:
    enter_frame
    lea     rcx, [hzName]
    lea     rdx, [rsp+96]
    mov     r8d, 4
    call    read_backup
    test    eax, eax
    jz      .done
    call    display_max
    mov     ecx, [rsp+96]
    call    set_hz
    lea     rcx, [hzName]
    call    drop_backup
.done:
    leave_frame

emu_conf_apply:
    enter_frame
    mov     [rsp+96], ecx
    call    conf_ready
    test    eax, eax
    jz      .done
    lea     rcx, [fpsText]
    lea     rdx, [fmtUnsigned]
    mov     r8d, [rsp+96]
    call    wsprintfA
    mov     eax, [miscSysInfo+32]
    shr     eax, 1
    cmp     eax, 2
    jae     .max
    mov     eax, 2
.max:
    cmp     eax, 8
    jbe     .cpus
    mov     eax, 8
.cpus:
    lea     rcx, [cpusText]
    lea     rdx, [fmtUnsigned]
    mov     r8d, eax
    call    wsprintfA
    lea     rcx, [sleConf]
    mov     edx, SLE_CONFS
    mov     r8d, 'E'
    call    conf_apply
.done:
    leave_frame

cpu_sets_init:
    enter4
    mov     dword [pCount], 0
    mov     dword [eCount], 0
    lea     rcx, [cpuSetBuf]
    mov     edx, 16384
    lea     r8, [miscSize]
    xor     r9d, r9d
    mov     qword [rsp+32], 0
    call    GetSystemCpuSetInformation
    test    eax, eax
    jz      .done
    mov     byte [rsp+96], 0xff
    mov     byte [rsp+97], 0
    lea     rsi, [cpuSetBuf]
    mov     edi, [miscSize]
    add     rdi, rsi
    mov     rbx, rsi
.range:
    cmp     rbx, rdi
    jae     .collect
    mov     al, [rbx+18]
    cmp     al, [rsp+96]
    jae     .hi
    mov     [rsp+96], al
.hi:
    cmp     al, [rsp+97]
    jbe     .step
    mov     [rsp+97], al
.step:
    mov     eax, [rbx]
    test    eax, eax
    jz      .done
    add     rbx, rax
    jmp     .range
.collect:
    mov     al, [rsp+96]
    cmp     al, [rsp+97]
    je      .done
    mov     rbx, rsi
.c:
    cmp     rbx, rdi
    jae     .done
    mov     al, [rbx+18]
    mov     edx, [rbx+8]
    cmp     al, [rsp+97]
    jne     .e
    mov     ecx, [pCount]
    cmp     ecx, 64
    jae     .s
    lea     r8, [pIds]
    mov     [r8+rcx*4], edx
    inc     dword [pCount]
    jmp     .s
.e:
    cmp     al, [rsp+96]
    jne     .s
    mov     ecx, [eCount]
    cmp     ecx, 64
    jae     .s
    lea     r8, [eIds]
    mov     [r8+rcx*4], edx
    inc     dword [eCount]
.s:
    mov     eax, [rbx]
    add     rbx, rax
    jmp     .c
.done:
    leave4

wifi_scan:

    enter4
    mov     [rsp+120], ecx
    xor     ecx, 1
    mov     [rsp+124], ecx
    mov     rax, [wlanHandle]
    mov     [rsp+104], rax
    test    rax, rax
    jnz     .open
    cmp     dword [rsp+120], 0
    jne     .done
    mov     ecx, 2
    xor     edx, edx
    lea     r8, [rsp+112]
    lea     r9, [rsp+104]
    call    WlanOpenHandle
    test    eax, eax
    jnz     .done
    mov     rax, [rsp+104]
    mov     [wlanHandle], rax
.open:
    mov     rcx, [rsp+104]
    xor     edx, edx
    lea     r8, [rsp+96]
    call    WlanEnumInterfaces
    test    eax, eax
    jnz     .close
    mov     rbx, [rsp+96]
    mov     esi, [rbx]
    lea     rdi, [rbx+8]
.loop:
    test    esi, esi
    jz      .free
    mov     rcx, [rsp+104]
    mov     rdx, rdi
    mov     r8d, 2
    mov     r9d, 4
    lea     rax, [rsp+120]
    mov     [rsp+32], rax
    mov     qword [rsp+40], 0
    call    WlanSetInterface
    mov     rcx, [rsp+104]
    mov     rdx, rdi
    mov     r8d, 3
    mov     r9d, 4
    lea     rax, [rsp+124]
    mov     [rsp+32], rax
    mov     qword [rsp+40], 0
    call    WlanSetInterface
    add     rdi, 532
    dec     esi
    jmp     .loop
.free:
    mov     rcx, rbx
    call    WlanFreeMemory
.close:
    cmp     dword [rsp+120], 0
    je      .done
    mov     rcx, [rsp+104]
    xor     edx, edx
    call    WlanCloseHandle
    mov     qword [wlanHandle], 0
.done:
    leave4

emu_pending_tick:
    enter_frame
    mov     eax, [emuPending]
    test    eax, eax
    jz      .done
    call    conf_ready
    test    eax, eax
    jz      .done
    cmp     dword [emuPending], 2
    je      .revert
    mov     ecx, [emuHz]
    call    emu_conf_apply
    test    eax, eax
    jz      .done
    mov     dword [emuPending], 0
    leave_frame
.revert:
    lea     rcx, [sleConf]
    mov     edx, SLE_CONFS
    mov     r8d, 'E'
    call    conf_revert
    test    eax, eax
    jz      .done
    mov     dword [emuPending], 0
.done:
    leave_frame

stop_updates:
    enter4
    xor     ecx, ecx
    xor     edx, edx
    mov     r8d, 1
    call    OpenSCManagerA
    test    rax, rax
    jz      .done
    mov     rbx, rax
    lea     rsi, [updateServices]
.loop:
    mov     rdx, [rsi]
    test    rdx, rdx
    jz      .close
    mov     rcx, rbx
    mov     r8d, 0x20
    call    OpenServiceA
    test    rax, rax
    jz      .next
    mov     rdi, rax
    mov     rcx, rax
    mov     edx, 1
    lea     r8, [miscBuf]
    call    ControlService
    mov     rcx, rdi
    call    CloseServiceHandle
.next:
    add     rsi, 8
    jmp     .loop
.close:
    mov     rcx, rbx
    call    CloseServiceHandle
.done:
    leave4

name_in:
    enter4
    mov     rbx, rcx
    mov     rsi, rdx
.loop:
    mov     rdx, [rsi]
    test    rdx, rdx
    jz      .no
    mov     rcx, rbx
    call    lstrcmpiA
    test    eax, eax
    jz      .yes
    add     rsi, 8
    jmp     .loop
.yes:
    mov     eax, 1
    leave4
.no:
    xor     eax, eax
    leave4

background_session:
    enter4
    mov     [rsp+120], ecx
    test    ecx, ecx
    jnz     .start
    xor     ebx, ebx
.resume:
    cmp     ebx, [frozenCount]
    jae     .unset
    lea     rax, [frozenPids]
    mov     r8d, [rax+rbx*4]
    mov     ecx, 0x800
    xor     edx, edx
    call    OpenProcess
    test    rax, rax
    jz      .rn
    mov     rsi, rax
    mov     rcx, rax
    call    [procResume]
    mov     rcx, rsi
    call    CloseHandle
.rn:
    inc     ebx
    jmp     .resume
.unset:
    mov     dword [frozenCount], 0
    xor     ebx, ebx
.u:
    cmp     ebx, [ecoreCount]
    jae     .clear
    lea     rax, [ecorePids]
    mov     r8d, [rax+rbx*4]
    mov     ecx, 0x2000
    xor     edx, edx
    call    OpenProcess
    test    rax, rax
    jz      .un
    mov     rsi, rax
    mov     rcx, rax
    xor     edx, edx
    xor     r8d, r8d
    call    SetProcessDefaultCpuSets
    mov     rcx, rsi
    call    CloseHandle
.un:
    inc     ebx
    jmp     .u
.clear:
    mov     dword [ecoreCount], 0
    leave4
.start:
    cmp     qword [procSuspend], 0
    je      .done
    mov     ecx, 2
    xor     edx, edx
    call    CreateToolhelp32Snapshot
    cmp     rax, -1
    je      .done
    mov     rdi, rax
    mov     dword [miscEntry], 304
    mov     rcx, rdi
    lea     rdx, [miscEntry]
    call    Process32First
.p:
    test    eax, eax
    jz      .snap_close
    lea     rcx, [miscEntry+44]
    lea     rdx, [freezeList]
    call    name_in
    test    eax, eax
    jz      .ecore
    mov     eax, [frozenCount]
    cmp     eax, 32
    jae     .pn
    mov     r8d, [miscEntry+8]
    mov     ecx, 0x800
    xor     edx, edx
    call    OpenProcess
    test    rax, rax
    jz      .pn
    mov     rsi, rax
    mov     rcx, rax
    call    [procSuspend]
    mov     ebx, eax
    mov     rcx, rsi
    call    CloseHandle
    test    ebx, ebx
    js      .pn
    mov     eax, [frozenCount]
    mov     edx, [miscEntry+8]
    lea     rcx, [frozenPids]
    mov     [rcx+rax*4], edx
    inc     dword [frozenCount]
    jmp     .pn
.ecore:
    cmp     dword [eCount], 0
    je      .pn
    lea     rcx, [miscEntry+44]
    lea     rdx, [ecoreList]
    call    name_in
    test    eax, eax
    jz      .pn
    mov     eax, [ecoreCount]
    cmp     eax, 64
    jae     .pn
    mov     r8d, [miscEntry+8]
    mov     ecx, 0x2000
    xor     edx, edx
    call    OpenProcess
    test    rax, rax
    jz      .pn
    mov     rsi, rax
    mov     rcx, rax
    lea     rdx, [eIds]
    mov     r8d, [eCount]
    call    SetProcessDefaultCpuSets
    mov     ebx, eax
    mov     rcx, rsi
    call    CloseHandle
    test    ebx, ebx
    jz      .pn
    mov     eax, [ecoreCount]
    mov     edx, [miscEntry+8]
    lea     rcx, [ecorePids]
    mov     [rcx+rax*4], edx
    inc     dword [ecoreCount]
.pn:
    mov     rcx, rdi
    lea     rdx, [miscEntry]
    call    Process32Next
    jmp     .p
.snap_close:
    mov     rcx, rdi
    call    CloseHandle
.done:
    leave4

session_start:
    enter_frame
    mov     ecx, 1
    call    misc_timer
    mov     dword [timerOn], 1
    mov     dword [purgeTick], 0
    call    misc_purge
    lea     rcx, [sessionRegs]
    mov     edx, SESSION_REGS
    mov     r8d, 'N'
    call    reg_apply
    xor     ecx, ecx
    call    wifi_scan
    call    stop_updates
    mov     ecx, 1
    call    background_session
    leave_frame

session_stop:
    enter4
    cmp     dword [timerOn], 0
    je      .done
    xor     ecx, ecx
    call    misc_timer
    mov     dword [timerOn], 0
    lea     rcx, [sessionRegs]
    mov     edx, SESSION_REGS
    mov     r8d, 'N'
    call    reg_revert
    mov     ecx, 1
    call    wifi_scan
    xor     ecx, ecx
    call    background_session
    call    misc_scan_players
    xor     ebx, ebx
.loop:
    cmp     ebx, [miscPidCount]
    jae     .clear
    lea     rax, [miscPids]
    mov     ecx, [rax+rbx*4]
    xor     edx, edx
    call    misc_boost
    inc     ebx
    jmp     .loop
.clear:
    mov     dword [boostCount], 0
.done:
    leave4

misc_shutdown:
    enter_frame
    call    session_stop
    leave_frame

misc_privilege:
    enter_frame
    call    GetCurrentProcess
    mov     rcx, rax
    mov     edx, 0x20
    lea     r8, [rsp+96]
    call    OpenProcessToken
    test    eax, eax
    jz      .done
    xor     ecx, ecx
    lea     rdx, [profilePriv]
    lea     r8, [rsp+108]
    call    LookupPrivilegeValueA
    mov     dword [rsp+104], 1
    mov     dword [rsp+116], 2
    mov     rcx, [rsp+96]
    xor     edx, edx
    lea     r8, [rsp+104]
    xor     r9d, r9d
    mov     qword [rsp+32], 0
    mov     qword [rsp+40], 0
    call    AdjustTokenPrivileges
    mov     rcx, [rsp+96]
    call    CloseHandle
.done:
    leave_frame

misc_cpu:
    enter4
    lea     rsi, [hwCpuRaw]
    mov     edi, 0x80000002
.leaf:
    mov     eax, edi
    xor     ecx, ecx
    cpuid
    mov     [rsi], eax
    mov     [rsi+4], ebx
    mov     [rsi+8], ecx
    mov     [rsi+12], edx
    add     rsi, 16
    inc     edi
    cmp     edi, 0x80000005
    jb      .leaf
    mov     byte [rsi], 0
    lea     rsi, [hwCpuRaw]
.trim:
    cmp     byte [rsi], ' '
    jne     .info
    inc     rsi
    jmp     .trim
.info:
    lea     rcx, [miscSysInfo]
    call    GetSystemInfo
    lea     rcx, [hwCpu]
    lea     rdx, [fmtCpu]
    mov     r8, rsi
    mov     r9d, [miscSysInfo+32]
    call    wsprintfA
    leave4

misc_gpu:
    enter4
    lea     rcx, [hwGpu0]
    lea     rdx, [notFound]
    call    lstrcpyA
    lea     rcx, [hwGpu1]
    lea     rdx, [notFound]
    call    lstrcpyA
    xor     ebx, ebx
    xor     esi, esi
.loop:
    cmp     ebx, 16
    jae     .done
    cmp     esi, 2
    jae     .done
    lea     rcx, [miscKeyBuf]
    lea     rdx, [gpuKeyFmt]
    mov     r8d, ebx
    call    wsprintfA
    mov     dword [miscSize], 128
    lea     rax, [miscSize]
    mov     [rsp+48], rax
    lea     rax, [miscTemp]
    mov     [rsp+40], rax
    mov     qword [rsp+32], 0
    mov     rcx, HKEY_LM
    lea     rdx, [miscKeyBuf]
    lea     r8, [driverDesc]
    mov     r9d, 2
    call    RegGetValueA
    test    eax, eax
    jnz     .next
    lea     rcx, [miscTemp]
    lea     rdx, [microsoftText]
    call    StrStrIA
    test    rax, rax
    jnz     .next
    mov     qword [miscQword], 0
    mov     dword [miscSize], 8
    lea     rax, [miscSize]
    mov     [rsp+48], rax
    lea     rax, [miscQword]
    mov     [rsp+40], rax
    mov     qword [rsp+32], 0
    mov     rcx, HKEY_LM
    lea     rdx, [miscKeyBuf]
    lea     r8, [qwMemValue]
    mov     r9d, 0xffff
    call    RegGetValueA
    test    eax, eax
    jz      .mem
    mov     qword [miscQword], 0
    mov     dword [miscSize], 8
    lea     rax, [miscSize]
    mov     [rsp+48], rax
    lea     rax, [miscQword]
    mov     [rsp+40], rax
    mov     qword [rsp+32], 0
    mov     rcx, HKEY_LM
    lea     rdx, [miscKeyBuf]
    lea     r8, [memValue]
    mov     r9d, 0xffff
    call    RegGetValueA
.mem:
    mov     r9, [miscQword]
    shr     r9, 20
    lea     rcx, [hwGpu0]
    test    esi, esi
    jz      .slot
    lea     rcx, [hwGpu1]
.slot:
    lea     rdx, [fmtGpu]
    lea     r8, [miscTemp]
    test    r9d, r9d
    jnz     .fmt
    lea     rdx, [fmtPlain]
.fmt:
    call    wsprintfA
    inc     esi
.next:
    inc     ebx
    jmp     .loop
.done:
    leave4

read_bios:
    enter_frame
    mov     byte [r8], 0
    mov     [rsp+40], r8
    mov     dword [miscSize], 160
    lea     rax, [miscSize]
    mov     [rsp+48], rax
    mov     qword [rsp+32], 0
    mov     r8, rdx
    mov     rcx, HKEY_LM
    lea     rdx, [biosKey]
    mov     r9d, 2
    call    RegGetValueA
    leave_frame

misc_board:
    enter_frame
    lea     rdx, [boardMaker]
    lea     r8, [miscTemp]
    call    read_bios
    lea     rdx, [boardProduct]
    lea     r8, [miscTemp2]
    call    read_bios
    lea     rdx, [biosVersionName]
    lea     r8, [miscTemp3]
    call    read_bios
    lea     rax, [miscTemp3]
    mov     [rsp+32], rax
    lea     rcx, [hwBoard]
    lea     rdx, [fmtBoard]
    lea     r8, [miscTemp]
    lea     r9, [miscTemp2]
    call    wsprintfA
    leave_frame

misc_ram:
    enter_frame
    mov     qword [miscQword], 0
    lea     rcx, [miscQword]
    call    GetPhysicallyInstalledSystemMemory
    mov     rax, [miscQword]
    add     rax, 0x80000
    shr     rax, 20
    mov     [rsp+96], eax
    mov     dword [miscMemStatus], 64
    lea     rcx, [miscMemStatus]
    call    GlobalMemoryStatusEx
    mov     rax, [miscMemStatus+8]
    mov     rcx, [miscMemStatus+16]
    mov     rdx, rax
    sub     rdx, rcx
    imul    rdx, rdx, 10
    shr     rdx, 30
    imul    rax, rax, 10
    shr     rax, 30
    mov     [rsp+104], eax
    mov     eax, edx
    xor     edx, edx
    mov     ecx, 10
    div     ecx
    mov     [rsp+108], eax
    mov     [rsp+112], edx
    mov     eax, [rsp+104]
    xor     edx, edx
    mov     ecx, 10
    div     ecx
    mov     [rsp+40], rax
    mov     [rsp+48], rdx
    mov     eax, [rsp+112]
    mov     [rsp+32], rax
    lea     rcx, [hwRam]
    lea     rdx, [fmtRam]
    mov     r8d, [rsp+96]
    mov     r9d, [rsp+108]
    call    wsprintfA
    leave_frame

misc_disk:
    enter4
    mov     byte [hwDiskList], 0
    call    GetLogicalDrives
    mov     esi, eax
    mov     ebx, 2
    xor     edi, edi
.loop:
    cmp     ebx, 26
    jae     .done
    bt      esi, ebx
    jnc     .next
    lea     eax, [ebx+'A']
    mov     [miscDriveRoot], al
    mov     byte [miscDriveRoot+1], ':'
    mov     byte [miscDriveRoot+2], '\'
    mov     byte [miscDriveRoot+3], 0
    lea     rcx, [miscDriveRoot]
    call    GetDriveTypeA
    cmp     eax, 3
    jne     .next
    lea     rcx, [miscDriveRoot]
    lea     rdx, [miscQword]
    lea     r8, [miscQword+8]
    lea     r9, [miscQword+16]
    call    GetDiskFreeSpaceExA
    test    eax, eax
    jz      .next
    mov     rax, [miscQword+8]
    shr     rax, 30
    add     edi, eax
    mov     [rsp+32], rax
    mov     r9, [miscQword]
    shr     r9, 30
    movzx   r8d, byte [miscDriveRoot]
    lea     rcx, [miscTemp]
    lea     rdx, [fmtDisk]
    call    wsprintfA
    lea     rcx, [hwDiskList]
    call    lstrlenA
    cmp     eax, 200
    jae     .next
    lea     rcx, [hwDiskList]
    lea     rdx, [miscTemp]
    call    lstrcatA
.next:
    inc     ebx
    jmp     .loop
.done:
    lea     rcx, [hwDisk]
    lea     rdx, [fmtDiskTotal]
    mov     r8d, edi
    call    wsprintfA
    lea     rcx, [hwDisk]
    lea     rdx, [hwDiskList]
    call    lstrcatA
    leave4

misc_init:
    enter_frame
    call    load_special
    lea     rcx, [ntdllName]
    call    GetModuleHandleA
    mov     [rsp+96], rax
    mov     rcx, rax
    lea     rdx, [timerProcName]
    call    GetProcAddress
    mov     [procTimer], rax
    mov     rcx, [rsp+96]
    lea     rdx, [sysInfoProcName]
    call    GetProcAddress
    mov     [procSysInfo], rax
    mov     rcx, [rsp+96]
    lea     rdx, [suspendProcName]
    call    GetProcAddress
    mov     [procSuspend], rax
    mov     rcx, [rsp+96]
    lea     rdx, [resumeProcName]
    call    GetProcAddress
    mov     [procResume], rax
    lea     rcx, [gdiName]
    call    GetModuleHandleA
    test    rax, rax
    jz      .info
    mov     rcx, rax
    lea     rdx, [gpuPrioProcName]
    call    GetProcAddress
    mov     [procGpuPrio], rax
.info:
    call    misc_privilege
    call    misc_cpu
    call    cpu_sets_init
    call    misc_gpu
    call    misc_board
    call    misc_ram
    call    misc_disk
    call    misc_emulator
    call    misc_emu_text
    leave_frame

misc_show:
    enter4
    xor     esi, esi
    cmp     dword [activeTab], MISC_TAB
    jne     .start
    mov     esi, 5
.start:
    xor     ebx, ebx
.loop:
    cmp     ebx, FEATURES+1
    jae     .done
    lea     rax, [specialButtons]
    mov     rcx, [rax+rbx*8]
    mov     edx, esi
    call    ShowWindow
    lea     rax, [specialButtons]
    mov     rcx, [rax+rbx*8]
    call    redraw
    inc     ebx
    jmp     .loop
.done:
    leave4

%macro hw_row 1
    box     ROW, 41, 172+%1*44, 489, 212+%1*44
    box     ACCENT, 41, 172+%1*44, 43, 212+%1*44
    text_pointer 51, 174+%1*44, 479, 190+%1*44, [hwLabels+%1*8], SOFT, fontSmall, LABEL_MIDDLE
    text_pointer 51, 190+%1*44, 479, 210+%1*44, [hwValues+%1*8], INK, fontRegular, LABEL_MIDDLE | LABEL_ELLIPSIS
%endmacro

%macro sp_row 1
    mov     r8d, ROW
    cmp     dword [miscFocus], %1
    jne     %%color
    mov     r8d, ROW_SELECTED
%%color:
    rect    509, 212+%1*58, 959, 264+%1*58
    call    fill_box
    bt      dword [specialBits], %1
    jnc     %%off
    box     ACCENT, 509, 212+%1*58, 512, 264+%1*58
%%off:
    text_pointer 521, 214+%1*58, 862, 232+%1*58, [spTitles+%1*8], INK, fontStrong, LABEL_MIDDLE | LABEL_ELLIPSIS
    text_pointer 521, 232+%1*58, 862, 262+%1*58, [spDetails+%1*8], SOFT, fontSmall, LABEL_WRAP
%endmacro

paint_misc:
    enter_frame
    frame_panel 23, 142, 977, 501
    frame_mark 236, 246
    frame_mark 249, 259
    text_label 35, 144, 966, 161, miscHeading, SOFT, fontSmall, LABEL_MIDDLE
    hw_row  0
    hw_row  1
    hw_row  2
    hw_row  3
    hw_row  4
    hw_row  5
    text_label 509, 166, 959, 186, specialHeading, INK, fontStrong, LABEL_MIDDLE
    text_label 509, 188, 959, 208, miscEmuText, SOFT, fontSmall, LABEL_MIDDLE | LABEL_ELLIPSIS
    sp_row  0
    sp_row  1
    sp_row  2
    box     LOG_BG, 190, 446, 959, 494
    text_label 200, 448, 815, 468, creditLine, INK, fontStrong, LABEL_MIDDLE | LABEL_ELLIPSIS
    text_label 200, 468, 815, 492, licenseLine, SOFT, fontSmall, LABEL_MIDDLE | LABEL_ELLIPSIS
    leave_frame

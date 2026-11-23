default rel
global entry
global catalogue, catalogue_starts, catalogue_counts

extern GetModuleHandleA, ExitProcess, LocalFree
extern CreateThread, PostMessageA
extern CreateFileA, ReadFile, GetFileSizeEx, CloseHandle, GetEnvironmentVariableA
extern CreateToolhelp32Snapshot, Process32First, Process32Next
extern OpenProcess, GetPriorityClass, QueryFullProcessImageNameA, GlobalMemoryStatusEx
extern lstrlenA, lstrcpyA, lstrcatA, lstrcmpiA, WideCharToMultiByte, StrStrIA
extern RegGetValueA, PowerGetActiveScheme, PowerReadFriendlyName
extern RegisterClassExA, CreateWindowExA, AdjustWindowRectEx, ShowWindow, UpdateWindow
extern GetMessageA, IsDialogMessageA, TranslateMessage, DispatchMessageA, DefWindowProcA
extern LoadCursorA, PostQuitMessage, SetWindowTextA, InvalidateRect, ScreenToClient
extern SystemParametersInfoA, SetProcessDPIAware
extern GetWindowTextA, SendMessageW, EnableWindow, SetBkColor, MoveWindow
extern BeginPaint, EndPaint, FillRect, DrawTextA, DrawFocusRect
extern wsprintfA
extern CreateFontA, CreateSolidBrush, SelectObject, SetTextColor, SetBkMode, DeleteObject
extern GetStockObject, RoundRect, Ellipse, SetViewportOrgEx
extern o3_first_instance
extern o3_choose_target, o3_target_label
extern SetTimer, KillTimer, o3_refresh_switches
extern o3_record_launch, o3_backup_first, o3_collect, o3_options, o3_apply, o3_restore
extern RegSetKeyValueA, MessageBoxA, DestroyWindow
extern o3_defaults, o3_item_total, o3_release_efficiency

%define RGB(r,g,b) ((r) | ((g) << 8) | ((b) << 16))

WINDOW_STYLE    equ 0x820A0000
WINDOW_EX_STYLE equ 0x00040000
BUTTON_STYLE    equ 0x5001000B

WM_CREATE       equ 0x0001
WM_DESTROY      equ 0x0002
WM_PAINT        equ 0x000F
WM_CLOSE        equ 0x0010
WM_ERASEBKGND   equ 0x0014
WM_DRAWITEM     equ 0x002B
WM_NCHITTEST    equ 0x0084
WM_COMMAND      equ 0x0111
WM_CTLCOLOREDIT equ 0x0133
WM_SCAN_READY   equ 0x8001
WM_SCAN_ITEM    equ 0x8002

LABEL_SINGLE   equ 0x0820
LABEL_MIDDLE   equ 0x0824
LABEL_CENTER   equ 0x0825
LABEL_WRAP     equ 0x0810
LABEL_RIGHT    equ 0x0826
LABEL_ELLIPSIS equ 0x8000

ID_REFRESH     equ 100
ID_EMULATOR    equ 101
ID_WINDOWS     equ 102
ID_BACKGROUND  equ 103
ID_EXPLORER    equ 104
ID_CPU         equ 105
ID_RAM         equ 106
ID_ENERGY      equ 107
ID_UNAVAILABLE equ 108
ID_MISC        equ 109
ID_SPECIAL     equ 180
ID_LICENSE     equ 184
ID_PREV        equ 113
ID_NEXT        equ 114
ID_SEARCH      equ 115
ID_RESTORE     equ 116
ID_MINIMIZE    equ 111
ID_CLOSE       equ 112

EN_CHANGE      equ 0x0300
PAGE_ROWS      equ 8
PAGE_SIZE      equ 2*PAGE_ROWS
ID_FIRST_ROW   equ 120
ID_FIRST_ON    equ ID_FIRST_ROW+PAGE_SIZE
ID_FIRST_OFF   equ ID_FIRST_ON+PAGE_SIZE

ROW_LEFT       equ 41
ROW_TOP        equ 163
ROW_WIDTH      equ 449
ROW_HEIGHT     equ 34
ROW_STRIDE     equ 39
COLUMN_STRIDE  equ ROW_WIDTH+20
ROW_BODY_WIDTH equ ROW_WIDTH-104
SWITCH_X       equ ROW_WIDTH-89
SWITCH_WIDTH   equ 80
SWITCH_HEIGHT  equ 30

SWITCH_Y       equ 2
TAB_LEFT       equ 23
TAB_RIGHT      equ 977
TAB_TOP        equ 119
TAB_GAP        equ 3
TAB_HEIGHT     equ 23
CAPTION_BUTTON_X equ 928
CAPTION_HEIGHT equ 36
PAGE_BUTTON_Y  equ 476
PAGE_BUTTON_H  equ 20

ITEM_COUNT     equ 112
DYNAMIC_LIMIT  equ 1200
MAX_ITEMS      equ ITEM_COUNT+DYNAMIC_LIMIT
CATEGORY_COUNT equ 7
ENERGY_TAB     equ 6
UNAVAILABLE_TAB equ CATEGORY_COUNT
MISC_TAB       equ CATEGORY_COUNT+1
TAB_COUNT      equ CATEGORY_COUNT+2

BG             equ 0
PANEL          equ 1
ROW            equ 2
ROW_SELECTED   equ 3
BORDER         equ 4
ACCENT         equ 5
LOG_BG         equ 6
BUTTON         equ 7
BUTTON_LIGHT   equ 8
STRIP          equ 9
SHADOW         equ 10
FRAME          equ 11
CELL_GLINT     equ 12
AMBER_SHADE    equ 13
AMBER_GLOW     equ 14
PANEL_GLOSS    equ 15
TRACK_OFF      equ 16
TRACK_LIGHT    equ 17
KNOB_OUTLINE   equ 18
KNOB_FACE      equ 19
KNOB_LIGHT     equ 20
KNOB_HUB       equ 21
KNOB_RING      equ 22
BRUSH_COUNT    equ 23

INK            equ RGB(240,248,255)
SOFT           equ RGB(140,210,255)
DIM            equ RGB(90,130,170)
LOG_TEXT       equ RGB(110,230,255)
OFF_TEXT       equ RGB(190,205,220)
DARK           equ RGB(40,28,0)
EDIT_BG        equ RGB(18,66,110)

%macro enter_frame 0
    sub     rsp, 0x88
%endmacro

%macro leave_frame 0
    add     rsp, 0x88
    ret
%endmacro

%macro rect 4
    mov     dword [drawingRect], %1
    mov     dword [drawingRect+4], %2
    mov     dword [drawingRect+8], %3
    mov     dword [drawingRect+12], %4
%endmacro

%macro box 5
    rect    %2, %3, %4, %5
    mov     r8d, %1
    call    fill_box
%endmacro

%macro round_shape 6
    rect    %2, %3, %4, %5
    mov     r8d, %1
    mov     r9d, %6
    call    fill_round
%endmacro

%macro text_pointer 8
    rect    %1, %2, %3, %4
    mov     rdx, %5
    mov     r8d, %6
    mov     r9, [%7]
    mov     dword [textFlags], %8
    call    draw_text
%endmacro

%macro text_label 8
    lea     rdx, [%5]
    text_pointer %1, %2, %3, %4, rdx, %6, %7, %8
%endmacro

struc Feature
    .title:     resq 1
    .detail:    resq 1
endstruc

section .rdata
className       db "OffOrOnWindow",0
windowTitle     db "OffOrOn (O3) v1.02",0
buttonClass     db "BUTTON",0
editClass       db "EDIT",0
searchPrompt    dw __utf16__('Cari fitur...'),0
searchLabel     db "CARI FITUR",0
fontName        db "Segoe UI",0
displayFont     db "Bahnschrift",0
logFont         db "Consolas",0
emptyText       db 0
brandName       db "OFFORON",0
subtitle        db "PENGATURAN FREE FIRE",0
minimizeLabel   db "Minimalkan",0
closeLabel      db "Tutup",0
closeGlyph      db "X",0
defaultLabel    db "DEFAULT",0
refreshTitle    db "Pindai ulang",0
emulatorTab     db "Emulator",0
windowsTab      db "Windows",0
backgroundTab   db "Latar",0
explorerTab     db "Explorer",0
cpuTab          db "CPU",0
ramTab          db "RAM",0
energyTab       db "Energy Saver",0
ecoLabel        db "ECO",0
maxLabel        db "MAX",0
unavailableTab  db "Unavailable",0
prevTitle       db "Sebelumnya",0
nextTitle       db "Berikutnya",0
fmtPage         db "%u-%u / %u",0
noResults       db "Tidak ada hasil",0
scanHeading     db "Memeriksa ketersediaan",0
scanDetail      db "Memeriksa apakah setiap fitur bisa diubah di perangkat ini.",0
searchHeading   db "Hasil pencarian",0
activityHeading db "AKTIVITAS",0
fmtToggleName   db "%s: %s",0
fmtScanProgress db "Memeriksa %u dari %u fitur",0
fmtLaunch       db "OffOrOn (O3) v1.02 | Dibuka %u kali",0
recreatedBackup db "Backup awal hilang, dibuat ulang dari setelan saat ini.",0
backupError     db "Backup gagal disimpan. ON/OFF dikunci sampai Pindai ulang berhasil.",0
closeTitle      db "Tutup O3",0
closeConfirm    db "All changes will be saved.",13,10,13,10,"Semua perubahan tetap tersimpan setelah O3 ditutup: service, scheduled task, startup, aplikasi latar, dan prioritas ECO/MAX tetap berlaku sampai kamu ubah sendiri atau tekan Pulihkan backup.",13,10,13,10,"Pelepasan RAM berkala untuk program ECO berhenti selama O3 tertutup.",13,10,13,10,"Tutup O3 sekarang?",0
restoreTitle    db "Pulihkan backup",0
restoreRunning  db "Memulihkan setelan dari backup...",0
restoreConfirm  db "Kembalikan semua setelan yang tercatat di backup ke nilai awalnya?",13,10,13,10
                db "Setelan per aplikasi tidak ikut. Tutup browser dan emulator dulu agar setelannya ikut dipulihkan.",0
footerRecordFail db "55012 - Backup item gagal disimpan. Setelan tidak diubah.",0
footerCancelled db "Dibatalkan. Tidak ada yang diubah.",0
footerRespawned db "55012 - Dijalankan ulang otomatis oleh Windows/aplikasi lain. Dipindah ke Unavailable.",0
footerApplying  db "Menerapkan setelan...",0
footerApplyFail db "55012 - Perubahan gagal diverifikasi",0
footerApplyLive db "200 - Berhasil, aktif sekarang",0
footerApplySave db "200 - Berhasil disimpan. Berlaku setelah aplikasi/Windows dibuka ulang.",0
footerSame      db "200 - Setelan sudah dalam keadaan tersebut",0
footerCannot    db "55012 - Setelan tidak dapat diubah",0
emulatorLabel   db "PROFIL EMULATOR",0
processLabel    db "PROSES EMULATOR",0
memoryLabel     db "RAM SISTEM",0
resolutionLabel db "RESOLUSI PEMAIN",0
emHeading       db "Emulator dan Free Fire",0
winHeading      db "Setelan Windows",0
bgHeading       db "Aktivitas latar",0
explorerHeading db "File Explorer",0
cpuHeading      db "CPU: yang sedang memakai CPU, service, scheduled task, dan aplikasi latar",0
ramHeading      db "RAM: service, program startup, dan yang bisa dihentikan sementara",0
energyHeading   db "Energy Saver: ECO hemat daya, MAX performa penuh, DEFAULT diatur Windows",0
manualStatus    db "Belum terbaca",0
inheritedStatus db "Belum diatur",0
unavailableStatus db "Tidak tersedia",0
failedStatus    db "55012 - Status gagal dibaca",0
failedBadge     db "Error 55012",0
pendingStatus   db "Memindai",0
unknownApplyStatus db "Status belum terverifikasi",0
applyingStatus   db "Menerapkan",0
unknownApplyBadge db "Belum pasti",0
psSource        db "PowerShell",0
cmdSource       db "CMD",0
winSource       db "Win32",0
attemptSources  db "3 pembacaan gagal",0
applyFailedSource db "Perubahan belum terverifikasi",0
notInO3Text     db "Belum didukung O3",0
unsupportedBadge db "Belum didukung",0
liveToggleText  db "Tersedia. Berlaku langsung di Windows.",0
savedToggleText db "Tersedia. Tersimpan, mungkin perlu buka ulang.",0
unsupportedText db "Tidak tersedia pada perangkat ini",0
unreadableText  db "Status belum bisa diverifikasi",0
readOnlyText    db "Status saja",0
pickApp db "Pilih aplikasi",0
availableText   db "Tersedia",0
fmtScanResult   db "%u tersedia  /  %u unavailable  /  %u error",0

tabTitles:      dq emulatorTab, windowsTab, backgroundTab, explorerTab, cpuTab, ramTab, energyTab, unavailableTab, miscTab
tabHeadings:    dq emHeading, winHeading, bgHeading, explorerHeading, cpuHeading, ramHeading, energyHeading, unavailableTab, miscHeading
statusLabels:   dq manualStatus, toggleOn, toggleOff, inheritedStatus
                dq unavailableStatus, failedStatus, pendingStatus, unknownApplyStatus, applyingStatus
defaultSource   db "Default aplikasi atau Windows",0
browserSource db "Preferensi browser",0
browserBusy db "55012 - Tutup browser termasuk ikon tray/Startup Boost, lalu ulangi.",0
emuBusy db "55012 - Tutup emulator agar konfigurasi bisa disimpan.",0
emuSource db "Konfigurasi emulator",0
androidSource db "Android",0
nativeSource db "Windows Settings",0
serviceSource db "Service Windows",0
taskSource db "Task Scheduler",0
startupSource db "Startup",0
backgroundAppSource db "Aplikasi latar",0
energySource db "Program berjalan",0
stopSource db "Hentikan sementara",0
sourceLabels:   dq emptyText, psSource, cmdSource, winSource, defaultSource, emuSource, browserSource, nativeSource, androidSource
                dq serviceSource, taskSource, startupSource, backgroundAppSource, energySource, stopSource
availabilityLabels: dq notInO3Text, liveToggleText, savedToggleText
                    dq unsupportedText, unreadableText, readOnlyText, pickApp
shortAvailability:  dq unsupportedBadge, availableText, availableText
                    dq unavailableStatus, unreadableText, readOnlyText, pickApp
applyLabels:    dq footerApplyFail, footerApplyLive, footerApplySave, footerSame, footerCannot, browserBusy, emuBusy
                dq footerRecordFail, footerCancelled, footerRespawned
probeManifest:  incbin "scan/probes.txt"
                db 0

%include "catalogue.inc"
section .rdata

brushColors:
    dd RGB(12,30,52), RGB(24,84,136), RGB(18,66,110), RGB(24,81,128), RGB(10,42,74)
    dd RGB(255,184,0), RGB(8,26,46), RGB(30,116,178), RGB(52,146,210), EDIT_BG
    dd RGB(18,20,26), RGB(56,60,68), RGB(80,180,235), RGB(196,120,0), RGB(255,212,80)
    dd RGB(32,102,160), RGB(64,84,104), RGB(84,106,128), RGB(110,114,122)
    dd RGB(176,182,190), RGB(234,238,244), RGB(204,209,216), RGB(150,156,166)

section .rdata
viewKey         db "Software\OffOrOn",0
viewValue       db "LastView",0
effectHeading   db "EFEK",0
effectHint      db "Pilih sebuah item untuk melihat apa yang terjadi jika dinonaktifkan.",0

section .data
windowRect      dd 0, 0, 1000, 690
scanMessage     dq scanHeading

section .bss
instanceHandle  resq 1
mainWindow      resq 1
refreshButton   resq 1
restoreButton   resq 1
restoreBusy     resd 1
restoreResult   resq 1
switchEnergy    resd 1
tabButtons      resq TAB_COUNT
prevButton      resq 1
nextButton      resq 1
searchBox       resq 1
scanThread      resq 1
rowButtons      resq PAGE_SIZE
rowOnButtons    resq PAGE_SIZE
rowOffButtons   resq PAGE_SIZE
windowClass     resb 80
workArea        resd 4
messageBuffer   resb 48
paintBuffer     resb 72
brushes         resq BRUSH_COUNT
fontHeading     resq 1
fontRegular     resq 1
fontSmall       resq 1
fontStrong      resq 1
fontLog         resq 1
drawingDc       resq 1
drawingRect     resd 4
nullPen         resq 1
textFlags       resd 1
viewState       resd 3
viewRestored    resd 1
activeTab       resd 1
activeRow       resd 1
pageOffset      resd 1
viewCount       resd 1
scanBusy        resd 1
catalogueReady  resd 1
launchCount     resd 1
backupReady     resd 1
scanProgress    resd 1
categoryTotals  resd TAB_COUNT
applyItem       resd 1
applyValue      resd 1
applyResult     resd 1
viewIndices     resd MAX_ITEMS
itemStates      resb MAX_ITEMS
itemSources     resb MAX_ITEMS
itemAvailability resb MAX_ITEMS
searchText      resb 96
pageText        resb 32
scanSummary     resb 64
scanProgressText resb 64
applyTitle      resq 1
applyMessage    resq 1
toggleName      resb 128

section .text
entry:
    enter_frame
    call    o3_first_instance
    test    eax, eax
    jz      .done
    call    fit_work_area
    xor     ecx, ecx
    call    GetModuleHandleA
    mov     [instanceHandle], rax
    mov     dword [windowClass], 80
    lea     rdx, [window_proc]
    mov     [windowClass+8], rdx
    mov     [windowClass+24], rax
    xor     ecx, ecx
    mov     edx, 32512
    call    LoadCursorA
    mov     [windowClass+40], rax
    lea     rax, [className]
    mov     [windowClass+64], rax
    lea     rcx, [windowClass]
    call    RegisterClassExA
    test    eax, eax
    jz      .fail

    lea     rcx, [windowRect]
    mov     edx, WINDOW_STYLE
    xor     r8d, r8d
    mov     r9d, WINDOW_EX_STYLE
    call    AdjustWindowRectEx
    mov     eax, [windowRect+8]
    sub     eax, [windowRect]
    mov     edx, [windowRect+12]
    sub     edx, [windowRect+4]
    mov     ecx, 0x80000000
    mov     [rsp+32], rcx
    mov     [rsp+40], rcx
    mov     [rsp+48], rax
    mov     [rsp+56], rdx
    mov     qword [rsp+64], 0
    mov     qword [rsp+72], 0
    mov     rax, [instanceHandle]
    mov     [rsp+80], rax
    mov     qword [rsp+88], 0
    mov     ecx, WINDOW_EX_STYLE
    lea     rdx, [className]
    lea     r8, [windowTitle]
    mov     r9d, WINDOW_STYLE
    call    CreateWindowExA
    test    rax, rax
    jz      .fail
    mov     rcx, [mainWindow]
    mov     edx, 5
    call    ShowWindow
    mov     rcx, [mainWindow]
    call    UpdateWindow
.messages:
    lea     rcx, [messageBuffer]
    xor     edx, edx
    xor     r8d, r8d
    xor     r9d, r9d
    call    GetMessageA
    test    eax, eax
    js      .fail
    jz      .done
    mov     rcx, [mainWindow]
    lea     rdx, [messageBuffer]
    call    IsDialogMessageA
    test    eax, eax
    jnz     .messages
    lea     rcx, [messageBuffer]
    call    TranslateMessage
    lea     rcx, [messageBuffer]
    call    DispatchMessageA
    jmp     .messages
.done:
    xor     ecx, ecx
    call    ExitProcess
.fail:
    mov     ecx, 1
    call    ExitProcess

fit_work_area:
    enter_frame
    mov     ecx, 0x30
    xor     edx, edx
    lea     r8, [workArea]
    xor     r9d, r9d
    call    SystemParametersInfoA
    test    eax, eax
    jz      .done
    mov     eax, [workArea+8]
    sub     eax, [workArea]
    cmp     eax, 1020
    jl      .physical
    mov     eax, [workArea+12]
    sub     eax, [workArea+4]
    cmp     eax, 738
    jge     .done
.physical:
    call    SetProcessDPIAware
.done:
    leave_frame

window_proc:
    cmp     edx, WM_CREATE
    je      .create
    cmp     edx, WM_PAINT
    je      .paint
    cmp     edx, WM_ERASEBKGND
    je      .erased
    cmp     edx, WM_NCHITTEST
    je      .hit_test
    cmp     edx, WM_DRAWITEM
    je      .draw
    cmp     edx, WM_CTLCOLOREDIT
    je      .color_edit
    cmp     edx, WM_SCAN_READY
    je      .scan_ready
    cmp     edx, 0x0113
    je      .refresh_switches
    cmp     edx, WM_SCAN_ITEM
    je      .scan_item
    cmp     edx, WM_COMMAND
    je      .command
    cmp     edx, WM_DESTROY
    je      .destroy
    cmp     edx, WM_CLOSE
    je      .confirm_close
    jmp     DefWindowProcA
.confirm_close:
    enter_frame
    mov     [rsp+96], rcx
    lea     rdx, [closeConfirm]
    lea     r8, [closeTitle]
    mov     r9d, 0x41
    call    MessageBoxA
    cmp     eax, 1
    jne     .keep_open
    call    save_view_state
    mov     rcx, [rsp+96]
    call    DestroyWindow
.keep_open:
    xor     eax, eax
    leave_frame
.create:
    enter_frame
    mov     [mainWindow], rcx
    call    o3_record_launch
    mov     [launchCount], eax
    test    eax, eax
    jz      .create_controls
    mov     r8d, eax
    lea     rcx, [scanSummary]
    lea     rdx, [fmtLaunch]
    call    wsprintfA
    mov     rcx, [mainWindow]
    lea     rdx, [scanSummary]
    call    SetWindowTextA
.create_controls:
    call    misc_init
    call    create_ui
    call    layout_checked_tabs
    call    rebuild_view
    call    begin_scan
    mov     rcx, [mainWindow]
    mov     edx, 1
    mov     r8d, 2000
    xor     r9d, r9d
    call    SetTimer
    call    update_controls
    xor     eax, eax
    leave_frame
.refresh_switches:
    cmp     dword [scanBusy], 0
    jne     .skip_tick
    cmp     dword [catalogueReady], 0
    je      .skip_tick
    enter_frame
    call    misc_tick
    call    scan_memory
    lea     rcx, [itemStates]
    lea     rdx, [itemSources]
    lea     r8, [itemAvailability]
    call    o3_refresh_switches
    test    eax, eax
    jz      .tick_done
    call    rebuild_checked_view
    call    show_scan_summary
    call    update_controls
.tick_done:
    xor     eax, eax
    leave_frame
.skip_tick:
    xor     eax, eax
    ret
.paint:
    enter_frame
    lea     rdx, [paintBuffer]
    call    BeginPaint
    mov     [drawingDc], rax
    call    paint_window
    mov     rcx, [mainWindow]
    lea     rdx, [paintBuffer]
    call    EndPaint
    xor     eax, eax
    leave_frame
.erased:
    mov     eax, 1
    ret
.hit_test:
    enter_frame
    movsx   eax, r9w
    mov     [rsp+96], eax
    sar     r9d, 16
    mov     [rsp+100], r9d
    lea     rdx, [rsp+96]
    call    ScreenToClient
    test    eax, eax
    jz      .client_hit
    cmp     dword [rsp+100], CAPTION_HEIGHT
    jae     .client_hit
    cmp     dword [rsp+96], CAPTION_BUTTON_X
    jae     .client_hit
    mov     eax, 2
    leave_frame
.client_hit:
    mov     eax, 1
    leave_frame
.draw:
    mov     rcx, r9
    jmp     draw_button
.color_edit:
    enter_frame
    mov     [rsp+96], r8
    mov     rcx, r8
    mov     edx, INK
    call    SetTextColor
    mov     rcx, [rsp+96]
    mov     edx, EDIT_BG
    call    SetBkColor
    mov     rax, [brushes+STRIP*8]
    leave_frame
.scan_item:
    cmp     dword [scanBusy], 0
    je      .ignore_scan_item
    cmp     dword [applyItem], 0
    jne     .ignore_scan_item
    cmp     r8d, [o3_item_total]
    ja      .ignore_scan_item
    enter_frame
    mov     [scanProgress], r8d
    test    r8d, 7
    jne     .scanning
    call    show_scan_progress
    mov     rcx, [mainWindow]
    call    redraw
.scanning:
    xor     eax, eax
    leave_frame
.ignore_scan_item:
    xor     eax, eax
    ret
.scan_ready:
    enter_frame
    mov     rcx, [scanThread]
    test    rcx, rcx
    jz      .scan_update
    call    CloseHandle
    mov     qword [scanThread], 0
.scan_update:
    mov     dword [scanBusy], 0
    mov     dword [catalogueReady], 1
    call    rebuild_checked_view
    call    load_view_state
    mov     rcx, [refreshButton]
    mov     edx, 1
    call    EnableWindow
    mov     rcx, [searchBox]
    mov     edx, 1
    call    EnableWindow
    mov     rcx, [restoreButton]
    mov     edx, 1
    call    EnableWindow
    cmp     dword [restoreBusy], 0
    je      .apply_result
    mov     rax, [restoreResult]
    mov     [applyMessage], rax
    mov     dword [restoreBusy], 0
.apply_result:
    cmp     dword [applyItem], 0
    je      .scan_summary
    mov     eax, [applyResult]
    lea     rdx, [applyLabels]
    mov     rax, [rdx+rax*8]
    mov     [applyMessage], rax
    call    energy_saved_note
    mov     dword [applyItem], 0
.scan_summary:
    call    show_scan_summary
    call    update_controls
    xor     eax, eax
    leave_frame
.command:
    test    r9, r9
    jz      .default
    enter_frame
    movzx   eax, r8w
    cmp     eax, ID_SEARCH
    je      .search
    cmp     eax, ID_MINIMIZE
    je      .minimize
    cmp     eax, ID_CLOSE
    je      .close
    cmp     eax, ID_REFRESH
    je      .refresh
    cmp     eax, ID_RESTORE
    je      .restore
    cmp     eax, ID_EMULATOR
    jb      .handled
    cmp     eax, ID_MISC
    jbe     .tab
    cmp     eax, ID_PREV
    je      .prev
    cmp     eax, ID_NEXT
    je      .next
    cmp     eax, ID_SPECIAL
    jae     .special
    cmp     eax, ID_FIRST_ON
    jae     .toggle
    sub     eax, ID_FIRST_ROW
    cmp     eax, PAGE_SIZE
    jae     .handled
    mov     edx, [pageOffset]
    add     edx, eax
    cmp     edx, [viewCount]
    jae     .handled
    mov     [activeRow], eax
    cmp     dword [scanBusy], 0
    jne     .update
    call    selected_feature
    test    rax, rax
    jz      .update
    lea     edx, [r10+1]
    mov     rcx, [mainWindow]
    call    o3_choose_target
    test    eax, eax
    jz      .update
    lea     rcx, [itemStates]
    lea     rdx, [itemSources]
    lea     r8, [itemAvailability]
    call    o3_refresh_switches
    call    rebuild_checked_view
    call    show_scan_summary
    jmp     .update
.search:
    shr     r8d, 16
    cmp     r8d, EN_CHANGE
    jne     .handled
    cmp     dword [scanBusy], 0
    jne     .handled
    call    rebuild_view
    jmp     .update
.minimize:
    mov     rcx, [mainWindow]
    mov     edx, 6
    call    ShowWindow
    jmp     .handled
.close:
    mov     rcx, [mainWindow]
    mov     edx, WM_CLOSE
    xor     r8d, r8d
    xor     r9d, r9d
    call    PostMessageA
    jmp     .handled
.refresh:
    call    begin_scan
    jmp     .update
.restore:
    cmp     dword [scanBusy], 0
    jne     .handled
    mov     rcx, [mainWindow]
    lea     rdx, [restoreConfirm]
    lea     r8, [restoreTitle]
    mov     r9d, 0x134
    call    MessageBoxA
    cmp     eax, 6
    jne     .handled
    cmp     dword [scanBusy], 0
    jne     .handled
    mov     dword [restoreBusy], 1
    lea     rax, [restoreTitle]
    mov     [applyTitle], rax
    lea     rax, [restoreRunning]
    mov     [applyMessage], rax
    call    start_worker
    jmp     .update
.tab:
    cmp     dword [scanBusy], 0
    jne     .handled
    sub     eax, ID_EMULATOR
    mov     [activeTab], eax
    mov     rcx, [searchBox]
    lea     rdx, [emptyText]
    call    SetWindowTextA
    call    rebuild_view
    jmp     .update
.prev:
    cmp     dword [pageOffset], PAGE_SIZE
    jb      .handled
    sub     dword [pageOffset], PAGE_SIZE
    mov     dword [activeRow], 0
    jmp     .update
.next:
    mov     eax, [pageOffset]
    add     eax, PAGE_SIZE
    cmp     eax, [viewCount]
    jae     .handled
    mov     [pageOffset], eax
    mov     dword [activeRow], 0
    jmp     .update
.special:
    cmp     eax, ID_LICENSE
    jne     .special_toggle
    mov     rcx, [mainWindow]
    lea     rdx, [licenseText]
    lea     r8, [licenseTitle]
    mov     r9d, 0x40
    call    MessageBoxA
    jmp     .handled
.special_toggle:
    cmp     dword [scanBusy], 0
    jne     .handled
    sub     eax, ID_SPECIAL
    cmp     eax, FEATURES
    jae     .handled
    mov     ecx, eax
    call    misc_toggle
    jmp     .update
.toggle:
    cmp     dword [scanBusy], 0
    jne     .handled
    xor     ecx, ecx
    cmp     eax, ID_FIRST_OFF
    jae     .toggle_off
    inc     ecx
    sub     eax, ID_FIRST_ON
    jmp     .toggle_row
.toggle_off:
    sub     eax, ID_FIRST_OFF
.toggle_row:
    cmp     eax, PAGE_SIZE
    jae     .handled
    mov     [activeRow], eax
    mov     ecx, 3
    call    begin_apply
    jmp     .update
.update:
    call    update_controls
.handled:
    xor     eax, eax
    leave_frame
.default:
    jmp     DefWindowProcA
.destroy:
    enter_frame
    call    o3_release_efficiency
    call    misc_shutdown
    mov     rcx, [scanThread]
    test    rcx, rcx
    jz      .dispose_ui
    call    CloseHandle
.dispose_ui:
    call    release_ui
    xor     ecx, ecx
    call    PostQuitMessage
    xor     eax, eax
    leave_frame

%macro add_button 6-7
    mov     ecx, %1
    mov     edx, %2
    mov     r8d, %3
    mov     r9d, %4
    mov     qword [rsp+32], %5
    lea     rax, [%6]
    mov     [rsp+40], rax
    call    create_button
%if %0 = 7
    mov     [%7], rax
%endif
%endmacro

create_ui:
    enter_frame
    mov     ecx, 8
    call    GetStockObject
    mov     [nullPen], rax
    mov     dword [rsp+96], 0
.brush:
    mov     eax, [rsp+96]
    lea     rdx, [brushColors]
    mov     ecx, [rdx+rax*4]
    call    CreateSolidBrush
    mov     ecx, [rsp+96]
    lea     rdx, [brushes]
    mov     [rdx+rcx*8], rax
    inc     ecx
    mov     [rsp+96], ecx
    cmp     ecx, BRUSH_COUNT
    jb      .brush
    mov     ecx, -18
    mov     edx, 650
    lea     r8, [displayFont]
    call    make_font
    mov     [fontHeading], rax
    mov     ecx, -15
    mov     edx, 400
    lea     r8, [fontName]
    call    make_font
    mov     [fontRegular], rax
    mov     ecx, -12
    mov     edx, 400
    lea     r8, [fontName]
    call    make_font
    mov     [fontSmall], rax
    mov     ecx, -15
    mov     edx, 650
    lea     r8, [displayFont]
    call    make_font
    mov     [fontStrong], rax
    mov     ecx, -13
    mov     edx, 400
    lea     r8, [logFont]
    call    make_font
    mov     [fontLog], rax

    call    create_search
    add_button 827, 81, 127, 22, ID_REFRESH, refreshTitle, refreshButton
    add_button 0, TAB_TOP, 160, TAB_HEIGHT, ID_EMULATOR, emulatorTab, tabButtons
    add_button 0, TAB_TOP, 160, TAB_HEIGHT, ID_WINDOWS, windowsTab, tabButtons+8
    add_button 0, TAB_TOP, 160, TAB_HEIGHT, ID_BACKGROUND, backgroundTab, tabButtons+16
    add_button 0, TAB_TOP, 160, TAB_HEIGHT, ID_EXPLORER, explorerTab, tabButtons+24
    add_button 0, TAB_TOP, 160, TAB_HEIGHT, ID_CPU, cpuTab, tabButtons+32
    add_button 0, TAB_TOP, 160, TAB_HEIGHT, ID_RAM, ramTab, tabButtons+40
    add_button 0, TAB_TOP, 160, TAB_HEIGHT, ID_ENERGY, energyTab, tabButtons+48
    add_button 0, TAB_TOP, 160, TAB_HEIGHT, ID_UNAVAILABLE, unavailableTab, tabButtons+56
    add_button 0, TAB_TOP, 160, TAB_HEIGHT, ID_MISC, miscTab, tabButtons+64
    add_button 870, 223, SWITCH_WIDTH, SWITCH_HEIGHT, ID_SPECIAL, toggleOn, specialButtons
    add_button 870, 281, SWITCH_WIDTH, SWITCH_HEIGHT, ID_SPECIAL+1, toggleOn, specialButtons+8
    add_button 870, 339, SWITCH_WIDTH, SWITCH_HEIGHT, ID_SPECIAL+2, toggleOn, specialButtons+16
    add_button 822, 458, 128, 26, ID_LICENSE, licenseButtonText, specialButtons+24
    mov     dword [rsp+96], 0
.row:
    mov     ecx, [rsp+96]
    call    create_row_controls
    inc     dword [rsp+96]
    cmp     dword [rsp+96], PAGE_SIZE
    jb      .row
    add_button 337, PAGE_BUTTON_Y, 100, PAGE_BUTTON_H, ID_PREV, prevTitle, prevButton
    add_button 563, PAGE_BUTTON_Y, 100, PAGE_BUTTON_H, ID_NEXT, nextTitle, nextButton
    add_button 41, PAGE_BUTTON_Y, 140, PAGE_BUTTON_H, ID_RESTORE, restoreTitle, restoreButton
    add_button CAPTION_BUTTON_X, 8, 26, 26, ID_MINIMIZE, minimizeLabel
    add_button CAPTION_BUTTON_X+31, 8, 26, 26, ID_CLOSE, closeLabel
    leave_frame

row_position:
    mov     edx, ecx
    and     edx, PAGE_ROWS-1
    imul    edx, ROW_STRIDE
    add     edx, ROW_TOP
    shr     ecx, 3
    imul    ecx, COLUMN_STRIDE
    add     ecx, ROW_LEFT
    ret

%macro create_row_control 7
    mov     ecx, [rsp+100]
    add     ecx, %1
    mov     edx, [rsp+104]
    add     edx, %2
    mov     r8d, %3
    mov     r9d, %4
    mov     eax, [rsp+96]
    add     eax, %5
    mov     [rsp+32], rax
    lea     rax, [%6]
    mov     [rsp+40], rax
    call    create_button
    mov     ecx, [rsp+96]
    lea     rdx, [%7]
    mov     [rdx+rcx*8], rax
%endmacro

create_row_controls:
    enter_frame
    mov     [rsp+96], ecx
    call    row_position
    mov     [rsp+100], ecx
    mov     [rsp+104], edx
    create_row_control 5, 2, ROW_BODY_WIDTH, ROW_HEIGHT-4, ID_FIRST_ROW, emptyText, rowButtons
    create_row_control SWITCH_X, SWITCH_Y, SWITCH_WIDTH, SWITCH_HEIGHT, ID_FIRST_ON, toggleOn, rowOnButtons
    create_row_control SWITCH_X, SWITCH_Y, SWITCH_WIDTH, SWITCH_HEIGHT, ID_FIRST_OFF, toggleOff, rowOffButtons
    leave_frame

create_search:
    enter_frame
    mov     qword [rsp+32], 829
    mov     qword [rsp+40], 59
    mov     qword [rsp+48], 123
    mov     qword [rsp+56], 20
    mov     rax, [mainWindow]
    mov     [rsp+64], rax
    mov     qword [rsp+72], ID_SEARCH
    mov     rax, [instanceHandle]
    mov     [rsp+80], rax
    mov     qword [rsp+88], 0
    xor     ecx, ecx
    lea     rdx, [editClass]
    lea     r8, [emptyText]
    mov     r9d, 0x50010080
    call    CreateWindowExA
    mov     [searchBox], rax
    mov     rcx, rax
    mov     edx, 0x0030
    mov     r8, [fontRegular]
    mov     r9d, 1
    call    SendMessageW
    mov     rcx, [searchBox]
    mov     edx, 0x1501
    mov     r8d, 1
    lea     r9, [searchPrompt]
    call    SendMessageW
    mov     rcx, [searchBox]
    mov     edx, 0x00d3
    mov     r8d, 3
    mov     r9d, 0x000a000a
    call    SendMessageW
    leave_frame

create_button:
    enter_frame
    mov     [rsp+32], rcx
    mov     [rsp+40], rdx
    mov     [rsp+48], r8
    mov     [rsp+56], r9
    mov     rax, [mainWindow]
    mov     [rsp+64], rax
    mov     rax, [rsp+176]
    mov     [rsp+72], rax
    mov     rax, [instanceHandle]
    mov     [rsp+80], rax
    mov     qword [rsp+88], 0
    xor     ecx, ecx
    lea     rdx, [buttonClass]
    mov     r8, [rsp+184]
    mov     r9d, BUTTON_STYLE
    call    CreateWindowExA
    leave_frame

make_font:
    enter_frame
    mov     [rsp+32], rdx
    mov     qword [rsp+40], 0
    mov     qword [rsp+48], 0
    mov     qword [rsp+56], 0
    mov     qword [rsp+64], 1
    mov     qword [rsp+72], 0
    mov     qword [rsp+80], 0
    mov     qword [rsp+88], 5
    mov     qword [rsp+96], 0
    mov     [rsp+104], r8
    xor     edx, edx
    xor     r8d, r8d
    xor     r9d, r9d
    call    CreateFontA
    leave_frame

release_ui:
    enter_frame
    mov     dword [rsp+96], 0
.brush:
    mov     eax, [rsp+96]
    lea     rdx, [brushes]
    mov     rcx, [rdx+rax*8]
    call    DeleteObject
    inc     dword [rsp+96]
    cmp     dword [rsp+96], BRUSH_COUNT
    jb      .brush
    mov     rcx, [fontHeading]
    call    DeleteObject
    mov     rcx, [fontRegular]
    call    DeleteObject
    mov     rcx, [fontSmall]
    call    DeleteObject
    mov     rcx, [fontStrong]
    call    DeleteObject
    mov     rcx, [fontLog]
    call    DeleteObject
    leave_frame

begin_scan:
    enter_frame
    cmp     dword [scanBusy], 0
    jne     .done
    mov     dword [applyItem], 0
    mov     dword [scanProgress], 0
    call    show_scan_progress
    call    start_worker
.done:
    leave_frame

begin_apply:
    enter_frame
    cmp     dword [backupReady], 0
    je      .done
    cmp     dword [scanBusy], 0
    jne     .done
    mov     [rsp+96], ecx
    call    selected_feature
    test    rax, rax
    jz      .done
    mov     r9, [rax+Feature.title]
    lea     rdx, [itemAvailability]
    movzx   ecx, byte [rdx+r10]
    dec     ecx
    cmp     ecx, 1
    ja      .done
    mov     [applyTitle], r9
    mov     eax, [rsp+96]
    mov     [applyValue], eax
    inc     r10d
    mov     [applyItem], r10d
    lea     rax, [footerApplying]
    mov     [applyMessage], rax
    call    start_worker
.done:
    leave_frame

start_worker:
    enter_frame
    mov     dword [scanBusy], 1
    mov     rcx, [refreshButton]
    xor     edx, edx
    call    EnableWindow
    mov     rcx, [searchBox]
    xor     edx, edx
    call    EnableWindow
    mov     rcx, [restoreButton]
    xor     edx, edx
    call    EnableWindow
    xor     ecx, ecx
    xor     edx, edx
    lea     r8, [scan_worker]
    xor     r9d, r9d
    mov     qword [rsp+32], 0
    mov     qword [rsp+40], 0
    call    CreateThread
    mov     [scanThread], rax
    test    rax, rax
    jnz     .done
    call    scan_worker
.done:
    leave_frame

scan_worker:
    enter_frame
    cmp     dword [restoreBusy], 0
    jne     .restore
    mov     ecx, [applyItem]
    test    ecx, ecx
    jnz     .apply
    call    scan_device
    lea     rcx, [itemStates]
    lea     rdx, [probeManifest]
    lea     r8, [configForProbe]
    lea     r9, [instanceName]
    lea     rax, [itemSources]
    mov     [rsp+32], rax
    xor     eax, eax
    cmp     dword [playerCount], 1
    seta    al
    mov     [rsp+40], rax
    call    o3_collect
    lea     rcx, [itemStates]
    lea     rdx, [itemSources]
    lea     r8, [itemAvailability]
    mov     r9, [mainWindow]
    call    o3_options
    lea     rcx, [catalogue]
    lea     rdx, [itemStates]
    lea     r8, [itemSources]
    lea     r9, [itemAvailability]
    call    o3_backup_first
    mov     [backupReady], eax
    jmp     .ready
.apply:
    lea     r8, [itemStates]
    lea     r8, [r8+rcx-1]
    lea     r9, [itemSources]
    lea     r9, [r9+rcx-1]
    lea     rax, [itemAvailability]
    lea     rax, [rax+rcx-1]
    mov     [rsp+32], rax
    mov     edx, [applyValue]
    call    o3_apply
    mov     [applyResult], eax
    jmp     .ready
.restore:
    lea     rcx, [itemStates]
    lea     rdx, [itemSources]
    lea     r8, [itemAvailability]
    call    o3_restore
    mov     [restoreResult], rax
    call    misc_restore_all
.ready:
    mov     rcx, [mainWindow]
    mov     edx, WM_SCAN_READY
    xor     r8d, r8d
    xor     r9d, r9d
    call    PostMessageA
    xor     eax, eax
    leave_frame

show_scan_progress:
    enter_frame
    lea     rcx, [scanProgressText]
    lea     rdx, [fmtScanProgress]
    mov     r8d, [scanProgress]
    mov     r9d, [o3_item_total]
    call    wsprintfA
    lea     rax, [scanProgressText]
    mov     [scanMessage], rax
    leave_frame

show_scan_summary:
    enter_frame
    xor     eax, eax
    xor     ecx, ecx
    lea     rdx, [itemStates]
.count:
    cmp     byte [rdx+rax], 5
    jne     .advance
    inc     ecx
.advance:
    inc     eax
    cmp     eax, [o3_item_total]
    jb      .count
    mov     [rsp+32], rcx
    lea     rcx, [scanSummary]
    lea     rdx, [fmtScanResult]
    mov     r8d, [o3_item_total]
    mov     r9d, [categoryTotals+UNAVAILABLE_TAB*4]
    sub     r8d, r9d
    call    wsprintfA
    lea     rax, [scanSummary]
    lea     rdx, [recreatedBackup]
    cmp     dword [backupReady], 2
    cmove   rax, rdx
    lea     rdx, [backupError]
    cmp     dword [backupReady], 0
    cmove   rax, rdx
    mov     [scanMessage], rax
    leave_frame

is_writable:
    lea     rax, [itemAvailability]
    movzx   eax, byte [rax+rcx]
    cmp     eax, 6
    jne     .range
    mov     eax, 1
    ret
.range:
    dec     eax
    cmp     eax, 1
    setbe   al
    movzx   eax, al
    ret

recount_catalogue:
    enter_frame
    xor     eax, eax
.clear:
    lea     rdx, [categoryTotals]
    mov     dword [rdx+rax*4], 0
    inc     eax
    cmp     eax, TAB_COUNT
    jb      .clear
    mov     dword [rsp+96], 0
.group:
    mov     eax, [rsp+96]
    lea     rdx, [catalogue_starts]
    mov     ecx, [rdx+rax*4]
    mov     [rsp+104], ecx
    lea     rdx, [catalogue_counts]
    add     ecx, [rdx+rax*4]
    mov     [rsp+108], ecx
    cmp     ecx, [rsp+104]
    je      .group_done
.item:
    mov     ecx, [rsp+104]
    call    is_writable
    mov     ecx, UNAVAILABLE_TAB
    test    eax, eax
    cmovnz  ecx, [rsp+96]
    lea     rdx, [categoryTotals]
    inc     dword [rdx+rcx*4]
    inc     dword [rsp+104]
    mov     eax, [rsp+104]
    cmp     eax, [rsp+108]
    jb      .item
.group_done:
    inc     dword [rsp+96]
    cmp     dword [rsp+96], CATEGORY_COUNT
    jb      .group
    leave_frame

layout_checked_tabs:
    enter_frame
    mov     ecx, 2
    xor     eax, eax
    lea     rdx, [categoryTotals]
.count:
    cmp     dword [rdx+rax*4], 0
    je      .count_next
    inc     ecx
.count_next:
    inc     eax
    cmp     eax, CATEGORY_COUNT
    jb      .count
    mov     eax, TAB_RIGHT-TAB_LEFT+TAB_GAP
    xor     edx, edx
    div     ecx
    sub     eax, TAB_GAP
    mov     [rsp+120], eax
    mov     dword [rsp+96], 0
    mov     dword [rsp+104], TAB_LEFT
.item:
    mov     eax, [rsp+96]
    lea     rdx, [tabButtons]
    mov     rcx, [rdx+rax*8]
    mov     [rsp+112], rcx
    cmp     dword [catalogueReady], 0
    je      .hidden
    cmp     eax, UNAVAILABLE_TAB
    jae     .visible
    lea     rdx, [categoryTotals]
    cmp     dword [rdx+rax*4], 0
    jne     .visible
.hidden:
    xor     edx, edx
    call    ShowWindow
    jmp     .next
.visible:
    mov     edx, [rsp+104]
    mov     r8d, TAB_TOP
    mov     r9d, [rsp+120]
    mov     qword [rsp+32], TAB_HEIGHT
    mov     qword [rsp+40], 1
    call    MoveWindow
    mov     rcx, [rsp+112]
    mov     edx, 5
    call    ShowWindow
    mov     eax, [rsp+120]
    lea     eax, [eax+TAB_GAP]
    add     [rsp+104], eax
.next:
    inc     dword [rsp+96]
    cmp     dword [rsp+96], TAB_COUNT
    jb      .item
    leave_frame

rebuild_checked_view:
    enter_frame
    call    selected_feature
    mov     dword [rsp+96], -1
    test    rax, rax
    jz      .count
    mov     [rsp+96], r10d
.count:
    call    recount_catalogue
    mov     eax, [activeTab]
    cmp     eax, UNAVAILABLE_TAB
    jae     .layout
    lea     rdx, [categoryTotals]
    cmp     dword [rdx+rax*4], 0
    jne     .layout
    xor     eax, eax
.find_category:
    cmp     eax, CATEGORY_COUNT
    jae     .select
    cmp     dword [rdx+rax*4], 0
    jne     .select
    inc     eax
    jmp     .find_category
.select:
    mov     [activeTab], eax
.layout:
    call    layout_checked_tabs
    call    rebuild_view
    mov     edx, [rsp+96]
    lea     rcx, [viewIndices]
    xor     eax, eax
.find_row:
    cmp     eax, [viewCount]
    jae     .done
    cmp     edx, [rcx+rax*4]
    je      .preserve
    inc     eax
    jmp     .find_row
.preserve:
    mov     ecx, eax
    and     eax, -PAGE_SIZE
    and     ecx, PAGE_SIZE-1
    mov     [pageOffset], eax
    mov     [activeRow], ecx
.done:
    leave_frame

rebuild_view:
    enter_frame
    mov     dword [viewCount], 0
    mov     dword [pageOffset], 0
    mov     dword [activeRow], 0
    cmp     dword [catalogueReady], 0
    je      .done
    mov     rcx, [searchBox]
    lea     rdx, [searchText]
    mov     r8d, 96
    call    GetWindowTextA
    mov     dword [rsp+96], 0
    mov     r9d, eax
    mov     eax, [o3_item_total]
    mov     [rsp+112], eax
    mov     eax, r9d
    test    eax, eax
    jnz     .candidate
    mov     eax, [activeTab]
    cmp     eax, MISC_TAB
    je      .done
    cmp     eax, UNAVAILABLE_TAB
    je      .candidate
    lea     rdx, [catalogue_starts]
    mov     ecx, [rdx+rax*4]
    mov     [rsp+96], ecx
    lea     rdx, [catalogue_counts]
    add     ecx, [rdx+rax*4]
    mov     [rsp+112], ecx
    cmp     ecx, [rsp+96]
    je      .done
.candidate:
    mov     ecx, [rsp+96]
    call    is_writable
    xor     ecx, ecx
    cmp     dword [activeTab], UNAVAILABLE_TAB
    setne   cl
    cmp     eax, ecx
    jne     .search_next
    cmp     byte [searchText], 0
    je      .match
    mov     eax, [rsp+96]
    imul    eax, Feature_size
    lea     rdx, [catalogue]
    add     rax, rdx
    mov     [rsp+104], rax
    mov     rcx, [rax+Feature.title]
    lea     rdx, [searchText]
    call    StrStrIA
    test    rax, rax
    jnz     .match
    mov     rax, [rsp+104]
    mov     rcx, [rax+Feature.detail]
    lea     rdx, [searchText]
    call    StrStrIA
    test    rax, rax
    jz      .search_next
.match:
    mov     eax, [viewCount]
    mov     ecx, [rsp+96]
    lea     rdx, [viewIndices]
    mov     [rdx+rax*4], ecx
    inc     dword [viewCount]
.search_next:
    inc     dword [rsp+96]
    mov     eax, [rsp+96]
    cmp     eax, [rsp+112]
    jb      .candidate
.done:
    leave_frame

feature_at_row:
    add     ecx, [pageOffset]
    cmp     ecx, [viewCount]
    jae     .no_feature
    lea     rdx, [viewIndices]
    mov     r10d, [rdx+rcx*4]
    imul    eax, r10d, Feature_size
    lea     rdx, [catalogue]
    add     rax, rdx
    ret
.no_feature:
    xor     eax, eax
    ret

feature_status:
    call    item_pending_status
    mov     ecx, eax
    test    eax, eax
    jnz     .label
    lea     rax, [itemStates]
    movzx   ecx, byte [rax+r10]
.label:
    lea     rax, [statusLabels]
    mov     rdx, [rax+rcx*8]
    ret

item_pending_status:
    xor     eax, eax
    cmp     dword [scanBusy], 0
    je      .done
    mov     eax, 6
    cmp     dword [applyItem], 0
    je      .done
    lea     eax, [r10+1]
    cmp     eax, [applyItem]
    sete    al
    movzx   eax, al
    shl     eax, 3
.done:
    ret

selected_feature:
    mov     ecx, [activeRow]
    jmp     feature_at_row

update_controls:
    enter_frame
    mov     dword [rsp+96], 0
.row:
    mov     ecx, [rsp+96]
    call    feature_at_row
    mov     [rsp+104], rax
    test    rax, rax
    jz      .hide_row
    mov     rdx, [rax+Feature.title]
    mov     eax, [rsp+96]
    lea     r8, [rowButtons]
    mov     rcx, [r8+rax*8]
    call    SetWindowTextA
.hide_row:
    xor     edx, edx
    cmp     qword [rsp+104], 0
    setne   dl
    imul    edx, 5
    mov     eax, [rsp+96]
    lea     r8, [rowButtons]
    mov     rcx, [r8+rax*8]
    call    ShowWindow
    mov     ecx, [rsp+96]
    call    update_row_toggles
    inc     dword [rsp+96]
    cmp     dword [rsp+96], PAGE_SIZE
    jb      .row
    xor     edx, edx
    cmp     dword [viewCount], PAGE_SIZE
    seta    dl
    imul    edx, 5
    mov     [rsp+96], edx
    mov     rcx, [prevButton]
    call    ShowWindow
    mov     rcx, [nextButton]
    mov     edx, [rsp+96]
    call    ShowWindow
    xor     edx, edx
    cmp     dword [pageOffset], PAGE_SIZE
    setae   dl
    mov     rcx, [prevButton]
    call    EnableWindow
    xor     edx, edx
    mov     eax, [pageOffset]
    add     eax, PAGE_SIZE
    cmp     eax, [viewCount]
    setb    dl
    mov     rcx, [nextButton]
    call    EnableWindow
    call    update_page_text
    mov     rcx, [mainWindow]
    call    redraw
    mov     rcx, [tabButtons]
    call    redraw
    mov     rcx, [tabButtons+8]
    call    redraw
    mov     rcx, [tabButtons+16]
    call    redraw
    mov     rcx, [tabButtons+24]
    call    redraw
    mov     rcx, [tabButtons+32]
    call    redraw
    mov     rcx, [tabButtons+40]
    call    redraw
    mov     rcx, [tabButtons+48]
    call    redraw
    mov     rcx, [tabButtons+56]
    call    redraw
    mov     rcx, [tabButtons+64]
    call    redraw
    call    misc_show
    mov     rcx, [refreshButton]
    call    redraw
    mov     rcx, [restoreButton]
    call    redraw
    mov     dword [rsp+96], 0
.redraw_row:
    mov     eax, [rsp+96]
    lea     rdx, [rowButtons]
    mov     rcx, [rdx+rax*8]
    call    redraw
    inc     dword [rsp+96]
    cmp     dword [rsp+96], PAGE_SIZE
    jb      .redraw_row
    leave_frame

update_row_toggles:
    enter_frame
    mov     [rsp+108], ecx
    lea     rdx, [rowOnButtons]
    mov     rax, [rdx+rcx*8]
    mov     [rsp+96], rax
    lea     rdx, [rowOffButtons]
    mov     rax, [rdx+rcx*8]
    mov     [rsp+120], rax
    call    feature_at_row
    test    rax, rax
    jz      .hide
    mov     rdx, [rax+Feature.title]
    mov     [rsp+112], rdx
    call    item_pending_status
    test    eax, eax
    jnz     .hide
    lea     rax, [itemAvailability]
    movzx   eax, byte [rax+r10]
    dec     eax
    cmp     eax, 1
    ja      .hide
    lea     rax, [itemStates]
    movzx   eax, byte [rax+r10]
    lea     rdx, [o3_defaults]
    cmp     byte [rdx+r10], 0
    je      .explicit
    mov     eax, 3
.explicit:
    mov     [rsp+104], eax

    mov     rcx, [rsp+96]
    mov     edx, [rsp+108]
    mov     r8, [rsp+112]
    mov     r9d, -1
    mov     [rsp+32], rax
    call    position_row_toggle
    mov     rcx, [rsp+120]
    xor     edx, edx
    call    ShowWindow
    leave_frame
.hide:
    mov     rcx, [rsp+96]
    xor     edx, edx
    call    ShowWindow
    mov     rcx, [rsp+120]
    xor     edx, edx
    call    ShowWindow
    leave_frame

position_row_toggle:
    enter_frame
    mov     eax, [rsp+176]
    cmp     eax, r9d
    je      .hide
    mov     [rsp+96], rcx
    mov     [rsp+104], r8
    mov     [rsp+112], r9d
    mov     [rsp+116], eax
    mov     ecx, edx
    call    row_position

    add     ecx, SWITCH_X
    add     edx, SWITCH_Y
    mov     r9d, SWITCH_WIDTH
    mov     r8d, edx
    mov     edx, ecx
    mov     rcx, [rsp+96]
    mov     qword [rsp+32], SWITCH_HEIGHT
    mov     qword [rsp+40], 0
    call    MoveWindow

    lea     r9, [toggleOn]
    cmp     dword [rsp+116], 1
    je      .name
    lea     r9, [defaultLabel]
    cmp     dword [rsp+116], 3
    je      .name
    lea     r9, [toggleOff]
.name:
    lea     rcx, [toggleName]
    lea     rdx, [fmtToggleName]
    mov     r8, [rsp+104]
    call    wsprintfA
    mov     rcx, [rsp+96]
    lea     rdx, [toggleName]
    call    SetWindowTextA
    mov     rcx, [rsp+96]
    xor     edx, edx
    cmp     dword [scanBusy], 0
    sete    dl
    call    EnableWindow
    mov     rcx, [rsp+96]
    mov     edx, 5
    call    ShowWindow
    mov     rcx, [rsp+96]
    call    redraw
    leave_frame
.hide:
    xor     edx, edx
    call    ShowWindow
    leave_frame

update_page_text:
    enter_frame
    mov     eax, [viewCount]
    mov     [rsp+32], rax
    mov     r8d, [pageOffset]
    lea     r9d, [r8+PAGE_SIZE]
    cmp     r9d, eax
    cmova   r9d, eax
    inc     r8d
    test    eax, eax
    jnz     .format
    xor     r8d, r8d
.format:
    lea     rcx, [pageText]
    lea     rdx, [fmtPage]
    call    wsprintfA
    leave_frame

redraw:
    xor     edx, edx
    xor     r8d, r8d
    jmp     InvalidateRect

fill_box:
    lea     rax, [brushes]
    mov     r8, [rax+r8*8]
    lea     rdx, [drawingRect]
    mov     rcx, [drawingDc]
    jmp     FillRect

fill_round:
    enter_frame
    mov     [rsp+96], r8d
    mov     [rsp+100], r9d
    mov     rcx, [drawingDc]
    mov     rdx, [nullPen]
    call    SelectObject
    mov     [rsp+104], rax
    mov     eax, [rsp+96]
    lea     rdx, [brushes]
    mov     rdx, [rdx+rax*8]
    mov     rcx, [drawingDc]
    call    SelectObject
    mov     [rsp+112], rax
    mov     rcx, [drawingDc]
    mov     edx, [drawingRect]
    mov     r8d, [drawingRect+4]
    mov     r9d, [drawingRect+8]
    mov     eax, [drawingRect+12]
    mov     [rsp+32], rax
    mov     eax, [rsp+100]
    test    eax, eax
    jz      .ellipse
    mov     [rsp+40], rax
    mov     [rsp+48], rax
    call    RoundRect
    jmp     .restore
.ellipse:
    call    Ellipse
.restore:
    mov     rcx, [drawingDc]
    mov     rdx, [rsp+112]
    call    SelectObject
    mov     rcx, [drawingDc]
    mov     rdx, [rsp+104]
    call    SelectObject
    leave_frame

paint_switch:
    enter_frame
    mov     [rsp+96], ecx
    rect    0, 0, SWITCH_WIDTH, SWITCH_HEIGHT
    call    fill_box
    round_shape SHADOW, 1, 4, 79, 26, 22
    cmp     dword [rsp+96], 1
    je      .on
    cmp     dword [rsp+96], 3
    je      .default
    round_shape TRACK_OFF, 2, 5, 78, 25, 20
    round_shape TRACK_LIGHT, 4, 6, 76, 15, 9
    lea     rax, [toggleOff]
    cmp     dword [switchEnergy], 0
    je      .off_text
    lea     rax, [ecoLabel]
.off_text:
    text_pointer 32, 4, 76, 26, rax, OFF_TEXT, fontStrong, LABEL_CENTER
    mov     dword [rsp+100], 2
    jmp     .knob
.default:
    round_shape BUTTON, 2, 5, 78, 25, 20
    round_shape BUTTON_LIGHT, 4, 6, 76, 15, 9
    text_label 2, 4, 78, 26, defaultLabel, INK, fontStrong, LABEL_CENTER
    leave_frame
.on:
    round_shape ACCENT, 2, 5, 78, 25, 20
    round_shape AMBER_GLOW, 4, 6, 76, 15, 9
    lea     rax, [toggleOn]
    cmp     dword [switchEnergy], 0
    je      .on_text
    lea     rax, [maxLabel]
.on_text:
    text_pointer 4, 4, 50, 26, rax, DARK, fontStrong, LABEL_CENTER
    mov     dword [rsp+100], 49
.knob:
    mov     eax, [rsp+100]
    lea     edx, [eax+30]
    round_shape SHADOW, eax, 0, edx, 30, 0
    mov     eax, [rsp+100]
    lea     edx, [eax+29]
    inc     eax
    round_shape KNOB_OUTLINE, eax, 1, edx, 29, 0
    mov     eax, [rsp+100]
    lea     edx, [eax+27]
    add     eax, 3
    round_shape KNOB_FACE, eax, 3, edx, 27, 0
    mov     eax, [rsp+100]
    lea     edx, [eax+25]
    add     eax, 5
    round_shape KNOB_LIGHT, eax, 4, edx, 16, 0
    mov     eax, [rsp+100]
    lea     edx, [eax+21]
    add     eax, 9
    round_shape KNOB_RING, eax, 9, edx, 21, 0
    mov     eax, [rsp+100]
    lea     edx, [eax+20]
    add     eax, 10
    round_shape KNOB_HUB, eax, 10, edx, 20, 0
    leave_frame

draw_text:
    enter_frame
    mov     [rsp+96], rdx
    mov     [rsp+104], r8
    mov     rcx, [drawingDc]
    mov     rdx, r9
    call    SelectObject
    mov     [rsp+112], rax
    mov     rcx, [drawingDc]
    mov     edx, 1
    call    SetBkMode
    mov     rcx, [drawingDc]
    mov     edx, [rsp+104]
    call    SetTextColor
    mov     rcx, [drawingDc]
    mov     rdx, [rsp+96]
    mov     r8d, -1
    lea     r9, [drawingRect]
    mov     eax, [textFlags]
    mov     [rsp+32], rax
    call    DrawTextA
    mov     rcx, [drawingDc]
    mov     rdx, [rsp+112]
    call    SelectObject
    leave_frame

%macro frame_panel 4
    box     FRAME, %1, %2, %3, %4
    box     SHADOW, (%1)+2, (%2)+2, (%3)-2, (%4)-2
    box     PANEL, (%1)+4, (%2)+4, (%3)-4, (%4)-4
    box     PANEL_GLOSS, (%1)+4, (%2)+4, (%3)-4, (%2)+17
    box     ACCENT, 465, %4, 535, (%4)+5
    box     AMBER_SHADE, 469, (%4)+1, 531, (%4)+3
%endmacro

%macro frame_mark 2
    box     ACCENT, 20, %1, 25, %2
    box     ACCENT, 975, %1, 980, %2
%endmacro

%macro panel_title 6
    box     ACCENT, (%1)-7, (%2)+4, (%3)+7, (%4)-3
    box     FRAME, %1, %2, %3, %4
    box     BUTTON, (%1)+2, (%2)+2, (%3)-2, (%4)-2
    box     BUTTON_LIGHT, (%1)+2, (%2)+2, (%3)-2, ((%2)+(%4))/2
    text_label %1, %2, %3, %4, %5, INK, %6, LABEL_CENTER
%endmacro

paint_window:
    enter_frame
    box     BG, 0, 0, 1000, 690
    text_label 23, 7, 540, 29, subtitle, DIM, fontStrong, LABEL_MIDDLE
    call    paint_summary
    call    paint_grid
    call    paint_activity
    call    paint_effect
    leave_frame

%macro summary_card 3
    box     BORDER, %1, 55, (%1)+183, 100
    box     ROW, (%1)+1, 56, (%1)+182, 99
    box     CELL_GLINT, (%1)+1, 56, (%1)+182, 57
    box     ACCENT, (%1)+2, 56, (%1)+4, 99
    text_label (%1)+11, 60, (%1)+176, 75, %2, SOFT, fontSmall, LABEL_MIDDLE
    lea     rdx, [%3]
    cmp     dword [scanBusy], 0
    je      %%value
    cmp     dword [applyItem], 0
    jne     %%value
    lea     rdx, [pendingStatus]
%%value:
    text_pointer (%1)+11, 75, (%1)+176, 96, rdx, INK, fontStrong, LABEL_MIDDLE | LABEL_ELLIPSIS
%endmacro

paint_summary:
    enter_frame
    frame_panel 23, 36, 977, 114
    frame_mark 60, 70
    frame_mark 73, 83
    panel_title 429, 24, 571, 47, brandName, fontHeading
    summary_card 41, emulatorLabel, valueProfile
    summary_card 234, processLabel, valueProcess
    summary_card 427, resolutionLabel, valueResolution
    summary_card 620, memoryLabel, valueMemory
    text_label 827, 43, 955, 58, searchLabel, SOFT, fontSmall, LABEL_CENTER
    box     BORDER, 827, 57, 954, 80
    box     STRIP, 829, 59, 952, 79
    leave_frame

paint_grid:
    cmp     dword [activeTab], MISC_TAB
    jne     .list
    cmp     byte [searchText], 0
    je      paint_misc
.list:
    enter_frame
    frame_panel 23, 142, 977, 501
    frame_mark 236, 246
    frame_mark 249, 259
    frame_mark 359, 369
    frame_mark 372, 382
    lea     rdx, [scanHeading]
    cmp     dword [catalogueReady], 0
    je      .heading
    lea     rax, [tabHeadings]
    mov     ecx, [activeTab]
    mov     rdx, [rax+rcx*8]
    cmp     byte [searchText], 0
    je      .heading
    lea     rdx, [searchHeading]
.heading:
    text_pointer 35, 144, 966, 161, rdx, SOFT, fontSmall, LABEL_MIDDLE
    cmp     dword [viewCount], 0
    je      .empty
    mov     dword [rsp+96], 0
.row:
    mov     ecx, [rsp+96]
    call    feature_at_row
    test    rax, rax
    jz      .page
    mov     ecx, [rsp+96]
    call    paint_row_housing
    inc     dword [rsp+96]
    cmp     dword [rsp+96], PAGE_SIZE
    jb      .row
.page:
    text_label 442, PAGE_BUTTON_Y, 558, PAGE_BUTTON_Y+PAGE_BUTTON_H, pageText, SOFT, fontSmall, LABEL_CENTER
    leave_frame
.empty:
    cmp     dword [catalogueReady], 0
    je      .scanning
    text_label 41, 228, 959, 285, noResults, SOFT, fontRegular, LABEL_CENTER
    leave_frame
.scanning:
    text_label 41, 228, 959, 285, scanDetail, SOFT, fontRegular, LABEL_CENTER
    leave_frame

paint_row_housing:
    enter_frame
    mov     [rsp+96], ecx
    call    row_position
    mov     r8d, edx
    mov     edx, ecx
    mov     rcx, [drawingDc]
    lea     r9, [rsp+104]
    call    SetViewportOrgEx
    box     BORDER, 0, 0, ROW_WIDTH, ROW_HEIGHT
    mov     r8d, ROW
    mov     eax, [rsp+96]
    cmp     eax, [activeRow]
    jne     .surface
    mov     r8d, ROW_SELECTED
.surface:
    rect    2, 2, ROW_WIDTH-2, ROW_HEIGHT-2
    call    fill_box
    box     CELL_GLINT, 2, 1, ROW_WIDTH-2, 2
    mov     ecx, [rsp+96]
    call    feature_at_row
    mov     [rsp+112], r10d
    call    item_pending_status
    test    eax, eax
    jnz     .badge
    lea     rdx, [itemAvailability]
    cmp     byte [rdx+r10], 6
    je      .badge
    cmp     dword [activeTab], UNAVAILABLE_TAB
    jne     .restore
.badge:
    call    feature_status
    cmp     ecx, 5
    jne     .check_unknown
    lea     rdx, [failedBadge]
.check_unknown:
    cmp     ecx, 7
    jne     .badge_text
    lea     rdx, [unknownApplyBadge]
.badge_text:
    mov     [rsp+120], rdx
    box     BORDER, ROW_WIDTH-100, 3, ROW_WIDTH-5, ROW_HEIGHT-3
    text_pointer ROW_WIDTH-98, 3, ROW_WIDTH-7, 17, [rsp+120], INK, fontSmall, LABEL_CENTER | LABEL_ELLIPSIS
    mov     r10d, [rsp+112]
    call    item_pending_status
    test    eax, eax
    jnz     .restore
    mov     eax, [rsp+112]
    lea     rdx, [itemAvailability]
    movzx   eax, byte [rdx+rax]
    lea     rdx, [shortAvailability]
    mov     rdx, [rdx+rax*8]
    text_pointer ROW_WIDTH-98, 16, ROW_WIDTH-7, 30, rdx, SOFT, fontSmall, LABEL_CENTER | LABEL_ELLIPSIS
.restore:
    mov     rcx, [drawingDc]
    mov     edx, [rsp+104]
    mov     r8d, [rsp+108]
    xor     r9d, r9d
    call    SetViewportOrgEx
    leave_frame

save_view_state:
    enter_frame
    mov     eax, [activeTab]
    mov     [viewState], eax
    mov     eax, [pageOffset]
    mov     [viewState+4], eax
    mov     eax, [activeRow]
    mov     [viewState+8], eax
    mov     rcx, 0xffffffff80000001
    lea     rdx, [viewKey]
    lea     r8, [viewValue]
    mov     r9d, 3
    lea     rax, [viewState]
    mov     [rsp+32], rax
    mov     dword [rsp+40], 12
    call    RegSetKeyValueA
    leave_frame

load_view_state:
    enter_frame
    cmp     dword [viewRestored], 0
    jne     .done
    mov     dword [viewRestored], 1
    mov     rcx, 0xffffffff80000001
    lea     rdx, [viewKey]
    lea     r8, [viewValue]
    mov     r9d, 8
    mov     qword [rsp+32], 0
    lea     rax, [viewState]
    mov     [rsp+40], rax
    mov     dword [rsp+112], 12
    lea     rax, [rsp+112]
    mov     [rsp+48], rax
    call    RegGetValueA
    test    eax, eax
    jnz     .done
    mov     eax, [viewState]
    cmp     eax, MISC_TAB
    ja      .done
    lea     rdx, [categoryTotals]
    cmp     eax, UNAVAILABLE_TAB
    jae     .tab_ok
    cmp     dword [rdx+rax*4], 0
    je      .done
.tab_ok:
    mov     [activeTab], eax
    call    layout_checked_tabs
    call    rebuild_view
    mov     eax, [viewState+4]
    cmp     eax, [viewCount]
    jae     .done
    and     eax, -PAGE_SIZE
    mov     [pageOffset], eax
    mov     ecx, [viewState+8]
    and     ecx, PAGE_SIZE-1
    add     eax, ecx
    cmp     eax, [viewCount]
    jae     .done
    mov     [activeRow], ecx
.done:
    leave_frame

paint_effect:
    enter_frame
    frame_panel 23, 624, 977, 684
    box     LOG_BG, 34, 632, 966, 676
    text_label 42, 634, 160, 652, effectHeading, INK, fontStrong, LABEL_MIDDLE
    cmp     dword [activeTab], MISC_TAB
    jne     .list
    mov     eax, [miscFocus]
    lea     rdx, [spEffects]
    mov     rax, [rdx+rax*8]
    text_pointer 162, 634, 958, 674, rax, SOFT, fontSmall, LABEL_WRAP
    leave_frame
.list:
    call    selected_feature
    test    rax, rax
    jz      .none
    lea     ecx, [r10+1]
    call    effect_text
    text_pointer 162, 634, 958, 674, rax, SOFT, fontSmall, LABEL_WRAP
    leave_frame
.none:
    text_label 162, 634, 958, 674, effectHint, DIM, fontSmall, LABEL_MIDDLE
    leave_frame

paint_activity:
    enter_frame
    frame_panel 23, 520, 977, 618
    frame_mark 551, 561
    frame_mark 564, 574
    panel_title 434, 507, 566, 531, activityHeading, fontStrong
    box     LOG_BG, 34, 536, 966, 609
    box     BORDER, 388, 543, 389, 602
    text_pointer 42, 540, 379, 557, [scanMessage], LOG_TEXT, fontLog, LABEL_MIDDLE | LABEL_ELLIPSIS
    cmp     dword [scanBusy], 0
    je      .last_action
    cmp     dword [applyItem], 0
    jne     .last_action
    box     BORDER, 42, 558, 378, 562
    imul    eax, [scanProgress], 336
    xor     edx, edx
    mov     ecx, [o3_item_total]
    div     ecx
    add     eax, 42
    box     ACCENT, 42, 558, eax, 562
.last_action:
    cmp     qword [applyMessage], 0
    je      .selected
    text_pointer 42, 566, 379, 583, [applyTitle], INK, fontLog, LABEL_MIDDLE | LABEL_ELLIPSIS
    text_pointer 42, 584, 379, 606, [applyMessage], LOG_TEXT, fontLog, LABEL_WRAP
.selected:
    call    selected_feature
    test    rax, rax
    jz      .done
    mov     [rsp+96], rax
    mov     [rsp+108], r10d
    mov     rdx, [rax+Feature.title]
    text_pointer 399, 539, 761, 556, rdx, INK, fontStrong, LABEL_MIDDLE | LABEL_ELLIPSIS
    mov     rax, [rsp+96]
    mov     rdx, [rax+Feature.detail]
    text_pointer 399, 556, 955, 587, rdx, SOFT, fontSmall, LABEL_WRAP

    mov     r10d, [rsp+108]
    call    feature_status
    mov     [rsp+112], rdx
    lea     rax, [emptyText]
    cmp     ecx, 6
    je      .draw_source
    cmp     ecx, 8
    je      .draw_source
    lea     rax, [attemptSources]
    cmp     ecx, 5
    je      .draw_source
    lea     rax, [applyFailedSource]
    cmp     ecx, 7
    je      .draw_source
    lea     rax, [itemSources]
    movzx   ecx, byte [rax+r10]
    lea     rax, [sourceLabels]
    mov     rax, [rax+rcx*8]
.draw_source:
    text_pointer 764, 539, 955, 556, rax, SOFT, fontSmall, LABEL_RIGHT | LABEL_ELLIPSIS
    box     BORDER, 399, 587, 954, 588
    text_pointer 399, 589, 610, 605, [rsp+112], LOG_TEXT, fontSmall, LABEL_MIDDLE | LABEL_ELLIPSIS
    mov     r10d, [rsp+108]
    call    item_pending_status
    test    eax, eax
    jz      .cached_availability
    cmp     eax, 6
    lea     rax, [emptyText]
    lea     rdx, [pendingStatus]
    cmove   rax, rdx
    jmp     .availability
.cached_availability:
    mov     eax, [rsp+108]
    lea     rdx, [itemAvailability]
    movzx   eax, byte [rdx+rax]
    lea     rdx, [availabilityLabels]
    mov     rax, [rdx+rax*8]
.availability:
    text_pointer 613, 589, 955, 605, rax, SOFT, fontSmall, LABEL_RIGHT | LABEL_ELLIPSIS
.done:
    leave_frame

draw_button:
    enter_frame
    mov     [rsp+96], rcx
    mov     rax, [rcx+32]
    mov     [drawingDc], rax
    mov     eax, [rcx+4]
    mov     [rsp+104], eax
    mov     edx, [rcx+48]
    mov     [rsp+112], edx
    mov     edx, [rcx+52]
    mov     [rsp+116], edx
    cmp     eax, ID_LICENSE
    je      .license
    cmp     eax, ID_SPECIAL
    jae     .special
    cmp     eax, ID_FIRST_ON
    jae     .switch
    cmp     eax, ID_FIRST_ROW
    jae     .row
    cmp     eax, ID_MINIMIZE
    je      .caption
    cmp     eax, ID_CLOSE
    je      .caption
    mov     r8d, BUTTON
    lea     rdx, [refreshTitle]
    cmp     eax, ID_REFRESH
    je      .standard
    lea     rdx, [prevTitle]
    cmp     eax, ID_PREV
    je      .standard
    lea     rdx, [nextTitle]
    cmp     eax, ID_NEXT
    je      .standard
    lea     rdx, [restoreTitle]
    cmp     eax, ID_RESTORE
    je      .standard
    sub     eax, ID_EMULATOR
    lea     rdx, [tabTitles]
    mov     rdx, [rdx+rax*8]
    cmp     eax, [activeTab]
    jne     .standard
    mov     r8d, AMBER_SHADE
.standard:
    mov     [rsp+120], rdx
    mov     r9d, INK
    mov     rax, [rsp+96]
    test    dword [rax+16], 4
    jz      .pressed
    mov     r8d, ROW
    mov     r9d, SOFT
    jmp     .button_face
.pressed:
    test    dword [rax+16], 1
    jz      .button_face
    mov     r8d, BUTTON_LIGHT
.button_face:
    mov     [rsp+128], r9d
    mov     eax, [rsp+112]
    mov     edx, [rsp+116]
    rect    0, 0, eax, edx
    call    fill_box
    mov     eax, [rsp+112]
    mov     ecx, [rsp+116]
    mov     r8d, [rsp+128]
    text_pointer 0, 0, eax, ecx, [rsp+120], r8d, fontSmall, LABEL_CENTER
    jmp     .focus
.caption:
    box     FRAME, 0, 0, 26, 26
    box     SHADOW, 2, 2, 24, 24
    box     BUTTON_LIGHT, 4, 4, 22, 22
    cmp     dword [rsp+104], ID_CLOSE
    jne     .minimize_glyph
    text_label 4, 4, 22, 22, closeGlyph, INK, fontStrong, LABEL_CENTER
    jmp     .focus
.minimize_glyph:
    box     KNOB_LIGHT, 9, 18, 17, 20
    jmp     .focus
.license:
    mov     r8d, BUTTON
    lea     rdx, [licenseButtonText]
    jmp     .standard
.special:
    sub     eax, ID_SPECIAL
    mov     ecx, 2
    bt      dword [specialBits], eax
    jnc     .special_bg
    mov     ecx, 1
.special_bg:
    mov     dword [switchEnergy], 0
    mov     r8d, ROW
    cmp     eax, [miscFocus]
    jne     .paint_switch
    mov     r8d, ROW_SELECTED
    jmp     .paint_switch
.switch:
    mov     ecx, eax
    sub     ecx, ID_FIRST_ON
    and     ecx, PAGE_SIZE-1
    mov     [rsp+108], ecx
    call    feature_at_row
    test    rax, rax
    jz      .handled
    call    item_pending_status
    test    eax, eax
    jnz     .handled
    lea     rax, [itemStates]
    movzx   ecx, byte [rax+r10]
    lea     rax, [o3_defaults]
    cmp     byte [rax+r10], 0
    je      .switch_state
    mov     ecx, 3
.switch_state:
    xor     edx, edx
    mov     eax, r10d
    sub     eax, [catalogue_starts+ENERGY_TAB*4]
    cmp     eax, [catalogue_counts+ENERGY_TAB*4]
    setb    dl
    mov     [switchEnergy], edx
    mov     r8d, ROW
    mov     eax, [rsp+108]
    cmp     eax, [activeRow]
    jne     .paint_switch
    mov     r8d, ROW_SELECTED
.paint_switch:
    call    paint_switch
    jmp     .focus
.row:
    sub     eax, ID_FIRST_ROW
    mov     [rsp+104], eax
    mov     ecx, eax
    call    feature_at_row
    test    rax, rax
    jz      .handled
    mov     [rsp+120], rax
    mov     r8d, ROW
    mov     eax, [rsp+104]
    cmp     eax, [activeRow]
    jne     .row_background
    mov     r8d, ROW_SELECTED
.row_background:
    mov     rax, [rsp+96]
    test    dword [rax+16], 1
    jz      .row_fill
    mov     r8d, BUTTON
.row_fill:
    rect    0, 0, ROW_BODY_WIDTH, ROW_HEIGHT-4
    call    fill_box
    mov     eax, [rsp+104]
    cmp     eax, [activeRow]
    jne     .row_title
    box     ACCENT, 0, 0, 2, ROW_HEIGHT-4
.row_title:
    mov     rax, [rsp+120]
    mov     rdx, [rax+Feature.title]
    text_pointer 7, 0, ROW_BODY_WIDTH-2, 18, rdx, INK, fontStrong, LABEL_MIDDLE | LABEL_ELLIPSIS
    mov     ecx, [rsp+104]
    call    feature_at_row
    lea     ecx, [r10+1]
    call    o3_target_label
    mov     rdx, rax
    test    rdx, rdx
    jnz     .detail
    mov     rax, [rsp+120]
    mov     rdx, [rax+Feature.detail]
.detail:
    text_pointer 7, 16, ROW_BODY_WIDTH-2, 30, rdx, SOFT, fontSmall, LABEL_MIDDLE | LABEL_ELLIPSIS
.focus:
    mov     rcx, [rsp+96]
    test    dword [rcx+16], 0x10
    jz      .handled
    mov     eax, [rsp+112]
    sub     eax, 2
    mov     r8d, [rsp+116]
    sub     r8d, 2
    rect    2, 2, eax, r8d
    mov     rcx, [drawingDc]
    lea     rdx, [drawingRect]
    call    DrawFocusRect
.handled:
    mov     eax, 1
    leave_frame

%include "effects.inc"
%include "status.inc"
%include "misc.inc"

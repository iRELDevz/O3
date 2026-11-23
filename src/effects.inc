extern o3_item_kind, o3_item_key, energy_saved

section .rdata

effectFormat    db "Jika dimatikan: %s",0
savedFormat     db "200 - ECO aktif. RAM dilepas %u MB, prioritas CPU Idle + EcoQoS.",0

fxStatic   db "Klik berganti DEFAULT > ON/OFF > DEFAULT. Efek ON dan OFF dijelaskan di atas. Pulihkan backup mengembalikan nilai awal.",0
fxService  db "Jika dimatikan: service diset Disabled dan langsung dihentikan. Fitur Windows yang memakainya berhenti dan tetap OFF setelah restart, sampai kamu ON-kan lagi.",0
fxTask     db "Jika dimatikan: Windows tidak menjalankan task ini lagi, juga setelah restart. Pekerjaan latar yang dijadwalkan task ini tidak dikerjakan.",0
fxStartup  db "Jika dimatikan: program tidak ikut jalan saat login, jadi RAM dan waktu booting lebih lega. Membuka program secara manual tetap bisa dan status tetap OFF.",0
fxBack     db "Jika dimatikan: aplikasi Store ini tidak boleh jalan di latar. Notifikasi, sinkronisasi, dan update live-tile berhenti saat aplikasinya ditutup.",0
fxEnergy   db "ECO: RAM program dilepas (diulang tiap 1 menit 20 detik), prioritas memori Very Low, CPU Idle + EcoQoS. MAX: prioritas High. Prioritas CPU tersimpan di Windows dan tetap berlaku walau O3 ditutup.",0
fxStopSvc  db "Jika dimatikan: service dihentikan sekarang saja. Setelah PC restart, atau saat ada aplikasi yang memanggilnya, Windows menjalankannya lagi.",0
fxClose    db "Jika dimatikan: program ditutup sekarang dan RAM/CPU-nya langsung dilepas. Pekerjaan yang belum disimpan hilang. Klik lagi untuk membuka ulang.",0
fxPause    db "Jika dimatikan: semua proses program ini dibekukan. CPU-nya 0% dan program tidak merespons sampai kamu ON-kan lagi. Program tidak ditutup dan datanya tetap ada. Status tetap OFF walau O3 ditutup.",0
fxWinProc  db "Jika dimatikan: proses Windows/driver ini ditutup. Fitur yang memakainya berhenti sampai Windows menjalankannya lagi atau sampai PC restart.",0

effectKinds: dq fxStatic, fxService, fxTask, fxStartup, fxBack, fxEnergy, fxStopSvc, fxClose, fxWinProc, fxPause
EFFECT_KINDS equ 10

kSysMain   db "SysMain",0
xSysMain   db "Windows berhenti memuat aplikasi yang sering dipakai ke RAM lebih dulu. RAM lebih lega, tapi aplikasi bisa sedikit lebih lambat dibuka.",0
kWSearch   db "WSearch",0
xWSearch   db "pencarian di Start dan File Explorer jadi lambat karena indeks file tidak diperbarui. Disk dan CPU lebih tenang.",0
kDiagTrack db "DiagTrack",0
xDiagTrack db "data diagnostik/telemetri tidak dikirim ke Microsoft. Tidak ada fitur harian yang hilang.",0
kSpooler   db "Spooler",0
xSpooler   db "semua printer dan Print to PDF tidak bisa dipakai sampai service ini ON lagi.",0
kXblAuth   db "XblAuthManager",0
xXblAuth   db "login Xbox Live di game PC/Game Pass gagal. Game yang tidak memakai akun Xbox tidak terpengaruh.",0
kXblSave   db "XblGameSave",0
xXblSave   db "save game Xbox tidak disinkronkan ke cloud.",0
kXboxNet   db "XboxNetApiSvc",0
xXboxNet   db "fitur multiplayer Xbox Live di PC bisa gagal terhubung.",0
kXboxGip   db "XboxGipSvc",0
xXboxGip   db "update firmware dan setelan aksesori Xbox (stik) tidak jalan. Stik tetap bisa dipakai.",0
kMaps      db "MapsBroker",0
xMaps      db "peta offline di aplikasi Maps tidak diunduh/diperbarui.",0
kFax       db "Fax",0
xFax       db "fitur kirim/terima fax tidak bisa dipakai.",0
kWer       db "WerSvc",0
xWer       db "laporan crash aplikasi tidak dibuat dan tidak dikirim.",0
kRetail    db "RetailDemo",0
xRetail    db "mode demo toko tidak tersedia. Aman untuk PC pribadi.",0
kPhoneSvc  db "PhoneSvc",0
xPhoneSvc  db "fitur panggilan telepon lewat PC tidak jalan.",0
kBth       db "bthserv",0
xBth       db "perangkat Bluetooth (headset, mouse, stik) tidak bisa tersambung.",0
kLfsvc     db "lfsvc",0
xLfsvc     db "aplikasi tidak bisa membaca lokasi perangkat.",0
kWbio      db "WbioSrvc",0
xWbio      db "login sidik jari/wajah (Windows Hello) tidak bisa dipakai.",0
kStisvc    db "stisvc",0
xStisvc    db "scanner dan sebagian kamera tidak terdeteksi.",0
kTrkWks    db "TrkWks",0
xTrkWks    db "shortcut ke file yang dipindah di jaringan tidak diperbaiki otomatis.",0
kDPS       db "DPS",0
xDPS       db "troubleshooter dan diagnosa masalah Windows tidak bisa berjalan.",0
kPcaSvc    db "PcaSvc",0
xPcaSvc    db "Windows tidak lagi memperingatkan masalah kompatibilitas program lama.",0
kCDPSvc    db "CDPSvc",0
xCDPSvc    db "Phone Link, Nearby Sharing, dan sinkron antar perangkat tidak jalan.",0
kEdgeUpd   db "edgeupdate",0
xEdgeUpd   db "Microsoft Edge tidak update otomatis.",0
kEdgeUpdM  db "edgeupdatem",0
xEdgeUpdM  db "Microsoft Edge tidak update otomatis.",0
kGupdate   db "gupdate",0
xGupdate   db "Google Chrome tidak update otomatis.",0
kGupdateM  db "gupdatem",0
xGupdateM  db "Google Chrome tidak update otomatis.",0
kNvSvc     db "NVDisplay.ContainerLocalSystem",0
xNvSvc     db "NVIDIA Control Panel dan overlay tidak bisa dibuka. Driver dan game tetap jalan.",0
kSteamSvc  db "Steam Client Service",0
xSteamSvc  db "Steam meminta izin admin saat install/update game.",0
kWisvc     db "wisvc",0
xWisvc     db "Windows Insider tidak bisa dipakai. Aman bila tidak ikut Insider.",0

kMsedge    db "msedge.exe",0
xMsedge    db "semua jendela dan tab Microsoft Edge ditutup, termasuk proses latar Startup Boost.",0
kWebview   db "msedgewebview2.exe",0
xWebview   db "aplikasi yang memakai WebView2 (Widgets, Teams, Search) error. Windows biasanya langsung menjalankannya lagi.",0
kDiscord   db "Discord.exe",0
xDiscord   db "Discord tertutup, voice call dan notifikasi berhenti.",0
kOneDrive  db "OneDrive.exe",0
xOneDrive  db "sinkron file OneDrive berhenti sampai OneDrive dibuka lagi.",0
kTeams     db "ms-teams.exe",0
xTeams     db "Microsoft Teams tertutup, panggilan dan chat berhenti.",0
kPhoneExp  db "PhoneExperienceHost.exe",0
xPhoneExp  db "Phone Link (notifikasi HP di PC) berhenti.",0
kWidgets   db "Widgets.exe",0
xWidgets   db "panel Widgets (cuaca, berita) tertutup.",0
kSteamWeb  db "steamwebhelper.exe",0
xSteamWeb  db "jendela Steam jadi kosong/error sampai Steam dibuka ulang.",0
kSteam     db "steam.exe",0
xSteam     db "Steam tertutup. Game Steam yang sedang jalan bisa ikut tertutup.",0
kNvCont    db "NVDisplay.Container.exe",0
xNvCont    db "NVIDIA Control Panel dan overlay berhenti. Driver dan game tetap jalan.",0
kNvShare   db "nvcontainer.exe",0
xNvShare   db "overlay, rekam, dan update driver NVIDIA berhenti.",0
kRealtek   db "RtkAudUService64.exe",0
xRealtek   db "panel audio Realtek dan efek suaranya berhenti. Suara biasa tetap keluar.",0
kSearchH   db "SearchHost.exe",0
xSearchH   db "kotak Search di taskbar tertutup. Windows menjalankannya lagi saat diklik.",0
kBluestack db "HD-Player.exe",0
xBluestack db "emulator tertutup dan game di dalamnya ikut berhenti tanpa save.",0
kChrome    db "chrome.exe",0
xChrome    db "semua jendela dan tab Chrome ditutup.",0
kSpotify   db "Spotify.exe",0
xSpotify   db "musik Spotify berhenti.",0
kEpic      db "EpicGamesLauncher.exe",0
xEpic      db "Epic Games Launcher tertutup, download yang berjalan dijeda.",0

effectTable:
    dq kSysMain, xSysMain, kWSearch, xWSearch, kDiagTrack, xDiagTrack, kSpooler, xSpooler
    dq kXblAuth, xXblAuth, kXblSave, xXblSave, kXboxNet, xXboxNet, kXboxGip, xXboxGip
    dq kMaps, xMaps, kFax, xFax, kWer, xWer, kRetail, xRetail
    dq kPhoneSvc, xPhoneSvc, kBth, xBth, kLfsvc, xLfsvc, kWbio, xWbio
    dq kStisvc, xStisvc, kTrkWks, xTrkWks, kDPS, xDPS, kPcaSvc, xPcaSvc
    dq kCDPSvc, xCDPSvc, kEdgeUpd, xEdgeUpd, kEdgeUpdM, xEdgeUpdM, kGupdate, xGupdate
    dq kGupdateM, xGupdateM, kNvSvc, xNvSvc, kSteamSvc, xSteamSvc, kWisvc, xWisvc
    dq kMsedge, xMsedge, kWebview, xWebview, kDiscord, xDiscord, kOneDrive, xOneDrive
    dq kTeams, xTeams, kPhoneExp, xPhoneExp, kWidgets, xWidgets, kSteamWeb, xSteamWeb
    dq kSteam, xSteam, kNvCont, xNvCont, kNvShare, xNvShare, kRealtek, xRealtek
    dq kSearchH, xSearchH, kBluestack, xBluestack, kChrome, xChrome, kSpotify, xSpotify
    dq kEpic, xEpic
EFFECT_COUNT equ ($-effectTable)/16

section .bss
effectBuffer    resb 600
savedBuffer     resb 128

section .text

energy_saved_note:
    enter_frame
    cmp     dword [applyResult], 1
    jne     .done
    mov     r8, [energy_saved]
    test    r8, r8
    jz      .done
    shr     r8, 20
    mov     qword [energy_saved], 0
    lea     rdx, [savedFormat]
    lea     rcx, [savedBuffer]
    call    wsprintfA
    lea     rax, [savedBuffer]
    mov     [applyMessage], rax
.done:
    leave_frame

effect_text:
    enter_frame
    mov     [rsp+96], ecx
    call    o3_item_kind
    cmp     eax, EFFECT_KINDS
    jb      .kind_ok
    xor     eax, eax
.kind_ok:
    mov     [rsp+100], eax
    test    eax, eax
    jz      .general
    cmp     eax, 5
    je      .general
    cmp     eax, 9
    je      .general
    mov     ecx, [rsp+96]
    call    o3_item_key
    mov     [rsp+104], rax
    mov     dword [rsp+112], 0
.find:
    mov     eax, [rsp+112]
    cmp     eax, EFFECT_COUNT
    jae     .general
    shl     eax, 4
    lea     rdx, [effectTable]
    mov     rdx, [rdx+rax]
    mov     rcx, [rsp+104]
    call    lstrcmpiA
    test    eax, eax
    jz      .found
    inc     dword [rsp+112]
    jmp     .find
.found:
    mov     eax, [rsp+112]
    shl     eax, 4
    lea     rdx, [effectTable]
    mov     r8, [rdx+rax+8]
    lea     rdx, [effectFormat]
    lea     rcx, [effectBuffer]
    call    wsprintfA
    lea     rax, [effectBuffer]
    leave_frame
.general:
    mov     eax, [rsp+100]
    lea     rdx, [effectKinds]
    mov     rax, [rdx+rax*8]
    leave_frame

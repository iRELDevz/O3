# OffOrOn (O3)

O3 adalah aplikasi Windows x64 untuk melihat setelan Free Fire, MSI App Player/BlueStacks, dan Windows. Katalog memuat 112 fitur dengan status, deskripsi efek, ketersediaan, serta tombol ON dan OFF.

## Build

Pasang NASM dan GCC dari MinGW-w64, lalu pastikan nasm.exe dan gcc.exe tersedia di PATH. Jalankan build.cmd dan buka build\OffOrOn.exe. Jika memindahkan hasil build, sertakan folder build\scan di samping executable.

## Cara scan bekerja

Saat O3 dibuka, pemindaian berlangsung di latar agar katalog tetap bisa dipakai. O3 membaca registry, API Windows, dan konfigurasi emulator secara langsung. PowerShell lalu CMD hanya dijalankan untuk item manifest yang gagal dibaca langsung, dan keduanya memakai path lengkap ke skrip di folder scan. Pindai ulang mengulangi pemindaian sesuai permintaan pemain.

O3 memeriksa ketersediaan satu per satu dan menampilkan progres langkahnya. Hasil pemindaian menentukan isi tab utama. Fitur yang dapat diubah muncul pada tab kategorinya. Fitur baca-saja, fitur yang belum didukung, dan fitur dengan status gagal diverifikasi masuk ke tab Unavailable. Pencarian mengikuti sisi katalog yang sedang dipilih.

| Status | Arti |
| --- | --- |
| ON / OFF | Nilai berhasil dibaca dari sumber yang ditampilkan. |
| Belum diatur | Nilai eksplisit tidak tersimpan; Windows menentukan perilaku bawaan. |
| Belum terbaca | Sumber status otomatis belum tersedia untuk fitur ini. |
| Tidak tersedia | Prasyarat fitur tidak ditemukan pada perangkat ini. |
| 55012 | Pembacaan langsung, PowerShell, dan CMD tidak menghasilkan status yang valid. |

## Kontrol ON dan OFF

O3 dapat mengubah setelan Windows, Edge/Chrome, konfigurasi emulator, dan indikator sentuhan Android. Jumlah yang tersedia bergantung pada perangkat. Pemain memilih ON atau OFF pada satu fitur, lalu O3 membaca ulang hasilnya. Setelan browser dan emulator hanya bisa diubah saat aplikasinya tertutup, termasuk proses latar dan Startup Boost.

Tombol nonaktif ketika fitur tampil sebagai Status saja, Belum didukung O3, Tidak tersedia, atau statusnya belum dapat diverifikasi. Pengaturan emulator, resolusi, dan DPI dibaca sebagai informasi.

## Tab CPU dan RAM

Saat scan, O3 mendata isi PC: service Windows (Otomatis/Manual), scheduled task, program startup, dan izin berjalan di latar untuk aplikasi Store. Tab CPU berisi service dengan pemakaian CPU rata-rata 1% atau lebih, service yang tidak berjalan tetapi bisa dipicu, scheduled task, dan aplikasi latar. Tab RAM berisi service yang sedang berjalan dan program startup, diurutkan dari pemakaian RAM terbesar.

O3 meminta izin administrator (UAC) setiap kali dibuka karena service dan scheduled task hanya bisa diubah oleh admin. Service yang menjaga Windows tetap bisa menyala, login, aman, dan online tidak ditampilkan, misalnya RPC, Defender, Firewall, Windows Update inti, dan Plug and Play.

DEFAULT pada item CPU/RAM adalah nilai yang O3 lihat saat pertama kali menemukan item tersebut. Nilai itu disimpan di `cpu-ram-baseline.txt`. Untuk service, OFF berarti Disabled dan service langsung dihentikan.

Setiap perubahan menampilkan kode hasil: 200 jika berhasil, 55012 jika gagal. Item yang gagal diubah pindah ke tab Unavailable sampai Pindai ulang.

## Energy Saver

Tab Energy Saver berisi semua program yang sedang berjalan di akun kamu, diurutkan dari pemakaian RAM terbesar. Bagian inti Windows seperti svchost, csrss, dwm, explorer, dan Defender tidak ditampilkan.

Setiap program punya tiga pilihan, berganti setiap klik: DEFAULT, ECO, MAX, lalu kembali ke DEFAULT.

- ECO menyalakan Efficiency mode Windows (EcoQoS) dan menurunkan prioritas ke Idle. ECO dilepas sementara saat jendela program itu sedang dipakai di depan.
- MAX mematikan EcoQoS dan menaikkan prioritas ke High.
- DEFAULT mengembalikan pengaturan ke Windows.

Pilihan disimpan per nama program di `energy-choices.txt`. Pilihan itu diterapkan lagi setiap kali program dibuka, selama O3 berjalan. Saat O3 ditutup, semua program kembali normal.

O3 menjaga dirinya tetap ringan. O3 berjalan dengan EcoQoS dan prioritas di bawah normal, mengembalikan memorinya setelah scan, dan saat diminimalkan hanya menerapkan pilihan Energy Saver tanpa membaca ulang semua setelan.
## Backup dan pemulihan

Folder `DO NOT DELETE THIS backupfirst` di samping EXE menyimpan dua file. Jika folder EXE tidak bisa ditulisi, kedua file disimpan di `%LOCALAPPDATA%\OffOrOn`.

- `first-run.json` adalah potret saat O3 pertama kali dibuka. File ini tidak pernah ditimpa.
- `added-items.json` mencatat nilai awal fitur yang baru bisa diubah setelah potret pertama, misalnya karena O3 diperbarui atau aplikasi baru dipasang. Setiap fitur dicatat sekali, sebelum O3 mengubahnya untuk pertama kali.

Jika `first-run.json` hilang, O3 membuatnya ulang dari setelan saat itu dan menandainya `"recreated":true`. ON/OFF hanya dikunci selama backup gagal disimpan.

Tombol Pulihkan backup mengembalikan setiap fitur yang tercatat ke nilai awalnya. Nilai registry dikembalikan persis, termasuk menghapus nilai yang semula tidak ada. Setelan per aplikasi (26, 33-37, 58, 88) tidak ikut dipulihkan. Item CPU/RAM dikembalikan ke nilai di `cpu-ram-baseline.txt`.

## Struktur sumber

- src/main.asm memuat jendela dan navigasi Win32.
- src/catalogue.inc memuat teks 112 fitur.
- src/status.inc memuat pembacaan ringkasan perangkat dan profil emulator.
- src/probes.c mengelola pembaca status, batas waktu, dan fallback.
- src/backup.inc menyimpan backup dan menjalankan pemulihan.
- src/system_items.inc memindai dan mengubah service, scheduled task, startup, dan aplikasi latar untuk tab CPU dan RAM.
- src/o3.manifest meminta izin administrator.
- scan/probes.txt mendefinisikan 37 pemeriksaan otomatis. probe.ps1 dan probe.cmd menjalankan pembaca shell.

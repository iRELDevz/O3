# O3 (OffOrOn)

O3 is a Windows optimizer built to be cheap to run and flexible. It doesn't ship
with a fixed list of tweaks that assumes every PC is the same. Instead it looks at
what is actually running on your Windows install and finds the things that can be
turned off or closed for a while: services, scheduled tasks, startup apps,
background apps, and programs eating RAM or CPU right now.

It started as a tool for Free Fire on MSI App Player / BlueStacks, so there are
also switches for the emulator, the browser and a handful of Windows settings.

The UI is written in x64 assembly (NASM) and the rest is C with no C runtime, so
the exe is around 220 KB and O3 itself sits at roughly 19 MB of RAM. While it is
open it runs itself in Windows efficiency mode.

## Download

Grab `OffOrOn-v1.zip` from [Releases](https://github.com/iRELDevz/O3/releases),
unzip it and run `OffOrOn.exe`. Keep the `scan` folder next to the exe.

O3 asks for admin every time it starts, because services and scheduled tasks
can't be changed without it. The exe isn't signed yet, so SmartScreen will
probably complain. Click "More info" and then "Run anyway".

## How it works

When O3 opens it scans the PC in the background. Settings are read directly from
the registry, Windows APIs and the emulator config. The scripts in `scan/` are only
used as a fallback for anything that couldn't be read directly.

Every switch cycles through three states: DEFAULT, then ON or OFF, then back to
DEFAULT. DEFAULT is whatever the value was the first time O3 saw it.

After each change O3 reads the value back and shows a result code:

- `200` the change went through
- `55012` it didn't

Anything that fails, or that Windows keeps restarting on its own, moves to the
Unavailable tab until the next rescan.

### CPU and RAM tabs

These are built from your PC, not from a list. You'll see services, scheduled
tasks, startup apps and Store apps allowed to run in the background, each with its
current RAM and CPU usage. The numbers update live, so you can watch memory drop
after you turn something off.

About 60 services that keep Windows booting, signed in, secure or online are never
listed (RPC, Defender, Firewall, core Windows Update, Plug and Play and so on).

Turning a service OFF sets it to Disabled and stops it right away. Turning a
startup app OFF also offers to close the copy that is already running. If you
open an app yourself later, O3 notices and shows it as ON again.

### Energy Saver tab

Lists every program running under your account. Each one can be set to:

- **ECO**: Windows efficiency mode (EcoQoS) plus idle priority. Lifted while the
  window is in front.
- **MAX**: no throttling, high priority.
- **DEFAULT**: left to Windows.

Choices are remembered per program name and applied again whenever that program
starts, as long as O3 is open. Closing O3 puts everything back to normal.

## Backup and restore

The first time it runs, O3 creates `DO NOT DELETE THIS backupfirst` next to the exe
(or `%LOCALAPPDATA%\OffOrOn` if that folder isn't writable).

- `first-run.json` is a snapshot of your settings from the first launch. It is
  never overwritten.
- `added-items.json` records the original value of anything that became
  changeable later, before O3 touches it.
- `cpu-ram-baseline.txt` holds the defaults for the CPU and RAM items.

The **Pulihkan backup** (restore) button puts everything recorded there back the
way it was. If the backup can't be written, O3 won't let you change anything.

## Building

You need NASM and MinGW-w64 (GCC, windres) on your PATH. Then:

```
build.cmd
```

The result is `build\OffOrOn.exe` with `build\scan` next to it.

## Source layout

| File | What's in it |
| --- | --- |
| `src/main.asm` | Window, tabs, paging, search, buttons |
| `src/catalogue.inc` | Text for the built-in items |
| `src/status.inc` | Device summary and emulator profile |
| `src/probes.c` | Reading and changing settings, timeouts, fallbacks |
| `src/system_items.inc` | CPU, RAM and Energy Saver scanning |
| `src/backup.inc` | Backup files and restore |
| `src/windows_settings.cpp` | Helper for settings stored outside the registry |
| `src/audio_session.cpp` | Audio session reads |
| `scan/` | Fallback readers (`probe.ps1`, `probe.cmd`) and their item list |

## License

MIT with one change: you can't sell O3 or anything built mainly on it unless the
repo owner says so in writing, or you're a contributor whose PR got merged. See
[LICENSE](LICENSE) for the exact wording.

`src/vendor/jsmn.h` is by Serge Zaitsev under the plain MIT license.

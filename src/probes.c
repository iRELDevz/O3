#define _WIN32_WINNT 0x0A00
#include <winsock2.h>
#include <windows.h>
#include <shlobj.h>

#ifndef SPI_GETMOUSEWHEELROUTING
#define SPI_GETMOUSEWHEELROUTING 0x201C
#define SPI_SETMOUSEWHEELROUTING 0x201D
#endif
#ifndef MOUSEWHEEL_ROUTING_MOUSE_POS
#define MOUSEWHEEL_ROUTING_FOCUS 0
#define MOUSEWHEEL_ROUTING_MOUSE_POS 2
#endif

enum { item_count = 112, dynamic_limit = 1200, probe_limit = 64, output_limit = 32768, config_limit = 524288 };

unsigned o3_item_total = item_count;
enum { manual, on, off, inherited, unavailable, failed, pending, apply_unknown };
enum { no_toggle, live_toggle, saved_toggle, unsupported, unreadable, read_only, choose_target };
enum { change_failed, changed_live, changed_saved, no_change, cannot_change, browser_busy, emulator_busy, record_failed, cancelled, respawned };

typedef struct {
    unsigned id;
    char kind;
    char subkey[160];
    char name[64];
    DWORD on_value;
    DWORD alternate;
    DWORD off_value;
    BOOL has_alternate;
} Probe;

static Probe probes[probe_limit];
static unsigned probe_count;
static char output[output_limit];
static char config_text[config_limit + 1];
static wchar_t workdir[1024];
static wchar_t executable[1024];
static wchar_t command[2304];

BOOL o3_first_instance(void) {

    static HANDLE guard;
    guard=CreateMutexW(NULL,FALSE,L"Local\\OffOrOn.O3.SingleInstance");
    if (!guard || GetLastError()==ERROR_ALREADY_EXISTS) return FALSE;
    return !FindWindowW(L"OffOrOnWindow",NULL);
}

static BOOL number(const char *p, DWORD *value) {
    DWORD v = 0;
    unsigned base = 10;
    BOOL any = FALSE;
    if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) {
        base = 16;
        p += 2;
    }
    for (; *p; ++p) {
        unsigned d;
        if (*p >= '0' && *p <= '9') d = (unsigned)(*p - '0');
        else if (*p >= 'a' && *p <= 'f') d = (unsigned)(*p - 'a' + 10);
        else if (*p >= 'A' && *p <= 'F') d = (unsigned)(*p - 'A' + 10);
        else return FALSE;
        if (d >= base || v > (0xffffffffUL - d) / base) return FALSE;
        v = v * base + d;
        any = TRUE;
    }
    if (any) *value = v;
    return any;
}

static char *token(char **cursor, char divider) {
    char *start = *cursor;
    char *p = start;
    while (*p && *p != divider) ++p;
    if (*p) {
        *p = 0;
        *cursor = p + 1;
    } else *cursor = p;
    return start;
}

static void trim_line(char *p) {
    int n = lstrlenA(p);
    while (n > 0 && (p[n - 1] == '\r' || p[n - 1] == ' ' || p[n - 1] == '\t')) p[--n] = 0;
}

static void load_probes(const char *manifest) {
    probe_count = 0;
    while (*manifest && probe_count < probe_limit) {
        char line[256];
        char *cursor = line;
        unsigned len = 0;
        while (*manifest && *manifest != '\n') {
            if (len + 1 < sizeof line) line[len++] = *manifest;
            ++manifest;
        }
        if (*manifest) ++manifest;
        line[len] = 0;
        trim_line(line);
        DWORD id;
        if (!number(token(&cursor, '|'), &id) || !id || id > item_count) continue;
        char *kind = token(&cursor, '|');
        char *path = token(&cursor, '|');
        char *name = token(&cursor, '|');
        char *expected_on = token(&cursor, '|');
        char *alternate = expected_on;
        expected_on = token(&alternate, ',');
        Probe *p = &probes[probe_count];
        if (lstrlenA(kind) != 1 || lstrlenA(path) >= (int)sizeof p->subkey ||
            lstrlenA(name) >= (int)sizeof p->name ||
            !number(expected_on, &p->on_value) || !number(cursor, &p->off_value)) continue;
        p->has_alternate = number(alternate, &p->alternate) && alternate != expected_on;
        p->id = id;
        p->kind = kind[0];
        lstrcpyA(p->subkey, path);
        lstrcpyA(p->name, name);
        ++probe_count;
    }
}

static unsigned interpret(const Probe *p, DWORD value) {
    if (p->kind == 'B') return (value & p->on_value) ? on : off;
    if (value == p->on_value || (p->has_alternate && value == p->alternate)) return on;
    return value == p->off_value ? off : failed;
}

static BOOL quoted_bit(const char *line, DWORD *value) {
    while (*line && *line != '=') ++line;
    if (*line++ != '=' || *line++ != '"') return FALSE;
    if (*line != '1' && *line != '0') return FALSE;
    *value = (DWORD)(*line++ - '0');
    if (*line++ != '"') return FALSE;
    while (*line == ' ' || *line == '\t' || *line == '\r') ++line;
    return *line == 0 || *line == '\n';
}

static BOOL prefix(const char *s, const char *start) {
    while (*start) if (*start++ != *s++) return FALSE;
    return TRUE;
}

static const Probe *by_id(unsigned id) {
    for (unsigned i = 0; i < probe_count; ++i)
        if (probes[i].id == id) return &probes[i];
    return 0;
}

static void parse_output(unsigned phase, unsigned char *values, unsigned char *sources) {
    unsigned current = 0;
    unsigned char config_results[item_count] = {0};
    char *cursor = output;
    while (*cursor) {
        char *line = token(&cursor, '\n');
        trim_line(line);
        while (*line == ' ' || *line == '\t') ++line;
        if (phase == 0) {
            DWORD id, value;
            char *lhs = token(&line, '=');
            if (number(lhs, &id) && by_id(id) && number(line, &value) && value <= 1 &&
                values[id - 1] == failed) {
                values[id - 1] = value ? on : off;
                sources[id - 1] = 1;
            }
            continue;
        }
        if (*line == '@') {
            DWORD id;
            current = number(line + 1, &id) && by_id(id) ? id : 0;
            continue;
        }
        if (!current || values[current - 1] != failed) continue;
        const Probe *p = by_id(current);
        DWORD value;
        if (p->kind == 'G' || p->kind == 'I') {
            if (!prefix(line, "bst.")) continue;
            unsigned char *candidate = &config_results[current - 1];
            *candidate = *candidate == 0 && quoted_bit(line, &value)
                ? (unsigned char)interpret(p, value) : failed;
            continue;
        }
        char *name = line;
        while (*line && *line != ' ' && *line != '\t') ++line;
        if (!*line) continue;
        *line++ = 0;
        while (*line == ' ' || *line == '\t') ++line;
        if (lstrcmpiA(name, p->name)) continue;
        char *type = line;
        while (*line && *line != ' ' && *line != '\t') ++line;
        if (!*line) continue;
        *line++ = 0;
        while (*line == ' ' || *line == '\t') ++line;
        if (lstrcmpiA(type, p->kind == 'D' ? "REG_DWORD" : "REG_SZ")) continue;
        if (number(line, &value)) {
            unsigned state = interpret(p, value);
            if (state == on || state == off) {
                values[current - 1] = (unsigned char)state;
                sources[current - 1] = 2;
            }
        }
    }
    for (unsigned i = 0; i < item_count; ++i) {
        if (config_results[i] == on || config_results[i] == off) {
            values[i] = config_results[i];
            sources[i] = 2;
        }
    }
}

static BOOL probe_directory(void) {
    DWORD n = GetModuleFileNameW(0, workdir, ARRAYSIZE(workdir));
    if (!n || n >= ARRAYSIZE(workdir)) return FALSE;
    while (n && workdir[n - 1] != L'\\') --n;
    if (!n || n + 5 >= ARRAYSIZE(workdir)) return FALSE;
    lstrcpyW(workdir + n, L"scan");
    return TRUE;
}

static BOOL run_command(unsigned phase) {
    DWORD n = GetSystemDirectoryW(executable, ARRAYSIZE(executable));
    if (!n || n >= ARRAYSIZE(executable) - 64) return FALSE;
    lstrcatW(executable, phase ? L"\\cmd.exe" : L"\\WindowsPowerShell\\v1.0\\powershell.exe");

    if (lstrlenW(executable) + lstrlenW(workdir) + 128 >= (int)ARRAYSIZE(command)) return FALSE;
    lstrcpyW(command, L"\"");
    lstrcatW(command, executable);
    lstrcatW(command, phase ? L"\" /d /q /c \"\""
                            : L"\" -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File \"");
    lstrcatW(command, workdir);
    lstrcatW(command, phase ? L"\\probe.cmd\"\"" : L"\\probe.ps1\"");

    SECURITY_ATTRIBUTES sa = { sizeof(sa), NULL, TRUE };
    HANDLE reader = NULL, writer = NULL;
    if (!CreatePipe(&reader, &writer, &sa, 0)) return FALSE;
    if (!SetHandleInformation(reader, HANDLE_FLAG_INHERIT, 0)) {
        CloseHandle(reader);
        CloseHandle(writer);
        return FALSE;
    }
    HANDLE input = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                               &sa, OPEN_EXISTING, 0, 0);
    if (input == INVALID_HANDLE_VALUE) {
        CloseHandle(reader);
        CloseHandle(writer);
        return FALSE;
    }

    STARTUPINFOW start = {0};
    PROCESS_INFORMATION process = {0};
    start.cb = sizeof start;
    start.dwFlags = STARTF_USESTDHANDLES;
    start.hStdInput = input;
    start.hStdOutput = writer;
    start.hStdError = writer;
    BOOL made = CreateProcessW(executable, command, NULL, NULL, TRUE, CREATE_NO_WINDOW,
                               NULL, workdir, &start, &process);
    CloseHandle(writer);
    CloseHandle(input);
    if (!made) {
        CloseHandle(reader);
        return FALSE;
    }
    CloseHandle(process.hThread);

    ULONGLONG started = GetTickCount64();
    DWORD used = 0, available;
    char chunk[512];
    BOOL timed_out = FALSE;
    while (GetTickCount64() - started < 10000) {
        if (!PeekNamedPipe(reader, NULL, 0, NULL, &available, NULL)) break;
        if (available) {
            DWORD got = 0;
            if (!ReadFile(reader, chunk, available < sizeof chunk ? available : sizeof chunk, &got, NULL)) break;
            for (DWORD i = 0; i < got && used + 1 < sizeof output; ++i) output[used++] = chunk[i];
        } else {
            if (WaitForSingleObject(process.hProcess, 0) == WAIT_OBJECT_0) break;
            Sleep(15);
        }
    }
    ULONGLONG elapsed = GetTickCount64() - started;
    DWORD remaining = elapsed < 10000 ? 10000 - (DWORD)elapsed : 0;
    if (WaitForSingleObject(process.hProcess, remaining) == WAIT_TIMEOUT) {
        timed_out = TRUE;
        TerminateProcess(process.hProcess, 1);
        WaitForSingleObject(process.hProcess, 500);
    }
    CloseHandle(process.hProcess);
    CloseHandle(reader);
    if (used + 1 >= sizeof output)
        while (used && output[used - 1] != '\n') --used;
    output[used] = 0;
    return used != 0 && !timed_out;
}

static BOOL load_config(const char *path) {
    config_text[0] = 0;
    if (!*path) return FALSE;
    HANDLE h = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                           NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return FALSE;
    LARGE_INTEGER size;
    DWORD got = 0;
    BOOL ok = GetFileSizeEx(h, &size) && size.QuadPart > 0 && size.QuadPart <= config_limit &&
              ReadFile(h, config_text, (DWORD)size.QuadPart, &got, NULL) && got == (DWORD)size.QuadPart;
    CloseHandle(h);
    config_text[ok ? got : 0] = 0;
    return ok;
}

static BOOL missing_install(void) {
    char path[512];
    DWORD len = GetEnvironmentVariableA("ProgramData", path, 400);
    if (len >= 400) return FALSE;
    if (!len) lstrcpyA(path, "C:\\ProgramData");
    len = (DWORD)lstrlenA(path);
    lstrcatA(path, "\\BlueStacks_msi5\\bluestacks.conf");
    if (GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES) return FALSE;
    DWORD error = GetLastError();
    if (error != ERROR_FILE_NOT_FOUND && error != ERROR_PATH_NOT_FOUND) return FALSE;
    lstrcpyA(path + len, "\\BlueStacks_nxt\\bluestacks.conf");
    if (GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES) return FALSE;
    error = GetLastError();
    return error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND;
}

static unsigned config_value(const Probe *probe, const char *instance, BOOL loaded, BOOL absent) {
    if (absent) return unavailable;
    if (!loaded) return failed;
    if (probe->kind == 'I' && !*instance) return manual;
    char needle[152];
    lstrcpyA(needle, "bst.");
    if (probe->kind == 'I') {
        lstrcatA(needle, "instance.");
        lstrcatA(needle, instance);
        lstrcatA(needle, ".");
    }
    lstrcatA(needle, probe->subkey);
    lstrcatA(needle, "=");

    const char *line = config_text;
    if ((unsigned char)line[0] == 0xef && (unsigned char)line[1] == 0xbb &&
        (unsigned char)line[2] == 0xbf) line += 3;
    unsigned result = failed, matches = 0;
    while (*line) {
        if (prefix(line, needle)) {
            DWORD value;
            if (++matches > 1) return failed;
            if (quoted_bit(line, &value)) result = interpret(probe, value);
        }
        while (*line && *line != '\n') ++line;
        if (*line) ++line;
    }
    return result;
}

static unsigned registry_value(const Probe *p) {
    DWORD value, type;
    char buf[64];
    DWORD size = p->kind == 'D' ? sizeof value : sizeof buf;
    LONG rc = RegGetValueA(HKEY_CURRENT_USER, p->subkey, p->name,
                          p->kind == 'D' ? RRF_RT_REG_DWORD : RRF_RT_REG_SZ, &type,
                          p->kind == 'D' ? (void *)&value : (void *)buf, &size);
    if (rc == ERROR_FILE_NOT_FOUND || rc == ERROR_PATH_NOT_FOUND) return inherited;
    if (rc != ERROR_SUCCESS || (p->kind != 'D' && !number(buf, &value))) return failed;
    return interpret(p, value);
}

static BOOL safe_instance(const char *s) {
    unsigned len = 0;
    for (; *s; ++s) {
        if (++len > 63) return FALSE;
        if (!((*s >= 'A' && *s <= 'Z') || (*s >= 'a' && *s <= 'z') ||
              (*s >= '0' && *s <= '9') || *s == '_' || *s == '-')) return FALSE;
    }
    return TRUE;
}

static BOOL safe_path(const char *path) {
    for (; *path; ++path)
        if (*path == '"' || *path == '\r' || *path == '\n') return FALSE;
    return TRUE;
}

static BOOL older_windows(void) {
    char build[20];
    DWORD size = sizeof build, value;
    LONG rc = RegGetValueA(HKEY_LOCAL_MACHINE,
                          "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion",
                          "CurrentBuildNumber", RRF_RT_REG_SZ, NULL, build, &size);
    return rc == ERROR_SUCCESS && number(build, &value) && value < 22000;
}

typedef struct {
    unsigned id;
    const char *subkey;
    const char *name;
    DWORD enabled;
    DWORD disabled;
    BOOL windows11;
    unsigned char default_state;
} Preference;

#define EXPLORER "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer"
#define ADVANCED EXPLORER "\\Advanced"
#define CAPTURES "Software\\Microsoft\\Windows\\CurrentVersion\\GameDVR"
#define GAMEBAR "Software\\Microsoft\\GameBar"
static const Preference preferences[] = {
    {20, GAMEBAR, "AutoGameModeEnabled", 1, 0, FALSE, on},
    {21, CAPTURES, "HistoricalCaptureEnabled", 1, 0, FALSE, off},
    {22, GAMEBAR, "UseNexusForGameBarEnabled", 1, 0, FALSE, on},
    {23, CAPTURES, "AudioCaptureEnabled", 1, 0, FALSE, on},
    {25, "Software\\Microsoft\\Multimedia\\Audio", "AccessibilityMonoMixState", 1, 0, FALSE, off},
    {27, "Software\\Microsoft\\Windows\\CurrentVersion\\PushNotifications", "ToastEnabled", 1, 0, FALSE, on},
    {28, "Software\\Microsoft\\Windows\\CurrentVersion\\PushNotifications", "LockScreenToastEnabled", 1, 0, FALSE, on},
    {29, "Software\\Microsoft\\Windows\\CurrentVersion\\Notifications\\Settings", "NOC_GLOBAL_SETTING_ALLOW_CRITICAL_TOASTS_ABOVE_LOCK", 1, 0, FALSE, on},
    {30, "Software\\Microsoft\\Windows\\CurrentVersion\\Notifications\\Settings", "NOC_GLOBAL_SETTING_ALLOW_NOTIFICATION_SOUND", 1, 0, FALSE, on},
    {31, "Software\\Microsoft\\Windows\\CurrentVersion\\ContentDeliveryManager", "SubscribedContent-310093Enabled", 1, 0, FALSE, on},
    {32, "Software\\Microsoft\\Windows\\CurrentVersion\\ContentDeliveryManager", "SubscribedContent-338389Enabled", 1, 0, FALSE, on},
    {54, ADVANCED, "TaskbarSn", 1, 0, TRUE, on},
    {57, ADVANCED, "TabletPostureTaskbar", 1, 0, TRUE, on},
    {59, "Software\\Classes\\Local Settings\\Software\\Microsoft\\Windows\\CurrentVersion\\TrayNotify", "SystemTrayChevronVisibility", 1, 0, TRUE, on},
    {60, "Software\\Microsoft\\Windows\\CurrentVersion\\Start", "ShowRecentList", 1, 0, TRUE, on},
    {61, "Software\\Microsoft\\Windows\\CurrentVersion\\Start", "ShowFrequentList", 1, 0, TRUE, off},
    {65, ADVANCED, "ShowSystrayDateTimeValueName", 1, 0, TRUE, on},
    {48, ADVANCED, "ShowTaskViewButton", 1, 0, FALSE, on},
    {49, ADVANCED, "TaskbarDa", 1, 0, TRUE, on},
    {51, ADVANCED, "TaskbarBadges", 1, 0, FALSE, on},
    {52, ADVANCED, "TaskbarFlashing", 1, 0, TRUE, on},
    {53, ADVANCED, "MMTaskbarEnabled", 1, 0, FALSE, on},
    {55, ADVANCED, "TaskbarSd", 1, 0, TRUE, on},
    {56, ADVANCED, "ShowSecondsInSystemClock", 1, 0, FALSE, off},
    {62, ADVANCED, "Start_TrackDocs", 1, 0, FALSE, on},
    {63, ADVANCED, "TaskbarSearchHoverEnabled", 1, 0, TRUE, on},
    {67, "Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize", "EnableTransparency", 1, 0, FALSE, on},
    {68, "Control Panel\\Accessibility", "DynamicScrollbars", 0, 1, FALSE, off},
    {87, "Software\\Microsoft\\LiveCaptions\\UI", "ProfanityFilterOn", 1, 0, TRUE, off},
    {89, "Software\\Microsoft\\Windows NT\\CurrentVersion\\Winlogon", "RestartApps", 1, 0, FALSE, off},
    {105, ADVANCED, "HideFileExt", 0, 1, FALSE, off},
    {106, ADVANCED, "Hidden", 1, 2, FALSE, off},
    {107, ADVANCED, "UseCompactMode", 1, 0, TRUE, off},
    {110, EXPLORER, "ShowRecent", 1, 0, FALSE, on},
    {111, EXPLORER, "ShowFrequent", 1, 0, FALSE, on},
    {112, ADVANCED, "AutoCheckSelect", 1, 0, FALSE, off}
};

static const Preference *preference_for(unsigned id) {
    for (unsigned i = 0; i < ARRAYSIZE(preferences); ++i)
        if (preferences[i].id == id) return &preferences[i];
    return NULL;
}

enum { bool_param, unsigned_param, route_param, accel_param, touch_param, flag_param };
typedef struct {
    unsigned id;
    UINT get, set;
    BYTE mode;
    UINT size;
    DWORD mask;
    DWORD available_mask;
} InputSetting;

static const InputSetting inputs[] = {
    {66, SPI_GETCLIENTAREAANIMATION, SPI_SETCLIENTAREAANIMATION, bool_param, 0, 0, 0},
    {69, SPI_GETMOUSEWHEELROUTING, SPI_SETMOUSEWHEELROUTING, route_param, 0, 0, 0},
    {70, SPI_GETMOUSE, SPI_SETMOUSE, accel_param, 0, 0, 0},
    {71, SPI_GETMOUSETRAILS, SPI_SETMOUSETRAILS, unsigned_param, 0, 0, 0},
    {72, SPI_GETMOUSEVANISH, SPI_SETMOUSEVANISH, bool_param, 0, 0, 0},
    {73, SPI_GETMOUSESONAR, SPI_SETMOUSESONAR, bool_param, 0, 0, 0},
    {74, SPI_GETSNAPTODEFBUTTON, SPI_SETSNAPTODEFBUTTON, unsigned_param, 0, 0, 0},
    {75, SPI_GETMOUSECLICKLOCK, SPI_SETMOUSECLICKLOCK, bool_param, 0, 0, 0},
    {76, SPI_GETMOUSEKEYS, SPI_SETMOUSEKEYS, flag_param, sizeof(MOUSEKEYS), MKF_MOUSEKEYSON, MKF_AVAILABLE},
    {77, SPI_GETSTICKYKEYS, SPI_SETSTICKYKEYS, flag_param, sizeof(STICKYKEYS), SKF_STICKYKEYSON, SKF_AVAILABLE},
    {78, SPI_GETSTICKYKEYS, SPI_SETSTICKYKEYS, flag_param, sizeof(STICKYKEYS), SKF_HOTKEYACTIVE, SKF_AVAILABLE},
    {79, SPI_GETFILTERKEYS, SPI_SETFILTERKEYS, flag_param, sizeof(FILTERKEYS), FKF_FILTERKEYSON, FKF_AVAILABLE},
    {80, SPI_GETFILTERKEYS, SPI_SETFILTERKEYS, flag_param, sizeof(FILTERKEYS), FKF_HOTKEYACTIVE, FKF_AVAILABLE},
    {81, SPI_GETTOGGLEKEYS, SPI_SETTOGGLEKEYS, flag_param, sizeof(TOGGLEKEYS), TKF_TOGGLEKEYSON, TKF_AVAILABLE},
    {82, SPI_GETTOGGLEKEYS, SPI_SETTOGGLEKEYS, flag_param, sizeof(TOGGLEKEYS), TKF_HOTKEYACTIVE, TKF_AVAILABLE},
    {83, SPI_GETCONTACTVISUALIZATION, SPI_SETCONTACTVISUALIZATION, touch_param, 0, 0, 0},
    {84, SPI_GETCONTACTVISUALIZATION, SPI_SETCONTACTVISUALIZATION, touch_param, 0, 0, 0}
};

typedef struct {
    const char *name;
    UINT get, set;
} AnimationEffect;

static const AnimationEffect effects[] = {
    {"client_area", SPI_GETCLIENTAREAANIMATION, SPI_SETCLIENTAREAANIMATION},
    {"window_minimize_restore", SPI_GETANIMATION, SPI_SETANIMATION},
    {"menus", SPI_GETMENUANIMATION, SPI_SETMENUANIMATION},
    {"combo_boxes", SPI_GETCOMBOBOXANIMATION, SPI_SETCOMBOBOXANIMATION},
    {"tooltips", SPI_GETTOOLTIPANIMATION, SPI_SETTOOLTIPANIMATION}
};

static BOOL read_effect(const AnimationEffect *effect, BOOL *enabled) {
    if (effect->get == SPI_GETANIMATION) {
        ANIMATIONINFO info = { sizeof info, 0 };
        if (!SystemParametersInfoW(effect->get, sizeof info, &info, 0)) return FALSE;
        *enabled = info.iMinAnimate != 0;
        return TRUE;
    }
    return SystemParametersInfoW(effect->get, 0, enabled, 0);
}

static BOOL set_effect(const AnimationEffect *effect, BOOL enabled) {
    UINT flags = SPIF_UPDATEINIFILE | SPIF_SENDCHANGE;
    if (effect->set == SPI_SETANIMATION) {
        ANIMATIONINFO info = { sizeof info, enabled };
        return SystemParametersInfoW(effect->set, sizeof info, &info, flags);
    }
    return SystemParametersInfoW(effect->set, 0, (void *)(ULONG_PTR)enabled, flags);
}

static BOOL set_animations(const BOOL *before, BOOL enabled) {
    unsigned changed = 0;
    for (unsigned i = 0; i < ARRAYSIZE(effects); ++i) {
        if (!!before[i] != enabled) {
            if (!set_effect(&effects[i], enabled)) goto rollback;
            changed |= 1u << i;
        }
        BOOL actual = FALSE;
        if (!read_effect(&effects[i], &actual) || !!actual != enabled) goto rollback;
    }
    return TRUE;
rollback:
    for (unsigned i = ARRAYSIZE(effects); i-- > 0;)
        if (changed & (1u << i)) set_effect(&effects[i], !!before[i]);
    return FALSE;
}

typedef union {
    UINT value;
    int mouse[3];
    BOOL animations[ARRAYSIZE(effects)];
    MOUSEKEYS mouse_keys;
    STICKYKEYS sticky_keys;
    FILTERKEYS filter_keys;
    TOGGLEKEYS toggle_keys;
    struct { UINT size; DWORD flags; } header;
} InputValue;

static const InputSetting *input_for(unsigned id) {
    for (unsigned i = 0; i < ARRAYSIZE(inputs); ++i)
        if (inputs[i].id == id) return &inputs[i];
    return NULL;
}

static BOOL read_input(const InputSetting *setting, InputValue *value, BOOL *enabled) {
    if (setting->id == 66) {
        *enabled = FALSE;
        for (unsigned i = 0; i < ARRAYSIZE(effects); ++i) {
            if (!read_effect(&effects[i], &value->animations[i])) return FALSE;
            if (value->animations[i]) *enabled = TRUE;
        }
        return TRUE;
    }
    value->header.size = setting->size;
    if (!SystemParametersInfoW(setting->get, setting->size, value, 0)) return FALSE;
    switch (setting->mode) {
        case touch_param:
            if (!(GetSystemMetrics(SM_DIGITIZER) & (NID_INTEGRATED_TOUCH | NID_EXTERNAL_TOUCH)) ||
                value->value > CONTACTVISUALIZATION_PRESENTATIONMODE) return FALSE;
            *enabled = setting->id == 83 ? value->value != 0 : value->value == CONTACTVISUALIZATION_PRESENTATIONMODE;
            break;
        case flag_param:
            if (!(value->header.flags & setting->available_mask)) return FALSE;
            *enabled = (value->header.flags & setting->mask) != 0;
            break;
        case route_param:
            *enabled = value->value == MOUSEWHEEL_ROUTING_MOUSE_POS;
            break;
        case accel_param:
            *enabled = value->mouse[2] != 0;
            break;
        default:
            *enabled = setting->id == 71 ? value->value > 1 : value->value != 0;
            break;
    }
    return TRUE;
}

static BOOL write_input(const InputSetting *setting, InputValue *value, BOOL enabled) {
    if (setting->id == 66) return set_animations(value->animations, enabled);
    UINT ui = 0;
    void *param = NULL;
    switch (setting->mode) {
        case touch_param:
            value->value = setting->id == 83 ? (enabled ? CONTACTVISUALIZATION_ON : CONTACTVISUALIZATION_OFF)
                : (enabled ? CONTACTVISUALIZATION_PRESENTATIONMODE : CONTACTVISUALIZATION_ON);
            param = &value->value;
            break;
        case flag_param:
            ui = setting->size;
            if (enabled) value->header.flags |= setting->mask;
            else value->header.flags &= ~setting->mask;
            param = value;
            break;
        case route_param:
            value->value = enabled ? MOUSEWHEEL_ROUTING_MOUSE_POS : MOUSEWHEEL_ROUTING_FOCUS;
            param = &value->value;
            break;
        case accel_param:
            value->mouse[2] = enabled ? 1 : 0;
            param = value->mouse;
            break;
        case unsigned_param:
            ui = enabled ? (setting->id == 71 ? 5 : 1) : 0;
            break;
        default:
            param = (void *)(ULONG_PTR)enabled;
            break;
    }
    return SystemParametersInfoW(setting->set, ui, param, SPIF_UPDATEINIFILE | SPIF_SENDCHANGE);
}

static unsigned read_preference(HKEY handle, const Preference *pref) {
    DWORD value, size = sizeof value;
    LONG rc = RegGetValueA(handle, NULL, pref->name, RRF_RT_REG_DWORD, NULL, &value, &size);
    if (rc == ERROR_FILE_NOT_FOUND) return inherited;
    if (rc != ERROR_SUCCESS) return failed;
    if (value == pref->enabled) return on;
    return value == pref->disabled ? off : failed;
}

static unsigned inherited_preference(const Preference *pref, unsigned char *source) {
    SHELLFLAGSTATE flags = {0};
    switch (pref->id) {
        case 105:
            SHGetSettings(&flags, SSF_SHOWEXTENSIONS);
            return flags.fShowExtensions ? on : off;
        case 106:
            SHGetSettings(&flags, SSF_SHOWALLOBJECTS);
            return flags.fShowAllObjects ? on : off;
        case 112:
            SHGetSettings(&flags, SSF_AUTOCHECKSELECT);
            return flags.fAutoCheckSelect ? on : off;
        case 107: {
            DWORD value, size = sizeof value;
            if (RegGetValueA(HKEY_LOCAL_MACHINE, ADVANCED "\\Folder\\UseCompactMode",
                             "DefaultValue", RRF_RT_REG_DWORD, NULL, &value, &size) == ERROR_SUCCESS
                && value <= 1) {
                *source = 4;
                return value ? on : off;
            }
            break;
        }
        case 51:
        case 53: {
            const char *key = pref->id == 51 ? ADVANCED "\\TaskbarBadges" : ADVANCED "\\MMTaskbarEnabled";
            const char *suffix = pref->id == 51 ? "Badging" : "MultiMon";
            const char *prefixes[] = {"SystemSettings_DesktopTaskbar_", "SystemSettings_Taskbar_"};
            for (unsigned i = 0; i < ARRAYSIZE(prefixes); ++i) {
                char name[64], value[4];
                DWORD size = sizeof value;
                wsprintfA(name, "%s%s", prefixes[i], suffix);
                if (RegGetValueA(HKEY_CURRENT_USER, key, name, RRF_RT_REG_SZ, NULL, value, &size)
                    == ERROR_SUCCESS && size == 2 && (value[0] == '0' || value[0] == '1'))
                    return value[0] == '1' ? on : off;
            }
            break;
        }
    }

    *source = 4;
    return pref->default_state;
}

#include "emulator_settings.inc"
#include "browser_settings.inc"
#include "target_settings.inc"
#include "android_touch.inc"

static unsigned taskbar_autohide(int requested, BYTE *state, BYTE *source) {
    APPBARDATA bar = {0};
    bar.cbSize = sizeof bar;
    bar.hWnd = FindWindowW(L"Shell_TrayWnd", NULL);
    if (!bar.hWnd) return cannot_change;
    UINT flags = (UINT)SHAppBarMessage(ABM_GETSTATE, &bar);
    *source = 3;
    *state = flags & ABS_AUTOHIDE ? on : off;
    if (requested < 0 || *state == (requested ? on : off)) return no_change;
    bar.lParam = requested ? flags | ABS_AUTOHIDE : flags & ~ABS_AUTOHIDE;
    if (!SHAppBarMessage(ABM_SETSTATE, &bar)) return change_failed;
    *state = SHAppBarMessage(ABM_GETSTATE, &bar) & ABS_AUTOHIDE ? on : off;
    return *state == (requested ? on : off) ? changed_live : change_failed;
}

static BOOL uses_windows_handler(unsigned id) {
    return id == 38 || (id >= 40 && id <= 47) || id == 64 || id == 85 || id == 90 || id == 91 || id == 92;
}

static DWORD windows_handler(unsigned id,int requested) {
    WCHAR path[MAX_PATH], args[64];
    DWORD length=GetModuleFileNameW(NULL,path,ARRAYSIZE(path));
    if (!length || length>=ARRAYSIZE(path)-24) return 30;
    while (length && path[length-1]!='\\') --length;
    lstrcpyW(path+length,L"scan\\O3Windows.exe");
    wsprintfW(args,L"O3Windows.exe %u %s",id,requested<0 ? L"read" : requested ? L"on" : L"off");
    STARTUPINFOW startup={0}; startup.cb=sizeof startup;
    PROCESS_INFORMATION process={0};
    if (!CreateProcessW(path,args,NULL,NULL,FALSE,CREATE_NO_WINDOW,NULL,NULL,&startup,&process)) return 30;
    DWORD code=30;
    if (WaitForSingleObject(process.hProcess,4500)==WAIT_OBJECT_0) GetExitCodeProcess(process.hProcess,&code);
    else TerminateProcess(process.hProcess,30);
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return code;
}

static unsigned decode_handler(DWORD code,BYTE *state,BYTE *source) {
    if (code==11 || code==12 || code==21 || code==22) {
        *source=7;
        *state=code==11 || code==21 ? on : off;
        return code<20 ? live_toggle : read_only;
    }
    *source=0; *state=unavailable; return unsupported;
}

static unsigned navigation_pane(int requested,BYTE *state,BYTE *source) {
    const char *path=EXPLORER "\\Modules\\GlobalSettings\\Sizer";
    const char *name="PageSpaceControlSizer";
    DWORD layout[4]={160,1,0,1004};
    DWORD size=sizeof layout;
    LONG rc=RegGetValueA(HKEY_CURRENT_USER,path,name,RRF_RT_REG_BINARY,NULL,layout,&size);
    BOOL absent=rc==ERROR_FILE_NOT_FOUND || rc==ERROR_PATH_NOT_FOUND;
    if (absent) {layout[0]=160;layout[1]=1;layout[2]=0;layout[3]=1004;}
    else if (rc!=ERROR_SUCCESS || size!=sizeof layout || layout[1]>1) return cannot_change;
    *state=layout[1]?on:off; *source=absent?4:3;
    if (requested<0 || *state==(requested?on:off)) return no_change;
    HKEY h;
    if (RegCreateKeyExA(HKEY_CURRENT_USER,path,0,NULL,0,KEY_SET_VALUE,NULL,&h,NULL)!=ERROR_SUCCESS) return cannot_change;
    layout[1]=(DWORD)requested;
    rc=RegSetValueExA(h,name,0,REG_BINARY,(BYTE*)layout,sizeof layout);
    RegCloseKey(h);
    if (rc!=ERROR_SUCCESS || navigation_pane(-1,state,source)!=no_change || *state!=(requested?on:off)) return change_failed;
    return changed_saved;
}

static BOOL captions_installed(void) {
    WCHAR path[MAX_PATH];
    UINT len=GetSystemDirectoryW(path,ARRAYSIZE(path));
    if (!len || len>=ARRAYSIZE(path)-20) return FALSE;
    lstrcatW(path,L"\\LiveCaptions.exe");
    return GetFileAttributesW(path)!=INVALID_FILE_ATTRIBUTES;
}

static unsigned libraries_access(int requested,BYTE *state,BYTE *source) {
    const char *cls="CLSID\\{031E4825-7B94-4dc3-B131-E946B44C8DD5}";
    const char *name="System.IsPinnedToNameSpaceTree";
    DWORD value,size=sizeof value;
    if (RegGetValueA(HKEY_CLASSES_ROOT,cls,name,RRF_RT_REG_DWORD,NULL,&value,&size)!=ERROR_SUCCESS || value>1)
        return cannot_change;
    *state=value ? on : off; *source=3;
    if (requested<0 || *state==(requested?on:off)) return no_change;
    char subkey[100]="Software\\Classes\\";
    lstrcatA(subkey,cls);
    HKEY key;
    if (RegCreateKeyExA(HKEY_CURRENT_USER,subkey,0,NULL,0,KEY_SET_VALUE,NULL,&key,NULL)!=ERROR_SUCCESS) return cannot_change;
    value=(DWORD)requested;
    LONG rc=RegSetValueExA(key,name,0,REG_DWORD,(BYTE*)&value,sizeof value);
    RegCloseKey(key);
    if (rc!=ERROR_SUCCESS) return change_failed;
    SHChangeNotify(SHCNE_ASSOCCHANGED,SHCNF_IDLIST,NULL,NULL);
    if (libraries_access(-1,state,source)!=no_change || *state!=(requested?on:off)) return change_failed;
    return changed_saved;
}

static unsigned dynamic_check(unsigned id, unsigned char *state, unsigned char *source, BYTE *mark);
static unsigned dynamic_apply(unsigned id, unsigned request, unsigned char *state, unsigned char *source);
static void dynamic_scan(void);
static BOOL dynamic_refresh(unsigned char *states, unsigned char *sources, unsigned char *availability);
static BOOL dynamic_light_tick(void);
unsigned o3_energy_next(unsigned id);
void o3_self_energy(BOOL trim);

static unsigned check_option_raw(unsigned id, unsigned char *state, unsigned char *source,
                                 BOOL old_windows) {
    if (id==17) return android_touch(-1,state,source)==no_change ? live_toggle : unsupported;
    if (uses_windows_handler(id)) return decode_handler(windows_handler(id,-1),state,source);
    if (id==57 && !GetSystemMetrics(SM_TABLETPC)) { *state=unavailable; *source=0; return unsupported; }
    if (id==108) return navigation_pane(-1,state,source)==no_change ? saved_toggle : unsupported;
    if (id==87 && !captions_installed()) { *state=unavailable; *source=0; return unsupported; }
    if (id==109) return libraries_access(-1,state,source)==no_change ? saved_toggle : unsupported;
    if (id==26) {
        int actual=o3_audio_access(-1);
        *state=actual<0 ? manual : actual ? on : off; *source=actual<0 ? 0 : 3;
        return actual<0 ? choose_target : live_toggle;
    }
    int group = target_group(id);
    if (group >= 0) {
        *state = manual;
        *source = 0;
        if (!has_target_root(group)) return unsupported;
        return target_access(id,-1,state,source) == no_change ? saved_toggle : choose_target;
    }
    if (id == 50)
        return taskbar_autohide(-1, state, source) == no_change ? live_toggle : unsupported;
    if (id >= 93 && id <= 104)
        return browser_access(id, -1, state, source)==no_change ? saved_toggle : unsupported;
    const EmulatorSwitch *emu = emulator_switch(id);
    if (emu) {
        if (!emulator_setting(emu, -1, state)) { *state = unavailable; *source = 0; return unsupported; }
        *source = 5;
        return saved_toggle;
    }
    if (id == 1 || id == 2 || id == 16) return read_only;

    const InputSetting *setting = input_for(id);
    if (setting) {
        InputValue value;
        BOOL enabled;
        if (!read_input(setting, &value, &enabled)) return unreadable;
        *state = enabled ? on : off;
        *source = 3;
        return live_toggle;
    }
    const Preference *pref = preference_for(id);
    if (!pref) return *state == unavailable ? unsupported : no_toggle;
    if (pref->windows11 && old_windows) {
        *state = unavailable;
        *source = 0;
        return unsupported;
    }
    HKEY handle;
    LONG rc = RegOpenKeyExA(HKEY_CURRENT_USER, pref->subkey, 0,
                           KEY_QUERY_VALUE | KEY_SET_VALUE, &handle);
    if (id==87 && (rc==ERROR_FILE_NOT_FOUND || rc==ERROR_PATH_NOT_FOUND)) {
        *state=off; *source=4; return saved_toggle;
    }
    if (rc != ERROR_SUCCESS)
        return rc == ERROR_FILE_NOT_FOUND || rc == ERROR_PATH_NOT_FOUND ? unsupported : read_only;

    unsigned current = read_preference(handle, pref);
    RegCloseKey(handle);
    *source = 3;
    if (current == inherited) current = inherited_preference(pref, source);
    *state = (unsigned char)current;
    if (current == failed) return unreadable;
    return saved_toggle;
}

static BYTE default_for(unsigned id) {
    static const BYTE defaults_on[] = {33,34,35,37,40,41,42,43,45,46,47,66,69,70,72,78,80,82,83,91,108};
    static const BYTE defaults_off[] = {17,26,36,38,50,58,71,73,74,75,76,77,79,81,84,90,92,109};
    if (id >= 93 && id <= 104) return browser_settings[id-93].fallback;
    for (unsigned i = 0; i < ARRAYSIZE(defaults_on); ++i) if (defaults_on[i] == id) return on;
    for (unsigned i = 0; i < ARRAYSIZE(defaults_off); ++i) if (defaults_off[i] == id) return off;
    return 0;
}

static BOOL has_default(unsigned id) {
    return id > item_count || preference_for(id) || default_for(id);
}

static BOOL preference_absent(const Preference *pref) {
    LONG rc = RegGetValueA(HKEY_CURRENT_USER, pref->subkey, pref->name, RRF_RT_ANY, NULL, NULL, NULL);
    return rc == ERROR_FILE_NOT_FOUND || rc == ERROR_PATH_NOT_FOUND;
}

BYTE o3_defaults[item_count + dynamic_limit];

static unsigned check_option(unsigned id, unsigned char *state, unsigned char *source, BOOL old_windows) {
    if (id > item_count) return dynamic_check(id, state, source, &o3_defaults[id-1]);
    unsigned availability = check_option_raw(id, state, source, old_windows);
    BYTE mark = 0;
    if ((availability == live_toggle || availability == saved_toggle) && (*state == on || *state == off)) {
        const Preference *pref = preference_for(id);
        if (pref) {
            if (preference_absent(pref)) mark = *state;
        } else if (default_for(id) == *state) mark = *state;
    }
    o3_defaults[id-1] = mark;
    return availability;
}

void o3_options(unsigned char *values, unsigned char *sources, unsigned char *availability,
                HWND progress_window) {
    BOOL old_windows = older_windows();
    o3_self_energy(FALSE);
    dynamic_scan();
    for (unsigned i = item_count; i < o3_item_total; ++i) values[i] = sources[i] = 0;
    for (unsigned i = 0; i < o3_item_total; ++i) {
        availability[i] = (unsigned char)check_option(i + 1, &values[i], &sources[i], old_windows);
        if (progress_window) PostMessageA(progress_window, WM_APP + 2, i + 1, availability[i]);
    }
    o3_self_energy(TRUE);
}

BOOL o3_refresh_switches(unsigned char *states, unsigned char *sources, unsigned char *availability) {
    if (dynamic_light_tick()) return FALSE;
    BOOL changed = FALSE;
    BOOL old_windows = older_windows();
    for (unsigned i = 0; i < item_count; ++i) {
        if (i+1!=26 && target_group(i+1)<0 && i+1!=108 && i+1!=109 && i + 1 != 50 && !input_for(i + 1) && !preference_for(i + 1) && !emulator_switch(i + 1)) continue;
        unsigned char before = states[i], source = sources[i], previous = availability[i], mark = o3_defaults[i];
        availability[i] = (unsigned char)check_option(i + 1, &states[i], &sources[i], old_windows);
        if (states[i] != before || sources[i] != source || availability[i] != previous ||
            o3_defaults[i] != mark) changed = TRUE;
    }
    if (dynamic_refresh(states, sources, availability)) changed = TRUE;
    return changed;
}

static unsigned apply_setting(unsigned id, unsigned requested, unsigned char *state, unsigned char *source) {
    if (!id || id > item_count || requested > 1) return cannot_change;
    unsigned wanted = requested ? on : off;
    if (id==17) return android_touch((int)requested,state,source);
    if (uses_windows_handler(id)) {
        unsigned available=decode_handler(windows_handler(id,-1),state,source);
        if (available!=live_toggle) return cannot_change;
        if (*state==wanted) return no_change;
        return decode_handler(windows_handler(id,(int)requested),state,source)==live_toggle &&
            *state==wanted ? changed_live : change_failed;
    }
    if (id==108) return navigation_pane((int)requested,state,source);
    if (id==87 && !captions_installed()) return cannot_change;
    if (id==109) return libraries_access((int)requested,state,source);
    if (id==57 && !GetSystemMetrics(SM_TABLETPC)) return cannot_change;
    if (id==26) {
        int before=o3_audio_access(-1);
        if (before<0) return cannot_change;
        if (before==(int)requested) return no_change;
        return o3_audio_access((int)requested)==(int)requested ? changed_live : change_failed;
    }
    if (target_group(id)>=0) return target_access(id,(int)requested,state,source);
    if (id == 50) return taskbar_autohide((int)requested, state, source);
    if (id >= 93 && id <= 104) return browser_access(id, (int)requested, state, source);
    const EmulatorSwitch *emu = emulator_switch(id);
    if (emu) {
        if (!emulator_setting(emu, -1, state)) return cannot_change;
        if (*state == wanted) return no_change;
        if (process_named(L"HD-Player.exe")) return emulator_busy;
        if (!emulator_setting(emu, (int)requested, state)) return change_failed;
        *source = 5;
        return *state == wanted ? changed_saved : change_failed;
    }
    const InputSetting *setting = input_for(id);
    if (setting) {
        InputValue value;
        BOOL enabled;
        if (!read_input(setting, &value, &enabled)) return cannot_change;
        *source = 3;
        *state = enabled ? on : off;
        if (*state == wanted) {
            BOOL matches = TRUE;
            if (id == 66)
                for (unsigned i = 0; i < ARRAYSIZE(effects); ++i)
                    if (!!value.animations[i] != (BOOL)requested) matches = FALSE;
            if (matches) return no_change;
        }
        if (!write_input(setting, &value, requested)) return change_failed;
        if (!read_input(setting, &value, &enabled)) {
            *state = apply_unknown;
            return change_failed;
        }
        *state = enabled ? on : off;
        return *state == wanted ? changed_live : change_failed;
    }
    const Preference *pref = preference_for(id);
    if (!pref || (pref->windows11 && older_windows())) return cannot_change;
    HKEY handle;
    LONG rc = id==87 ? RegCreateKeyExA(HKEY_CURRENT_USER,pref->subkey,0,NULL,0,KEY_QUERY_VALUE|KEY_SET_VALUE,NULL,&handle,NULL) :
        RegOpenKeyExA(HKEY_CURRENT_USER,pref->subkey,0,KEY_QUERY_VALUE|KEY_SET_VALUE,&handle);
    if (rc!=ERROR_SUCCESS) return cannot_change;

    unsigned result = change_failed;
    unsigned current = read_preference(handle, pref);
    *state = current == failed ? apply_unknown : (unsigned char)current;
    *source = 3;
    if (current == wanted) result = no_change;
    else if (current != failed) {
        DWORD value = requested ? pref->enabled : pref->disabled;
        if (RegSetValueExA(handle, pref->name, 0, REG_DWORD, (const BYTE *)&value, sizeof value)
            == ERROR_SUCCESS) {
            unsigned saved = read_preference(handle, pref);
            *state = saved == failed ? apply_unknown : (unsigned char)saved;
            if (*state == wanted) result = changed_saved;
        }
    }
    RegCloseKey(handle);
    if (result == changed_saved) {
        DWORD_PTR ignored;
        SendMessageTimeoutA(HWND_BROADCAST, WM_SETTINGCHANGE, 0, (LPARAM)pref->subkey,
                            SMTO_ABORTIFHUNG | SMTO_BLOCK, 100, &ignored);
    }
    return result;
}

static BOOL record_before_change(unsigned id);

enum { request_off, request_on, request_default, request_cycle };

static unsigned reset_setting(unsigned id, unsigned char *state, unsigned char *source) {
    const Preference *pref = preference_for(id);
    if (!pref) {
        BYTE wanted = default_for(id);
        return wanted ? apply_setting(id, wanted == on, state, source) : cannot_change;
    }
    if (pref->windows11 && older_windows()) return cannot_change;
    HKEY handle;
    LONG rc = RegOpenKeyExA(HKEY_CURRENT_USER, pref->subkey, 0, KEY_QUERY_VALUE | KEY_SET_VALUE, &handle);
    if (rc == ERROR_FILE_NOT_FOUND || rc == ERROR_PATH_NOT_FOUND) return no_change;
    if (rc != ERROR_SUCCESS) return cannot_change;
    rc = RegDeleteValueA(handle, pref->name);
    RegCloseKey(handle);
    if (rc == ERROR_FILE_NOT_FOUND) return no_change;
    if (rc != ERROR_SUCCESS) return change_failed;
    DWORD_PTR ignored;
    SendMessageTimeoutA(HWND_BROADCAST, WM_SETTINGCHANGE, 0, (LPARAM)pref->subkey,
                        SMTO_ABORTIFHUNG | SMTO_BLOCK, 100, &ignored);
    return changed_saved;
}

static unsigned next_request(unsigned id) {
    unsigned energy = o3_energy_next(id);
    if (energy != 99) return energy;
    unsigned char state = manual, source = 0;
    unsigned availability = check_option(id, &state, &source, older_windows());
    if (availability != live_toggle && availability != saved_toggle) return request_on;
    if (o3_defaults[id-1]) return o3_defaults[id-1] == on ? request_off : request_on;
    if (has_default(id)) return request_default;
    return state == on ? request_off : request_on;
}

unsigned o3_apply(unsigned id, unsigned requested, unsigned char *state, unsigned char *source,
                  unsigned char *availability) {
    if (!id || id > o3_item_total || requested > request_cycle) return cannot_change;
    if (requested == request_cycle) requested = next_request(id);

    if (!record_before_change(id)) return record_failed;
    unsigned result = id > item_count ? dynamic_apply(id, requested, state, source)
                    : requested == request_default ? reset_setting(id, state, source)
                    : apply_setting(id, requested, state, source);
    *availability = (unsigned char)check_option(id, state, source, older_windows());
    if (result == changed_saved || result == changed_live || result == no_change) {
        BOOL usable = *availability == saved_toggle || *availability == live_toggle;
        BOOL reached = requested == request_default ? o3_defaults[id-1] != 0
                                                    : *state == (requested ? on : off);
        if (!usable || !reached) return change_failed;
    }
    return result;
}

void o3_collect(unsigned char *values, const char *manifest, const char *path, const char *instance,
                unsigned char *sources, unsigned ambiguous) {
    for (unsigned i = 0; i < item_count; ++i) {
        values[i] = manual;
        sources[i] = 0;
    }
    load_probes(manifest);
    if (!safe_instance(instance)) instance = "";
    if (!safe_path(path)) path = "";
    emulator_file[0] = emulator_instance[0] = 0;
    if (!ambiguous && *path && lstrlenA(path) < (int)sizeof emulator_file) {
        lstrcpyA(emulator_file, path);
        lstrcpyA(emulator_instance, instance);
    }
    BOOL absent = !*path && missing_install();

    BOOL old_windows = older_windows();
    if (old_windows) {
        for (unsigned id = 44; id <= 47; ++id) values[id - 1] = unavailable;
        for (unsigned id = 85; id <= 87; ++id) values[id - 1] = unavailable;
    }
    for (unsigned i = 0; i < probe_count; ++i) {
        const Probe *p = &probes[i];
        BOOL emulator = p->kind == 'I' || p->kind == 'G';
        if (emulator && ambiguous) continue;
        if ((emulator && absent) || (old_windows && p->id == 49))
            values[p->id - 1] = unavailable;
        else values[p->id - 1] = failed;
    }

    BOOL loaded = load_config(path), pending = FALSE;
    for (unsigned i = 0; i < probe_count; ++i) {
        const Probe *p = &probes[i];
        unsigned char *v = &values[p->id - 1];
        if (*v != failed) continue;
        BOOL emulator = p->kind == 'I' || p->kind == 'G';
        *v = (unsigned char)(emulator ? config_value(p, instance, loaded, absent) : registry_value(p));
        if (*v == failed) pending = TRUE;
        else if (*v != manual && *v != unavailable) sources[p->id - 1] = emulator ? 5 : 3;
    }
    if (!pending || !probe_directory()) return;
    SetEnvironmentVariableA("O3_SCAN_CONFIG", *path ? path : NULL);
    SetEnvironmentVariableA("O3_SCAN_INSTANCE", *instance ? instance : NULL);
    if (run_command(0)) parse_output(0, values, sources);
    for (unsigned i = 0; i < probe_count; ++i) {
        if (values[probes[i].id - 1] == failed) {
            if (run_command(1)) parse_output(1, values, sources);
            break;
        }
    }
    SetEnvironmentVariableA("O3_SCAN_CONFIG", NULL);
    SetEnvironmentVariableA("O3_SCAN_INSTANCE", NULL);
}

#include "backup.inc"
#include "system_items.inc"

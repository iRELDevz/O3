#define COBJMACROS
#include <taskschd.h>
#include <psapi.h>

enum { dyn_service = 1, dyn_task, dyn_startup, dyn_background, dyn_energy, dyn_stop };

enum { tab_cpu = 4, tab_ram = 5, tab_energy = 6 };

enum { energy_normal, energy_eco, energy_max };

typedef struct {
    BYTE type, group, locked, place, running, sharing, restarts;
    DWORD baseline, current, pid;
    ULONGLONG weight;
    WCHAR *key;
    char *title, *detail;
    WCHAR image[64];
    WCHAR *path;
} DynItem;
enum { detail_size = 200 };

extern BackupFeature catalogue[];
extern DWORD catalogue_starts[], catalogue_counts[];

static DynItem *dyn_items[dynamic_limit];
static unsigned dyn_count;
static BOOL dyn_elevated;

static const GUID task_scheduler_class = {0x0f87369f,0xa4e5,0x4cfc,{0xbd,0x3e,0x73,0xe6,0x15,0x45,0x72,0xdd}};
static const GUID task_service_interface = {0x2faba4c7,0x4da9,0x4013,{0x96,0x97,0x20,0xcc,0x3f,0xd4,0x0f,0x85}};

static void *dyn_alloc(SIZE_T size) {
    return HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, size);
}

static WCHAR *dup_wide(const WCHAR *text) {
    WCHAR *copy = dyn_alloc((lstrlenW(text) + 1) * sizeof(WCHAR));
    if (copy) lstrcpyW(copy, text);
    return copy;
}

static char *dup_ansi(const char *text) {
    char *copy = dyn_alloc(lstrlenA(text) + 1);
    if (copy) lstrcpyA(copy, text);
    return copy;
}

static void to_ansi(const WCHAR *text, char *out, int size) {
    if (!WideCharToMultiByte(CP_ACP, 0, text, -1, out, size, NULL, NULL)) out[0] = 0;
    out[size - 1] = 0;
}

static BOOL starts_with(const WCHAR *text, const WCHAR *start) {
    int n = lstrlenW(start);
    return lstrlenW(text) >= n && CompareStringOrdinal(text, n, start, n, TRUE) == CSTR_EQUAL;
}

static BOOL clean_key(const WCHAR *key) {
    int n = 0;
    for (; key[n]; ++n) if (key[n] == '\r' || key[n] == '\n') return FALSE;
    return n > 0 && n < 900;
}

static ULONGLONG file_time(FILETIME t) {
    return ((ULONGLONG)t.dwHighDateTime << 32) | t.dwLowDateTime;
}

static BOOL process_elevated(void) {
    HANDLE token;
    TOKEN_ELEVATION elevation;
    DWORD size;
    BOOL result = FALSE;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
        result = GetTokenInformation(token, TokenElevation, &elevation, sizeof elevation, &size) &&
                 elevation.TokenIsElevated;
        CloseHandle(token);
    }
    return result;
}

typedef struct { DWORD pid; WCHAR name[64]; ULONGLONG ws, cpu, life; unsigned pct; BYTE mine; } ProcInfo;
typedef struct { DWORD pid; BYTE mine; } OwnerMark;
static OwnerMark *owner_marks;
static unsigned owner_count;

static BOOL player_owned(DWORD pid) {
    static BYTE own_buffer[256];
    static PSID own = NULL;
    static BOOL known = FALSE;
    DWORD session, own_session_id = 0, size;
    if (pid == GetCurrentProcessId() || !ProcessIdToSessionId(pid, &session) ||
        !ProcessIdToSessionId(GetCurrentProcessId(), &own_session_id) || session != own_session_id) return FALSE;
    if (!known) {
        HANDLE token;
        if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
            if (GetTokenInformation(token, TokenUser, own_buffer, sizeof own_buffer, &size))
                own = ((TOKEN_USER *)own_buffer)->User.Sid;
            CloseHandle(token);
        }
        known = TRUE;
    }
    if (!own) return FALSE;
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!h) return FALSE;
    HANDLE token;
    BYTE buffer[256];
    BOOL mine = FALSE;
    if (OpenProcessToken(h, TOKEN_QUERY, &token)) {
        if (GetTokenInformation(token, TokenUser, buffer, sizeof buffer, &size))
            mine = EqualSid(own, ((TOKEN_USER *)buffer)->User.Sid);
        CloseHandle(token);
    }
    CloseHandle(h);
    return mine;
}
typedef struct { DWORD pid; ULONGLONG cpu; } CpuMark;
static CpuMark *cpu_marks;
static unsigned cpu_mark_count;
static ULONGLONG cpu_mark_time;
enum { proc_limit = 2048 };
static ProcInfo *proc_list;
static unsigned proc_total;

static void snapshot_processes(BOOL measure) {
    proc_total = 0;
    if (!proc_list) proc_list = dyn_alloc(sizeof(ProcInfo) * proc_limit);
    if (!proc_list) return;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return;
    PROCESSENTRY32W entry;
    entry.dwSize = sizeof entry;
    FILETIME now;
    GetSystemTimeAsFileTime(&now);
    for (BOOL more = Process32FirstW(snap, &entry); more && proc_total < proc_limit;
         more = Process32NextW(snap, &entry)) {
        ProcInfo *p = &proc_list[proc_total++];
        p->pid = entry.th32ProcessID;
        lstrcpynW(p->name, entry.szExeFile, ARRAYSIZE(p->name));
        p->ws = p->cpu = p->life = 0;
        p->pct = 0;
        p->mine = 2;
        for (unsigned j = 0; j < owner_count; ++j)
            if (owner_marks[j].pid == p->pid) { p->mine = owner_marks[j].mine; break; }
        if (p->mine == 2) p->mine = (BYTE)player_owned(p->pid);
        if (!measure) continue;
        HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, p->pid);
        if (!h) continue;
        PROCESS_MEMORY_COUNTERS memory;
        memory.cb = sizeof memory;
        if (GetProcessMemoryInfo(h, &memory, sizeof memory)) p->ws = memory.WorkingSetSize;
        FILETIME created, ended, kernel, user;
        if (GetProcessTimes(h, &created, &ended, &kernel, &user)) {
            ULONGLONG start = file_time(created), current = file_time(now);
            p->cpu = file_time(kernel) + file_time(user);
            p->life = current > start ? current - start : 0;
        }
        CloseHandle(h);
    }
    CloseHandle(snap);
    if (!owner_marks) owner_marks = dyn_alloc(sizeof(OwnerMark) * proc_limit);
    if (owner_marks) {
        for (unsigned i = 0; i < proc_total; ++i) {
            owner_marks[i].pid = proc_list[i].pid;
            owner_marks[i].mine = proc_list[i].mine;
        }
        owner_count = proc_total;
    }
    if (!measure) return;

    static unsigned cores;
    if (!cores) {
        SYSTEM_INFO info;
        GetSystemInfo(&info);
        cores = info.dwNumberOfProcessors ? info.dwNumberOfProcessors : 1;
    }
    ULONGLONG stamp = file_time(now), span = stamp > cpu_mark_time ? stamp - cpu_mark_time : 0;
    for (unsigned i = 0; i < proc_total; ++i) {
        ProcInfo *p = &proc_list[i];
        ULONGLONG used = 0, base = 0;
        BOOL known = FALSE;
        if (span >= 2000000)
            for (unsigned j = 0; j < cpu_mark_count && !known; ++j)
                if (cpu_marks[j].pid == p->pid && cpu_marks[j].cpu <= p->cpu) {
                    known = TRUE;
                    used = p->cpu - cpu_marks[j].cpu;
                    base = span;
                }
        if (!known) { used = p->cpu; base = p->life; }
        p->pct = base ? (unsigned)(used * 1000 / (base * cores)) : 0;
        if (p->pct > 1000) p->pct = 1000;
    }
    if (!cpu_marks) cpu_marks = dyn_alloc(sizeof(CpuMark) * proc_limit);
    if (!cpu_marks) return;
    for (unsigned i = 0; i < proc_total; ++i) {
        cpu_marks[i].pid = proc_list[i].pid;
        cpu_marks[i].cpu = proc_list[i].cpu;
    }
    cpu_mark_count = proc_total;
    cpu_mark_time = stamp;
}

static const ProcInfo *process_by_pid(DWORD pid) {
    for (unsigned i = 0; i < proc_total; ++i) if (proc_list[i].pid == pid) return &proc_list[i];
    return NULL;
}

static ULONGLONG memory_of_image(const WCHAR *image) {
    ULONGLONG total = 0;
    if (!*image) return 0;
    for (unsigned i = 0; i < proc_total; ++i)
        if (!lstrcmpiW(proc_list[i].name, image)) total += proc_list[i].ws;
    return total;
}

static DynItem **found_items;
static unsigned found_count;

static DynItem *add_item(BYTE type, BYTE group, const WCHAR *key, const char *title, const char *detail,
                         DWORD current, ULONGLONG weight, BOOL writable) {
    if (found_count >= dynamic_limit || !clean_key(key) || !*title) return NULL;
    DynItem *d = dyn_alloc(sizeof *d);
    if (!d) return NULL;
    d->type = type;
    d->group = group;
    d->current = d->baseline = current;
    d->weight = weight;
    d->locked = (BYTE)!writable;
    d->key = dup_wide(key);
    d->title = dup_ansi(title);
    d->detail = dyn_alloc(detail_size);
    if (d->detail) lstrcpynA(d->detail, detail, detail_size);
    if (!d->key || !d->title || !d->detail) return NULL;
    found_items[found_count++] = d;
    return d;
}

static WCHAR *stopped_file;

static BOOL stopped_path(WCHAR *path) {
    if (!initialize_paths() || lstrlenW(backup_folder) + 24 >= MAX_PATH) return FALSE;
    wsprintfW(path, L"%s\\stopped-items.txt", backup_folder);
    return TRUE;
}

static void load_stopped_file(void) {
    WCHAR path[MAX_PATH];
    if (stopped_file) { HeapFree(GetProcessHeap(), 0, stopped_file); stopped_file = NULL; }
    if (!stopped_path(path)) return;
    HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (file == INVALID_HANDLE_VALUE) return;
    DWORD size = GetFileSize(file, NULL), read = 0;
    char *bytes = size && size < 256 * 1024 ? HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, size + 1) : NULL;
    if (bytes && ReadFile(file, bytes, size, &read, NULL) && read == size) {
        int n = MultiByteToWideChar(CP_UTF8, 0, bytes, (int)read, NULL, 0);
        stopped_file = n > 0 ? HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (SIZE_T)(n + 1) * sizeof(WCHAR)) : NULL;
        if (stopped_file) MultiByteToWideChar(CP_UTF8, 0, bytes, (int)read, stopped_file, n);
    }
    if (bytes) HeapFree(GetProcessHeap(), 0, bytes);
    CloseHandle(file);
}

static void each_stopped(void (*visit)(WCHAR kind, const WCHAR *name, const WCHAR *path)) {
    for (WCHAR *line = stopped_file; line && *line;) {
        WCHAR *end = line;
        while (*end && *end != '\n') ++end;
        WCHAR *stop = end > line && end[-1] == '\r' ? end - 1 : end;
        if (stop - line > 2 && stop - line < 300 && line[1] == '|' &&
            (line[0] == 'S' || line[0] == 'P' || line[0] == 'W' || line[0] == 'Z')) {
            WCHAR name[300], *path = NULL;
            int n = (int)(stop - line - 2);
            for (int i = 0; i < n; ++i) name[i] = line[2 + i];
            name[n] = 0;
            for (int i = 0; i < n; ++i) if (name[i] == '	') { name[i] = 0; path = name + i + 1; break; }
            visit(line[0], name, path);
        }
        line = *end ? end + 1 : end;
    }
}

static BOOL stopped_has(WCHAR kind, const WCHAR *name) {
    for (WCHAR *line = stopped_file; line && *line;) {
        WCHAR *end = line;
        while (*end && *end != '\n') ++end;
        WCHAR *stop = end > line && end[-1] == '\r' ? end - 1 : end;
        int n = (int)(stop - line - 2), length = lstrlenW(name);
        for (int i = 0; i < n; ++i) if (line[2 + i] == '	') { n = i; break; }
        if (n == length && line[0] == kind && line[1] == '|' &&
            CompareStringOrdinal(line + 2, n, name, length, TRUE) == CSTR_EQUAL) return TRUE;
        line = *end ? end + 1 : end;
    }
    return FALSE;
}

static void save_stopped(WCHAR kind, const WCHAR *name, const WCHAR *exe, BOOL off) {
    WCHAR path[MAX_PATH], temp[MAX_PATH];
    load_stopped_file();
    if (stopped_has(kind, name) == off) return;
    if (!stopped_path(path)) return;
    lstrcpyW(temp, path);
    lstrcatW(temp, L".tmp");
    HANDLE file = CreateFileW(temp, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) return;
    char line[1400];
    DWORD written;
    for (WCHAR *row = stopped_file; row && *row;) {
        WCHAR *end = row;
        while (*end && *end != '\n') ++end;
        WCHAR *stop = end > row && end[-1] == '\r' ? end - 1 : end;
        int n = (int)(stop - row), length = lstrlenW(name), name_end = n;
        for (int i = 2; i < n; ++i) if (row[i] == '	') { name_end = i; break; }
        BOOL same = name_end - 2 == length && row[0] == kind && row[1] == '|' &&
                    CompareStringOrdinal(row + 2, length, name, length, TRUE) == CSTR_EQUAL;
        if (!same && n > 0 && n < 600) {
            int bytes = WideCharToMultiByte(CP_UTF8, 0, row, n, line, sizeof line - 2, NULL, NULL);
            line[bytes++] = '\r'; line[bytes++] = '\n';
            WriteFile(file, line, (DWORD)bytes, &written, NULL);
        }
        row = *end ? end + 1 : end;
    }
    if (off) {
        int bytes = wsprintfA(line, "%c|", (char)kind);
        bytes += WideCharToMultiByte(CP_UTF8, 0, name, -1, line + bytes, sizeof line - bytes - 2, NULL, NULL) - 1;
        if (exe && *exe) {
            line[bytes++] = '	';
            bytes += WideCharToMultiByte(CP_UTF8, 0, exe, -1, line + bytes, sizeof line - bytes - 2, NULL, NULL) - 1;
        }
        line[bytes++] = '\r'; line[bytes++] = '\n';
        WriteFile(file, line, (DWORD)bytes, &written, NULL);
    }
    FlushFileBuffers(file);
    CloseHandle(file);
    if (MoveFileExW(temp, path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) load_stopped_file();
    else DeleteFileW(temp);
}

static const WCHAR *const protected_services[] = {
    L"AppIDSvc", L"Appinfo", L"AppXSvc", L"AudioEndpointBuilder", L"Audiosrv", L"BFE", L"BITS",
    L"BrokerInfrastructure", L"camsvc", L"ClipSVC", L"CoreMessagingRegistrar", L"CryptSvc",
    L"DcomLaunch", L"DeviceInstall", L"Dhcp", L"DispBrokerDesktopSvc", L"Dnscache", L"DsmSvc",
    L"EventLog", L"EventSystem", L"gpsvc", L"hidserv", L"KeyIso", L"LanmanWorkstation", L"LSM",
    L"mpssvc", L"msiserver", L"Netlogon", L"netprofm", L"NgcCtnrSvc", L"NgcSvc", L"NlaSvc", L"nsi",
    L"PlugPlay", L"Power", L"ProfSvc", L"RpcEptMapper", L"RpcSs", L"SamSs", L"Schedule",
    L"SecurityHealthService", L"Sense", L"SENS", L"SgrmBroker", L"StateRepository",
    L"SystemEventsBroker", L"TextInputManagementService", L"TimeBrokerSvc", L"TokenBroker",
    L"TrustedInstaller", L"UserManager", L"UsoSvc", L"VaultSvc", L"WaaSMedicSvc", L"Wcmsvc",
    L"WdNisSvc", L"webthreatdefsvc", L"webthreatdefusersvc", L"WinDefend", L"WinHttpAutoProxySvc",
    L"Winmgmt", L"WlanSvc", L"wscsvc", L"wuauserv"
};

static BOOL protected_service(const WCHAR *name) {
    for (unsigned i = 0; i < ARRAYSIZE(protected_services); ++i)
        if (!lstrcmpiW(name, protected_services[i])) return TRUE;
    return FALSE;
}

static BOOL service_start(SC_HANDLE service, DWORD *value) {
    static BYTE buffer[8192];
    DWORD needed;
    QUERY_SERVICE_CONFIGW *config = (QUERY_SERVICE_CONFIGW *)buffer;
    if (!QueryServiceConfigW(service, config, sizeof buffer, &needed)) return FALSE;
    *value = config->dwStartType;
    if (*value == SERVICE_AUTO_START) {
        SERVICE_DELAYED_AUTO_START_INFO delayed;
        if (QueryServiceConfig2W(service, SERVICE_CONFIG_DELAYED_AUTO_START_INFO, (BYTE *)&delayed,
                                 sizeof delayed, &needed) && delayed.fDelayedAutostart) *value = 6;
    }
    return TRUE;
}

static BYTE *service_buffer;
static DWORD service_buffer_size;

static ENUM_SERVICE_STATUS_PROCESSW *enum_services(DWORD *count) {
    *count = 0;
    SC_HANDLE scm = OpenSCManagerW(NULL, NULL, SC_MANAGER_CONNECT | SC_MANAGER_ENUMERATE_SERVICE);
    if (!scm) return NULL;
    BOOL ok = FALSE;
    for (unsigned attempt = 0; attempt < 3 && !ok; ++attempt) {
        DWORD needed = 0, resume = 0;
        ok = service_buffer && EnumServicesStatusExW(scm, SC_ENUM_PROCESS_INFO, SERVICE_WIN32, SERVICE_STATE_ALL,
                                                     service_buffer, service_buffer_size, &needed, count, &resume, NULL);
        if (ok || (service_buffer && GetLastError() != ERROR_MORE_DATA)) break;
        if (!service_buffer)
            EnumServicesStatusExW(scm, SC_ENUM_PROCESS_INFO, SERVICE_WIN32, SERVICE_STATE_ALL, NULL, 0,
                                  &needed, count, &resume, NULL);
        if (service_buffer) HeapFree(GetProcessHeap(), 0, service_buffer);
        service_buffer_size = needed + 16384;
        service_buffer = dyn_alloc(service_buffer_size);
        *count = 0;
    }
    CloseServiceHandle(scm);
    return ok ? (ENUM_SERVICE_STATUS_PROCESSW *)service_buffer : NULL;
}

static unsigned services_in_process(const ENUM_SERVICE_STATUS_PROCESSW *list, DWORD count, DWORD pid) {
    unsigned n = 0;
    for (DWORD i = 0; i < count; ++i)
        if (list[i].ServiceStatusProcess.dwProcessId == pid &&
            list[i].ServiceStatusProcess.dwCurrentState == SERVICE_RUNNING) ++n;
    return n;
}

static void scan_services(void) {
    DWORD count = 0;
    ENUM_SERVICE_STATUS_PROCESSW *list = enum_services(&count);
    SC_HANDLE scm = list ? OpenSCManagerW(NULL, NULL, SC_MANAGER_CONNECT) : NULL;
    if (!scm) return;
    for (DWORD i = 0; i < count; ++i) {
        const ENUM_SERVICE_STATUS_PROCESSW *s = &list[i];

        if ((s->ServiceStatusProcess.dwServiceType & 0xC0) || protected_service(s->lpServiceName)) continue;
        SC_HANDLE handle = OpenServiceW(scm, s->lpServiceName, SERVICE_QUERY_CONFIG);
        DWORD start;
        BOOL known = handle && service_start(handle, &start);
        if (handle) CloseServiceHandle(handle);
        if (!known || start < SERVICE_AUTO_START) continue;
        BOOL writable = FALSE;
        if (dyn_elevated && (handle = OpenServiceW(scm, s->lpServiceName, SERVICE_CHANGE_CONFIG))) {
            writable = TRUE;
            CloseServiceHandle(handle);
        }
        DWORD pid = s->ServiceStatusProcess.dwProcessId;
        BOOL running = s->ServiceStatusProcess.dwCurrentState == SERVICE_RUNNING && pid;
        unsigned sharing = running ? services_in_process(list, count, pid) : 0;
        const ProcInfo *proc = running ? process_by_pid(pid) : NULL;
        BOOL own = proc && sharing == 1;
        char title[256];
        to_ansi(s->lpDisplayName, title, sizeof title);
        BYTE group = running ? tab_ram : tab_cpu;
        ULONGLONG weight = 0;
        if (proc) {
            unsigned share = own ? 1 : sharing;
            if (own && proc->pct >= 5) group = tab_cpu;
            weight = group == tab_cpu ? proc->pct : proc->ws / share;
        }
        DynItem *d = add_item(dyn_service, group, s->lpServiceName, title, "-", start, weight, writable);
        if (d) {
            d->running = (BYTE)running;
            d->pid = pid;
            d->sharing = (BYTE)(sharing > 255 ? 255 : sharing);
        }

        if (running && (s->ServiceStatusProcess.dwControlsAccepted & SERVICE_ACCEPT_STOP)) {
            char stop_title[300];
            wsprintfA(stop_title, "Hentikan: %s", title);
            DynItem *t = add_item(dyn_stop, group, s->lpServiceName, stop_title, "-", 1, weight, dyn_elevated);
            if (t) {
                t->place = 0;
                t->running = 1;
                t->pid = pid;
                t->sharing = (BYTE)(sharing > 255 ? 255 : sharing);
            }
        }
    }
    CloseServiceHandle(scm);
}

static BOOL service_read(const WCHAR *name, DWORD *value) {
    SC_HANDLE scm = OpenSCManagerW(NULL, NULL, SC_MANAGER_CONNECT);
    if (!scm) return FALSE;
    SC_HANDLE service = OpenServiceW(scm, name, SERVICE_QUERY_CONFIG);
    BOOL ok = service && service_start(service, value);
    if (service) CloseServiceHandle(service);
    CloseServiceHandle(scm);
    return ok;
}

static BOOL service_write(const WCHAR *name, DWORD value, BOOL *live) {
    SC_HANDLE scm = OpenSCManagerW(NULL, NULL, SC_MANAGER_CONNECT);
    if (!scm) return FALSE;
    SC_HANDLE service = OpenServiceW(scm, name, SERVICE_CHANGE_CONFIG | SERVICE_QUERY_CONFIG |
                                     SERVICE_QUERY_STATUS | SERVICE_STOP | SERVICE_START);
    DWORD start = value == 6 ? SERVICE_AUTO_START : value;
    BOOL ok = service && ChangeServiceConfigW(service, SERVICE_NO_CHANGE, start, SERVICE_NO_CHANGE,
                                              NULL, NULL, NULL, NULL, NULL, NULL, NULL);
    if (ok && start == SERVICE_AUTO_START) {
        SERVICE_DELAYED_AUTO_START_INFO delayed = { value == 6 };
        ChangeServiceConfig2W(service, SERVICE_CONFIG_DELAYED_AUTO_START_INFO, &delayed);
    }
    if (ok && value == SERVICE_DISABLED) {
        SERVICE_STATUS status;
        if (ControlService(service, SERVICE_CONTROL_STOP, &status) || GetLastError() == ERROR_SERVICE_NOT_ACTIVE) {
            for (unsigned wait = 0; wait < 40; ++wait) {
                if (!QueryServiceStatus(service, &status)) break;
                if (status.dwCurrentState == SERVICE_STOPPED) { *live = TRUE; break; }
                Sleep(100);
            }
        }
    }
    if (ok && start == SERVICE_AUTO_START &&
        (StartServiceW(service, 0, NULL) || GetLastError() == ERROR_SERVICE_ALREADY_RUNNING)) *live = TRUE;
    if (service) CloseServiceHandle(service);
    CloseServiceHandle(scm);
    return ok;
}

static const WCHAR *const protected_tasks[] = {
    L"\\Microsoft\\Windows\\AppID\\", L"\\Microsoft\\Windows\\BitLocker\\",
    L"\\Microsoft\\Windows\\CertificateServicesClient\\", L"\\Microsoft\\Windows\\Chkdsk\\",
    L"\\Microsoft\\Windows\\EDP\\", L"\\Microsoft\\Windows\\ExploitGuard\\",
    L"\\Microsoft\\Windows\\InstallService\\", L"\\Microsoft\\Windows\\Plug and Play\\",
    L"\\Microsoft\\Windows\\Registry\\", L"\\Microsoft\\Windows\\Servicing\\",
    L"\\Microsoft\\Windows\\SoftwareProtectionPlatform\\", L"\\Microsoft\\Windows\\SystemRestore\\",
    L"\\Microsoft\\Windows\\TaskScheduler\\", L"\\Microsoft\\Windows\\TPM\\",
    L"\\Microsoft\\Windows\\UpdateOrchestrator\\", L"\\Microsoft\\Windows\\WaaSMedic\\",
    L"\\Microsoft\\Windows\\Windows Defender\\", L"\\Microsoft\\Windows\\WindowsUpdate\\"
};

static BOOL protected_task(const WCHAR *path) {
    for (unsigned i = 0; i < ARRAYSIZE(protected_tasks); ++i)
        if (starts_with(path, protected_tasks[i])) return TRUE;
    return FALSE;
}

static ITaskService *connect_tasks(void) {
    ITaskService *service = NULL;
    if (FAILED(CoCreateInstance(&task_scheduler_class, NULL, CLSCTX_INPROC_SERVER, &task_service_interface,
                                (void **)&service)) || !service) return NULL;
    VARIANT empty;
    VariantInit(&empty);
    if (FAILED(ITaskService_Connect(service, empty, empty, empty, empty))) {
        ITaskService_Release(service);
        return NULL;
    }
    return service;
}

static void scan_task_folder(ITaskFolder *folder, unsigned depth) {
    IRegisteredTaskCollection *tasks = NULL;
    LONG count = 0;
    if (SUCCEEDED(ITaskFolder_GetTasks(folder, TASK_ENUM_HIDDEN, &tasks)) && tasks &&
        SUCCEEDED(IRegisteredTaskCollection_get_Count(tasks, &count))) {
        for (LONG i = 1; i <= count; ++i) {
            VARIANT index;
            VariantInit(&index);
            index.vt = VT_I4;
            index.lVal = i;
            IRegisteredTask *task = NULL;
            if (FAILED(IRegisteredTaskCollection_get_Item(tasks, index, &task)) || !task) continue;
            BSTR path = NULL, name = NULL;
            VARIANT_BOOL enabled = VARIANT_FALSE;
            if (SUCCEEDED(IRegisteredTask_get_Path(task, &path)) && path &&
                SUCCEEDED(IRegisteredTask_get_Name(task, &name)) && name &&
                SUCCEEDED(IRegisteredTask_get_Enabled(task, &enabled)) && !protected_task(path)) {
                char title[256], folder_text[512], detail[600];
                to_ansi(name, title, sizeof title);
                to_ansi(path, folder_text, sizeof folder_text);
                int cut = lstrlenA(folder_text) - lstrlenA(title);
                if (cut > 1) folder_text[cut - 1] = 0;
                else lstrcpyA(folder_text, "\\");
                wsprintfA(detail, "Scheduled task | %s", folder_text);
                add_item(dyn_task, tab_cpu, path, title, detail, enabled ? 1 : 0, 0, dyn_elevated);
            }
            if (path) SysFreeString(path);
            if (name) SysFreeString(name);
            IRegisteredTask_Release(task);
        }
    }
    if (tasks) IRegisteredTaskCollection_Release(tasks);
    if (depth >= 8) return;
    ITaskFolderCollection *folders = NULL;
    if (SUCCEEDED(ITaskFolder_GetFolders(folder, 0, &folders)) && folders &&
        SUCCEEDED(ITaskFolderCollection_get_Count(folders, &count))) {
        for (LONG i = 1; i <= count; ++i) {
            VARIANT index;
            VariantInit(&index);
            index.vt = VT_I4;
            index.lVal = i;
            ITaskFolder *child = NULL;
            if (SUCCEEDED(ITaskFolderCollection_get_Item(folders, index, &child)) && child) {
                scan_task_folder(child, depth + 1);
                ITaskFolder_Release(child);
            }
        }
    }
    if (folders) ITaskFolderCollection_Release(folders);
}

static void scan_tasks(void) {
    ITaskService *service = connect_tasks();
    if (!service) return;
    BSTR root = SysAllocString(L"\\");
    ITaskFolder *folder = NULL;
    if (root && SUCCEEDED(ITaskService_GetFolder(service, root, &folder)) && folder) {
        scan_task_folder(folder, 0);
        ITaskFolder_Release(folder);
    }
    if (root) SysFreeString(root);
    ITaskService_Release(service);
}

static BOOL task_access(const WCHAR *path, int requested, DWORD *value) {
    HRESULT com = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    BOOL ok = FALSE;
    ITaskService *service = connect_tasks();
    int split = lstrlenW(path);
    while (split > 0 && path[split - 1] != '\\') --split;
    WCHAR *folder_path = dup_wide(path);
    if (service && folder_path && split > 0) {
        folder_path[split > 1 ? split - 1 : 1] = 0;
        BSTR folder_name = SysAllocString(folder_path), task_name = SysAllocString(path + split);
        ITaskFolder *folder = NULL;
        IRegisteredTask *task = NULL;
        if (folder_name && task_name && SUCCEEDED(ITaskService_GetFolder(service, folder_name, &folder)) && folder &&
            SUCCEEDED(ITaskFolder_GetTask(folder, task_name, &task)) && task) {
            ok = TRUE;
            if (requested >= 0) {
                ok = SUCCEEDED(IRegisteredTask_put_Enabled(task, requested ? VARIANT_TRUE : VARIANT_FALSE));
                if (ok && !requested) IRegisteredTask_Stop(task, 0);
                IRegisteredTask_Release(task);
                task = NULL;
                ok = ok && SUCCEEDED(ITaskFolder_GetTask(folder, task_name, &task)) && task;
            }
            VARIANT_BOOL enabled = VARIANT_FALSE;
            ok = ok && SUCCEEDED(IRegisteredTask_get_Enabled(task, &enabled));
            if (ok) *value = enabled ? 1 : 0;
        }
        if (task) IRegisteredTask_Release(task);
        if (folder) ITaskFolder_Release(folder);
        if (folder_name) SysFreeString(folder_name);
        if (task_name) SysFreeString(task_name);
    }
    if (folder_path) HeapFree(GetProcessHeap(), 0, folder_path);
    if (service) ITaskService_Release(service);
    if (SUCCEEDED(com)) CoUninitialize();
    return ok;
}

#define APPROVED L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\StartupApproved\\"
typedef struct { BOOL machine; REGSAM view; const WCHAR *run; const WCHAR *approval; int folder; const char *label; } StartupPlace;
static const StartupPlace startup_places[] = {
    {FALSE, 0, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", APPROVED L"Run", 0, "pengguna"},
    {TRUE, KEY_WOW64_64KEY, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", APPROVED L"Run", 0, "semua pengguna"},
    {TRUE, KEY_WOW64_32KEY, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", APPROVED L"Run32", 0, "semua pengguna (32-bit)"},
    {FALSE, 0, NULL, APPROVED L"StartupFolder", CSIDL_STARTUP, "folder Startup"},
    {TRUE, 0, NULL, APPROVED L"StartupFolder", CSIDL_COMMON_STARTUP, "folder Startup bersama"}
};

static HKEY place_root(const StartupPlace *place) {
    return place->machine ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
}

static BOOL startup_read(const StartupPlace *place, const WCHAR *name, DWORD *value) {
    BYTE data[64];
    DWORD size = sizeof data;
    LONG rc = RegGetValueW(place_root(place), place->approval, name, RRF_RT_REG_BINARY, NULL, data, &size);
    if (rc == ERROR_FILE_NOT_FOUND || rc == ERROR_PATH_NOT_FOUND) { *value = 0; return TRUE; }
    if (rc != ERROR_SUCCESS || size < 1) return FALSE;
    *value = data[0] ? data[0] : 2;
    return TRUE;
}

static BOOL startup_write(const StartupPlace *place, const WCHAR *name, DWORD value) {
    HKEY root = place_root(place);
    if (!value) {
        LONG rc = RegDeleteKeyValueW(root, place->approval, name);
        return rc == ERROR_SUCCESS || rc == ERROR_FILE_NOT_FOUND || rc == ERROR_PATH_NOT_FOUND;
    }
    BYTE data[12];
    for (unsigned i = 0; i < sizeof data; ++i) data[i] = 0;
    data[0] = (BYTE)value;
    if (value & 1) {
        FILETIME now;
        GetSystemTimeAsFileTime(&now);
        for (unsigned i = 0; i < sizeof now; ++i) data[4 + i] = ((const BYTE *)&now)[i];
    }
    HKEY key;
    if (RegCreateKeyExW(root, place->approval, 0, NULL, 0, KEY_SET_VALUE, NULL, &key, NULL) != ERROR_SUCCESS)
        return FALSE;
    LONG rc = RegSetValueExW(key, name, 0, REG_BINARY, data, sizeof data);
    RegCloseKey(key);
    return rc == ERROR_SUCCESS;
}

static void command_image(const WCHAR *command, WCHAR *image, int size) {
    image[0] = 0;
    const WCHAR *start = command, *end = NULL;
    for (const WCHAR *p = command; *p; ++p) {
        if (starts_with(p, L"--processStart ")) {
            start = p + 15;
            while (*start == ' ' || *start == '"') ++start;
            end = start;
            while (*end && *end != ' ' && *end != '"') ++end;
            break;
        }
    }
    if (!end) {
        for (const WCHAR *p = command; *p; ++p)
            if (starts_with(p, L".exe")) { end = p + 4; break; }
        if (!end) return;
        start = end;
        while (start > command && start[-1] != '\\' && start[-1] != '"' && start[-1] != '/') --start;
    }
    int n = (int)(end - start);
    if (n <= 0 || n >= size) return;
    for (int i = 0; i < n; ++i) image[i] = start[i];
    image[n] = 0;
}

static void add_startup(unsigned index, const WCHAR *name, const WCHAR *command) {
    const StartupPlace *place = &startup_places[index];
    if (!lstrcmpiW(name, L"SecurityHealth") || lstrlenW(name) > 255) return;
    DWORD value;
    if (!startup_read(place, name, &value)) return;
    WCHAR key[300], image[64];
    wsprintfW(key, L"%u:%s", index, name);
    command_image(command, image, ARRAYSIZE(image));
    ULONGLONG memory = memory_of_image(image);
    char title[256], image_text[96], detail[400];
    to_ansi(name, title, sizeof title);
    to_ansi(*image ? image : L"shortcut", image_text, sizeof image_text);
    if (memory) wsprintfA(detail, "Startup %s | %s | berjalan %u MB RAM", place->label, image_text, (unsigned)(memory >> 20));
    else wsprintfA(detail, "Startup %s | %s | berlaku saat Windows dinyalakan", place->label, image_text);
    DynItem *d = add_item(dyn_startup, tab_ram, key, title, detail, value, memory, !place->machine || dyn_elevated);
    if (d) {
        d->place = (BYTE)index;
        lstrcpyW(d->image, image);
        d->running = (BYTE)(memory != 0);
    }
}

static void scan_startup(void) {
    for (unsigned index = 0; index < ARRAYSIZE(startup_places); ++index) {
        const StartupPlace *place = &startup_places[index];
        if (place->run) {
            HKEY key;
            if (RegOpenKeyExW(place_root(place), place->run, 0, KEY_READ | place->view, &key) != ERROR_SUCCESS) continue;
            for (DWORD i = 0;; ++i) {
                WCHAR name[256], command[1024];
                DWORD name_size = ARRAYSIZE(name), data_size = sizeof command - sizeof(WCHAR), type;
                LONG rc = RegEnumValueW(key, i, name, &name_size, NULL, &type, (BYTE *)command, &data_size);
                if (rc == ERROR_NO_MORE_ITEMS) break;
                if (rc != ERROR_SUCCESS || (type != REG_SZ && type != REG_EXPAND_SZ) || !name_size) continue;
                command[data_size / sizeof(WCHAR)] = 0;
                add_startup(index, name, command);
            }
            RegCloseKey(key);
        } else {
            WCHAR folder[MAX_PATH], pattern[MAX_PATH + 4];
            if (FAILED(SHGetFolderPathW(NULL, place->folder, NULL, 0, folder)) || lstrlenW(folder) > MAX_PATH - 4) continue;
            wsprintfW(pattern, L"%s\\*", folder);
            WIN32_FIND_DATAW file;
            HANDLE find = FindFirstFileW(pattern, &file);
            if (find == INVALID_HANDLE_VALUE) continue;
            do {
                if (file.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_HIDDEN)) continue;
                if (!lstrcmpiW(file.cFileName, L"desktop.ini")) continue;
                add_startup(index, file.cFileName, file.cFileName);
            } while (FindNextFileW(find, &file));
            FindClose(find);
        }
    }
}

#define BACKGROUND_APPS L"Software\\Microsoft\\Windows\\CurrentVersion\\BackgroundAccessApplications"

static BOOL background_read(const WCHAR *package, DWORD *value) {
    WCHAR path[400];
    if (lstrlenW(package) > 300) return FALSE;
    wsprintfW(path, L"%s\\%s", BACKGROUND_APPS, package);
    DWORD disabled = 0, size = sizeof disabled;
    LONG rc = RegGetValueW(HKEY_CURRENT_USER, path, L"Disabled", RRF_RT_REG_DWORD, NULL, &disabled, &size);
    if (rc != ERROR_SUCCESS && rc != ERROR_FILE_NOT_FOUND) return FALSE;
    *value = rc == ERROR_SUCCESS && disabled ? 0 : 1;
    return TRUE;
}

static BOOL background_write(const WCHAR *package, DWORD value) {
    WCHAR path[400];
    HKEY key;
    if (lstrlenW(package) > 300) return FALSE;
    wsprintfW(path, L"%s\\%s", BACKGROUND_APPS, package);
    if (RegCreateKeyExW(HKEY_CURRENT_USER, path, 0, NULL, 0, KEY_SET_VALUE, NULL, &key, NULL) != ERROR_SUCCESS)
        return FALSE;
    BOOL ok;
    if (value) {
        LONG a = RegDeleteValueW(key, L"Disabled"), b = RegDeleteValueW(key, L"DisabledByUser");
        ok = (a == ERROR_SUCCESS || a == ERROR_FILE_NOT_FOUND) && (b == ERROR_SUCCESS || b == ERROR_FILE_NOT_FOUND);
    } else {
        DWORD one = 1;
        ok = RegSetValueExW(key, L"Disabled", 0, REG_DWORD, (const BYTE *)&one, sizeof one) == ERROR_SUCCESS &&
             RegSetValueExW(key, L"DisabledByUser", 0, REG_DWORD, (const BYTE *)&one, sizeof one) == ERROR_SUCCESS;
    }
    RegCloseKey(key);
    return ok;
}

static void scan_background(void) {
    HKEY key;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, BACKGROUND_APPS, 0, KEY_READ, &key) != ERROR_SUCCESS) return;
    for (DWORD i = 0;; ++i) {
        WCHAR package[256];
        DWORD size = ARRAYSIZE(package), value;
        LONG rc = RegEnumKeyExW(key, i, package, &size, NULL, NULL, NULL, NULL);
        if (rc == ERROR_NO_MORE_ITEMS) break;
        if (rc != ERROR_SUCCESS || !background_read(package, &value)) continue;
        char name[256], title[300], detail[400];
        to_ansi(package, name, sizeof name);
        wsprintfA(detail, "Aplikasi Store berjalan di latar | %s", name);
        for (char *c = name; *c; ++c) if (*c == '_') { *c = 0; break; }
        wsprintfA(title, "Latar: %s", name);
        add_item(dyn_background, tab_cpu, package, title, detail, value, 0, TRUE);
    }
    RegCloseKey(key);
}

static DynItem *dynamic_item(unsigned id);

static const WCHAR *const energy_skip[] = {
    L"System", L"Registry", L"smss.exe", L"csrss.exe", L"wininit.exe", L"winlogon.exe", L"services.exe",
    L"lsass.exe", L"lsaiso.exe", L"svchost.exe", L"dwm.exe", L"fontdrvhost.exe", L"sihost.exe",
    L"ctfmon.exe", L"conhost.exe", L"taskhostw.exe", L"explorer.exe", L"userinit.exe", L"LogonUI.exe",
    L"ShellExperienceHost.exe", L"StartMenuExperienceHost.exe", L"TextInputHost.exe", L"audiodg.exe",
    L"OffOrOn.exe", L"O3Windows.exe", L"SecurityHealthSystray.exe", L"MsMpEng.exe", L"NisSrv.exe",
    L"MemCompression", L"Secure System", L"Idle"
};

static BOOL energy_skipped(const WCHAR *name) {
    for (unsigned i = 0; i < ARRAYSIZE(energy_skip); ++i) if (!lstrcmpiW(name, energy_skip[i])) return TRUE;
    return FALSE;
}

static const WCHAR *const never_close[] = {
    L"System", L"Registry", L"smss.exe", L"csrss.exe", L"wininit.exe", L"winlogon.exe",
    L"services.exe", L"lsass.exe", L"lsaiso.exe", L"svchost.exe", L"dwm.exe", L"fontdrvhost.exe",
    L"MemCompression", L"Memory Compression", L"[System Process]", L"Secure System", L"Idle", L"LsaIso.exe", L"audiodg.exe", L"spoolsv.exe",
    L"MsMpEng.exe", L"NisSrv.exe", L"MpDefenderCoreService.exe", L"SecurityHealthService.exe",
    L"OffOrOn.exe", L"O3Windows.exe", L"conhost.exe", L"WUDFHost.exe", L"dasHost.exe"
};

static BOOL never_closed(const WCHAR *name) {
    for (unsigned i = 0; i < ARRAYSIZE(never_close); ++i) if (!lstrcmpiW(name, never_close[i])) return TRUE;
    return FALSE;
}

static BOOL critical_process(DWORD pid) {
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!h) return TRUE;
    BOOL critical = TRUE;
    if (!IsProcessCritical(h, &critical)) critical = TRUE;
    CloseHandle(h);
    return critical;
}

static BOOL user_process(const ProcInfo *p) {
    return p->mine == 1;
}

static const WCHAR *const restarting_hosts[] = {
    L"msedgewebview2.exe", L"RuntimeBroker.exe", L"SearchHost.exe", L"dllhost.exe",
    L"backgroundTaskHost.exe", L"smartscreen.exe", L"ApplicationFrameHost.exe", L"LockApp.exe",
    L"UserOOBEBroker.exe", L"CompPkgSrv.exe", L"ShellHost.exe", L"WidgetService.exe",
    L"SystemSettingsBroker.exe", L"SearchProtocolHost.exe", L"SearchFilterHost.exe", L"WmiPrvSE.exe",
    L"unsecapp.exe", L"AggregatorHost.exe"
};

static WCHAR *restart_file;

static BOOL restart_path(WCHAR *path) {
    if (!initialize_paths() || lstrlenW(backup_folder) + 24 >= MAX_PATH) return FALSE;
    wsprintfW(path, L"%s\\auto-restart.txt", backup_folder);
    return TRUE;
}

static void load_restart_file(void) {
    WCHAR path[MAX_PATH];
    if (restart_file) { HeapFree(GetProcessHeap(), 0, restart_file); restart_file = NULL; }
    if (!restart_path(path)) return;
    HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (file == INVALID_HANDLE_VALUE) return;
    DWORD size = GetFileSize(file, NULL), read = 0;
    char *bytes = size && size < 256 * 1024 ? dyn_alloc(size + 1) : NULL;
    if (bytes && ReadFile(file, bytes, size, &read, NULL) && read == size) {
        int n = MultiByteToWideChar(CP_UTF8, 0, bytes, (int)read, NULL, 0);
        restart_file = n > 0 ? dyn_alloc((SIZE_T)(n + 1) * sizeof(WCHAR)) : NULL;
        if (restart_file) MultiByteToWideChar(CP_UTF8, 0, bytes, (int)read, restart_file, n);
    }
    if (bytes) HeapFree(GetProcessHeap(), 0, bytes);
    CloseHandle(file);
}

static BOOL restarts_automatically(const WCHAR *name) {
    for (unsigned i = 0; i < ARRAYSIZE(restarting_hosts); ++i)
        if (!lstrcmpiW(name, restarting_hosts[i])) return TRUE;
    int length = lstrlenW(name);
    for (const WCHAR *line = restart_file; line && *line;) {
        const WCHAR *end = line;
        while (*end && *end != '\n') ++end;
        const WCHAR *stop = end > line && end[-1] == '\r' ? end - 1 : end;
        if (stop - line == length && CompareStringOrdinal(line, length, name, length, TRUE) == CSTR_EQUAL) return TRUE;
        line = *end ? end + 1 : end;
    }
    return FALSE;
}

static void remember_restart(DynItem *d) {
    d->restarts = 1;
    d->locked = 1;
    if (d->type != dyn_stop || d->place != 1 || restarts_automatically(d->key)) return;
    WCHAR path[MAX_PATH];
    char line[200];
    if (!restart_path(path)) return;
    int n = WideCharToMultiByte(CP_UTF8, 0, d->key, -1, line, sizeof line - 3, NULL, NULL);
    if (n <= 1) return;
    line[n - 1] = '\r';
    line[n] = '\n';
    HANDLE file = CreateFileW(path, FILE_APPEND_DATA, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    DWORD written;
    if (file != INVALID_HANDLE_VALUE) {
        WriteFile(file, line, (DWORD)n + 1, &written, NULL);
        CloseHandle(file);
    }
    load_restart_file();
}

static BOOL energy_path(WCHAR *path) {
    if (!initialize_paths() || lstrlenW(backup_folder) + 24 >= MAX_PATH) return FALSE;
    wsprintfW(path, L"%s\\energy-choices.txt", backup_folder);
    return TRUE;
}

static WCHAR *energy_file;

static void load_energy_file(void) {
    WCHAR path[MAX_PATH];
    if (energy_file) { HeapFree(GetProcessHeap(), 0, energy_file); energy_file = NULL; }
    if (!energy_path(path)) return;
    HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (file == INVALID_HANDLE_VALUE) return;
    DWORD size = GetFileSize(file, NULL), read = 0;
    char *bytes = size && size < 1024 * 1024 ? dyn_alloc(size + 1) : NULL;
    if (bytes && ReadFile(file, bytes, size, &read, NULL) && read == size) {
        int n = MultiByteToWideChar(CP_UTF8, 0, bytes, (int)read, NULL, 0);
        energy_file = n > 0 ? dyn_alloc((SIZE_T)(n + 1) * sizeof(WCHAR)) : NULL;
        if (energy_file) MultiByteToWideChar(CP_UTF8, 0, bytes, (int)read, energy_file, n);
    }
    if (bytes) HeapFree(GetProcessHeap(), 0, bytes);
    CloseHandle(file);
}

static void each_energy_choice(void (*visit)(BYTE choice, const WCHAR *name)) {
    for (WCHAR *line = energy_file; line && *line;) {
        WCHAR *end = line;
        while (*end && *end != '\n') ++end;
        WCHAR *stop = end > line && end[-1] == '\r' ? end - 1 : end;
        if (stop - line > 2 && stop - line < 66 && (line[0] == 'E' || line[0] == 'M') && line[1] == '|') {
            WCHAR name[64];
            int n = (int)(stop - line - 2);
            for (int i = 0; i < n; ++i) name[i] = line[2 + i];
            name[n] = 0;
            visit(line[0] == 'E' ? energy_eco : energy_max, name);
        }
        line = *end ? end + 1 : end;
    }
}

static BOOL save_energy_choices(void) {
    WCHAR path[MAX_PATH], temp[MAX_PATH];
    if (!energy_path(path)) return FALSE;
    lstrcpyW(temp, path);
    lstrcatW(temp, L".tmp");
    HANDLE file = CreateFileW(temp, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) return FALSE;
    BOOL ok = TRUE;
    for (unsigned i = 0; ok && i < dyn_count; ++i) {
        const DynItem *d = dyn_items[i];
        if (d->type != dyn_energy || d->current == energy_normal) continue;
        char line[200];
        int n = wsprintfA(line, "%c|", d->current == energy_eco ? 'E' : 'M');
        n += WideCharToMultiByte(CP_UTF8, 0, d->key, -1, line + n, sizeof line - n - 2, NULL, NULL) - 1;
        line[n++] = '\r';
        line[n++] = '\n';
        DWORD written;
        ok = WriteFile(file, line, (DWORD)n, &written, NULL) && written == (DWORD)n;
    }
    ok = ok && FlushFileBuffers(file);
    CloseHandle(file);
    if (ok) ok = MoveFileExW(temp, path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
    if (!ok) DeleteFileW(temp);
    else load_energy_file();
    return ok;
}

static DynItem *energy_item(const WCHAR *name) {
    for (unsigned i = 0; i < found_count; ++i)
        if (found_items[i]->type == dyn_energy && !lstrcmpiW(found_items[i]->key, name)) return found_items[i];
    return NULL;
}

static void energy_saved_choice(BYTE choice, const WCHAR *name) {
    DynItem *d = energy_item(name);
    if (!d && !energy_skipped(name)) {
        char title[96], detail[200];
        to_ansi(name, title, sizeof title);
        lstrcpyA(detail, "Program tidak berjalan | pilihan berlaku saat program dibuka");
        d = add_item(dyn_energy, tab_energy, name, title, detail, energy_normal, 0, TRUE);
        if (d) lstrcpynW(d->image, name, ARRAYSIZE(d->image));
    }
    if (d) d->current = choice;
}

static void scan_energy(void) {
    load_energy_file();
    unsigned first = found_count;
    for (unsigned i = 0; i < proc_total; ++i) {
        const ProcInfo *p = &proc_list[i];
        if (!p->name[0] || energy_skipped(p->name) || !user_process(p)) continue;
        DynItem *d = NULL;
        for (unsigned j = first; j < found_count && !d; ++j)
            if (!lstrcmpiW(found_items[j]->key, p->name)) d = found_items[j];
        if (!d) {
            char title[96];
            to_ansi(p->name, title, sizeof title);
            HANDLE h = OpenProcess(PROCESS_SET_INFORMATION, FALSE, p->pid);
            if (h) CloseHandle(h);
            d = add_item(dyn_energy, tab_energy, p->name, title, "-", energy_normal, 0, h != NULL);
            if (!d) continue;
            lstrcpynW(d->image, p->name, ARRAYSIZE(d->image));
            d->running = 0;
            d->baseline = 0;
        }
        ++d->running;
        d->weight += p->ws;
        d->place = (BYTE)(d->place + (p->life && p->cpu * 1000 / p->life >= 1));
    }
    for (unsigned j = first; j < found_count; ++j) {
        found_items[j]->place = 0;
        found_items[j]->running = 1;
    }
    each_energy_choice(energy_saved_choice);
}

static void scan_programs(void);
static BOOL refresh_detail(DynItem *d);

typedef LONG (NTAPI *SuspendCall)(HANDLE process);

static BOOL pause_process(DWORD pid, BOOL pause) {
    static SuspendCall suspend, resume;
    if (!suspend) {
        HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
        suspend = (SuspendCall)(void *)GetProcAddress(ntdll, "NtSuspendProcess");
        resume = (SuspendCall)(void *)GetProcAddress(ntdll, "NtResumeProcess");
    }
    if (!suspend || !resume) return FALSE;
    HANDLE h = OpenProcess(PROCESS_SUSPEND_RESUME, FALSE, pid);
    if (!h) return FALSE;
    LONG status = pause ? suspend(h) : resume(h);
    CloseHandle(h);
    return status >= 0;
}

static BOOL pause_forbidden(const ProcInfo *p) {
    if (never_closed(p->name) || p->pid == GetCurrentProcessId()) return TRUE;
    static const WCHAR *const extra[] = { L"explorer.exe", L"ShellExperienceHost.exe", L"StartMenuExperienceHost.exe",
        L"SearchHost.exe", L"TextInputHost.exe", L"ctfmon.exe", L"sihost.exe", L"taskhostw.exe", L"RuntimeBroker.exe",
        L"winlogon.exe", L"LogonUI.exe", L"powershell.exe", L"cmd.exe", L"WindowsTerminal.exe", L"Taskmgr.exe" };
    for (unsigned i = 0; i < ARRAYSIZE(extra); ++i) if (!lstrcmpiW(p->name, extra[i])) return TRUE;
    return critical_process(p->pid);
}

static unsigned pause_run(DynItem *d, BOOL run) {
    if (d->current == (DWORD)run) return no_change;
    if (!run) {
        WCHAR text[400];
        wsprintfW(text, L"Bekukan %s?\r\n\r\nCPU-nya jadi 0%% dan program berhenti merespons sampai kamu ON-kan lagi. "
                        L"Program tidak ditutup dan datanya tidak hilang.\r\n\r\n"
                        L"Jangan bekukan program yang sedang kamu pakai (game, call, download).", d->key);
        if (MessageBoxW(FindWindowW(L"OffOrOnWindow", NULL), text, L"Jeda CPU",
                        MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2 | MB_SETFOREGROUND) != IDYES) return cancelled;
    }
    snapshot_processes(FALSE);
    unsigned done = 0, failed_count = 0;
    for (unsigned i = 0; i < proc_total; ++i) {
        const ProcInfo *p = &proc_list[i];
        if (lstrcmpiW(p->name, d->key) || pause_forbidden(p)) continue;
        if (pause_process(p->pid, !run)) ++done; else ++failed_count;
    }
    if (!done) return failed_count ? change_failed : no_change;
    d->current = run ? 1 : 0;
    return failed_count ? change_failed : changed_live;
}

static void scan_pause(void) {
    unsigned first = found_count;
    for (unsigned i = 0; i < proc_total; ++i) {
        const ProcInfo *p = &proc_list[i];
        if (!p->name[0] || pause_forbidden(p)) continue;
        DynItem *d = NULL;
        for (unsigned j = first; j < found_count && !d; ++j)
            if (!lstrcmpiW(found_items[j]->key, p->name)) d = found_items[j];
        if (!d) {
            char name[96], title[128];
            to_ansi(p->name, name, sizeof name);
            wsprintfA(title, "Jeda CPU: %s", name);
            HANDLE h = OpenProcess(PROCESS_SUSPEND_RESUME, FALSE, p->pid);
            if (h) CloseHandle(h);
            d = add_item(dyn_stop, tab_cpu, p->name, title, "-", 1, 0, h != NULL);
            if (!d) continue;
            d->place = 3;
            d->running = 1;
            lstrcpynW(d->image, p->name, ARRAYSIZE(d->image));
        }
        d->weight += (ULONGLONG)p->pct * 1000000ull + (p->ws >> 20);
    }

    unsigned kept = first;
    for (unsigned j = first; j < found_count; ++j) {
        DynItem *d = found_items[j];
        if (!d->locked || d->weight >= 1000000ull || stopped_has('Z', d->key)) found_items[kept++] = d;
        else { HeapFree(GetProcessHeap(), 0, d->key); HeapFree(GetProcessHeap(), 0, d->title);
               HeapFree(GetProcessHeap(), 0, d->detail); HeapFree(GetProcessHeap(), 0, d); }
    }
    found_count = kept;
}

static unsigned stopped_first;

static void stopped_visit(WCHAR kind, const WCHAR *name, const WCHAR *path) {
    for (unsigned j = 0; j < found_count; ++j) {
        DynItem *d = found_items[j];
        if (d->type != dyn_stop || lstrcmpiW(d->key, name)) continue;
        if ((kind == 'S') != (d->place == 0) || (kind == 'Z') != (d->place == 3)) continue;
        d->current = 0;
        if (!d->path && path) d->path = dup_wide(path);
        return;
    }
    char text[300], title[340];
    to_ansi(name, text, sizeof text);
    if (kind == 'Z') return;
    wsprintfA(title, kind == 'S' ? "Hentikan: %s" : kind == 'W' ? "Tutup (Windows): %s" : "Tutup: %s", text);
    DynItem *d = add_item(dyn_stop, kind == 'S' ? tab_cpu : tab_ram, name, title, "-", 0, 0, kind != 'P' ? dyn_elevated : TRUE);
    if (!d) return;
    d->place = kind == 'S' ? 0 : kind == 'P' ? 1 : 2;
    d->running = 0;
    d->baseline = 1;
    lstrcpynW(d->image, name, ARRAYSIZE(d->image));
    if (path) d->path = dup_wide(path);
}

static void scan_programs(void) {
    unsigned first = found_count;
    load_restart_file();
    stopped_first = first;
    for (unsigned i = 0; i < proc_total; ++i) {
        const ProcInfo *p = &proc_list[i];
        if (!p->name[0] || energy_skipped(p->name) || !user_process(p)) continue;
        DynItem *d = NULL;
        for (unsigned j = first; j < found_count && !d; ++j)
            if (!lstrcmpiW(found_items[j]->key, p->name)) d = found_items[j];
        if (!d) {
            char name[96], title[128];
            to_ansi(p->name, name, sizeof name);
            wsprintfA(title, "Tutup: %s", name);
            d = add_item(dyn_stop, tab_ram, p->name, title, "-", 1, 0, TRUE);
            if (!d) continue;
            d->place = 1;
            d->running = 1;
            lstrcpynW(d->image, p->name, ARRAYSIZE(d->image));
            if (restarts_automatically(p->name)) {
                d->restarts = 1;
                d->locked = 1;
            }
        }
        if (!d->path) {
            HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, p->pid);
            WCHAR path[MAX_PATH];
            DWORD size = ARRAYSIZE(path);
            if (h && QueryFullProcessImageNameW(h, 0, path, &size)) d->path = dup_wide(path);
            if (h) CloseHandle(h);
        }
        d->weight += p->ws;
        d->pid += p->pct;
    }

    for (unsigned i = 0; i < proc_total; ++i) {
        const ProcInfo *p = &proc_list[i];
        if (!p->name[0] || p->pid == GetCurrentProcessId() || user_process(p) || never_closed(p->name)) continue;
        if (p->ws < (24ull << 20) && p->pct < 5) continue;
        DynItem *d = NULL;
        for (unsigned j = first; j < found_count && !d; ++j)
            if (found_items[j]->place == 2 && !lstrcmpiW(found_items[j]->key, p->name)) d = found_items[j];
        if (!d) {
            char name[96], title[128];
            to_ansi(p->name, name, sizeof name);
            wsprintfA(title, "Tutup (Windows): %s", name);
            HANDLE h = OpenProcess(PROCESS_TERMINATE, FALSE, p->pid);
            BOOL closable = h && !critical_process(p->pid);
            if (h) CloseHandle(h);
            d = add_item(dyn_stop, tab_ram, p->name, title, "-", 1, 0, closable && dyn_elevated);
            if (!d) continue;
            d->place = 2;
            d->running = 1;
            lstrcpynW(d->image, p->name, ARRAYSIZE(d->image));
            if (restarts_automatically(p->name)) { d->restarts = 1; d->locked = 1; }
        }
        d->weight += p->ws;
        d->pid += p->pct;
    }

    for (unsigned j = first; j < found_count; ++j) {
        DynItem *d = found_items[j];
        if (d->pid >= 5) {
            d->group = tab_cpu;
            d->weight = d->pid;
        }
        d->pid = 0;
    }
}

typedef struct { DWORD pid; WCHAR name[64]; BYTE mode; DWORD priority; } EnergyApplied;
enum { applied_limit = 1024 };
static EnergyApplied applied[applied_limit];
static unsigned applied_count;
unsigned o3_eco_count;
ULONGLONG energy_saved;
static void usage_of(const WCHAR *image, unsigned *count, ULONGLONG *memory, unsigned *pct);

static BOOL set_energy(DWORD pid, BYTE mode, DWORD *priority) {
    HANDLE h = OpenProcess(PROCESS_SET_INFORMATION | PROCESS_QUERY_LIMITED_INFORMATION |
                           PROCESS_SET_QUOTA | PROCESS_QUERY_INFORMATION, FALSE, pid);
    if (!h) return FALSE;
    if (priority && !*priority) *priority = GetPriorityClass(h);

    PROCESS_POWER_THROTTLING_STATE throttle = { PROCESS_POWER_THROTTLING_CURRENT_VERSION,
        mode == energy_normal ? 0 : PROCESS_POWER_THROTTLING_EXECUTION_SPEED,
        mode == energy_eco ? PROCESS_POWER_THROTTLING_EXECUTION_SPEED : 0 };
    BOOL ok = SetProcessInformation(h, ProcessPowerThrottling, &throttle, sizeof throttle);
    DWORD wanted = mode == energy_eco ? IDLE_PRIORITY_CLASS : mode == energy_max ? HIGH_PRIORITY_CLASS
                 : priority && *priority ? *priority : NORMAL_PRIORITY_CLASS;
    ok = SetPriorityClass(h, wanted) && ok;

    MEMORY_PRIORITY_INFORMATION memory = { mode == energy_eco ? MEMORY_PRIORITY_VERY_LOW : MEMORY_PRIORITY_NORMAL };
    SetProcessInformation(h, ProcessMemoryPriority, &memory, sizeof memory);
    if (mode == energy_eco) K32EmptyWorkingSet(h);
    CloseHandle(h);
    return ok;
}

static BYTE energy_choice(const WCHAR *name) {
    for (unsigned i = 0; i < dyn_count; ++i) {
        const DynItem *d = dyn_items[i];
        if (d->type == dyn_energy && !lstrcmpiW(d->key, name)) return (BYTE)d->current;
    }
    return energy_normal;
}

static BYTE energy_wanted(const ProcInfo *p, const WCHAR *foreground) {
    if (!user_process(p) || energy_skipped(p->name)) return energy_normal;
    BYTE choice = energy_choice(p->name);
    if (choice == energy_eco && !lstrcmpiW(p->name, foreground)) return energy_normal;
    return choice;
}

static void enforce_energy(void) {
    DWORD foreground_pid = 0;
    WCHAR foreground[64] = L"";
    GetWindowThreadProcessId(GetForegroundWindow(), &foreground_pid);
    const ProcInfo *front = process_by_pid(foreground_pid);
    if (front) lstrcpyW(foreground, front->name);
    for (unsigned i = 0; i < applied_count;) {
        EnergyApplied *a = &applied[i];
        const ProcInfo *p = process_by_pid(a->pid);
        BOOL same = p && !lstrcmpiW(p->name, a->name);
        BYTE wanted = same ? energy_wanted(p, foreground) : energy_normal;
        if (same && wanted == a->mode) { ++i; continue; }
        if (same && wanted != energy_normal) { set_energy(a->pid, wanted, &a->priority); a->mode = wanted; ++i; continue; }
        if (same) set_energy(a->pid, energy_normal, &a->priority);
        applied[i] = applied[--applied_count];
    }
    for (unsigned i = 0; i < proc_total && applied_count < applied_limit; ++i) {
        const ProcInfo *p = &proc_list[i];
        BOOL listed = FALSE;
        for (unsigned j = 0; j < applied_count && !listed; ++j) listed = applied[j].pid == p->pid;
        if (listed) continue;
        BYTE wanted = energy_wanted(p, foreground);
        if (wanted == energy_normal) continue;
        EnergyApplied *a = &applied[applied_count];
        a->pid = p->pid;
        a->priority = 0;
        lstrcpyW(a->name, p->name);
        a->mode = wanted;
        if (set_energy(p->pid, wanted, &a->priority)) ++applied_count;
    }
    static unsigned trim_tick;
    if (++trim_tick >= 40) {
        trim_tick = 0;
        for (unsigned i = 0; i < applied_count; ++i) {
            if (applied[i].mode != energy_eco) continue;
            HANDLE h = OpenProcess(PROCESS_SET_QUOTA | PROCESS_QUERY_INFORMATION, FALSE, applied[i].pid);
            if (h) { K32EmptyWorkingSet(h); CloseHandle(h); }
        }
    }
    o3_eco_count = applied_count;
}

static int energy_verify(const DynItem *d) {
    int running = 0;
    for (unsigned i = 0; i < proc_total; ++i) {
        const ProcInfo *p = &proc_list[i];
        if (lstrcmpiW(p->name, d->key) || !user_process(p)) continue;
        HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, p->pid);
        if (!h) return -1;
        DWORD priority = GetPriorityClass(h);
        CloseHandle(h);
        ++running;
        if (d->current == energy_eco && priority != IDLE_PRIORITY_CLASS) return -1;
        if (d->current == energy_max && priority != HIGH_PRIORITY_CLASS) return -1;
        if (d->current == energy_normal && (priority == IDLE_PRIORITY_CLASS || priority == HIGH_PRIORITY_CLASS)) {
            BOOL original = FALSE;
            for (unsigned j = 0; j < applied_count; ++j) original |= applied[j].pid == p->pid;
            if (!original) continue;
            return -1;
        }
    }
    return running;
}

#define IFEO L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Image File Execution Options\\"

static BOOL persist_energy(const WCHAR *name, BYTE mode) {
    WCHAR path[MAX_PATH];
    if (lstrlenW(name) > 80) return FALSE;
    wsprintfW(path, L"%s%s\\PerfOptions", IFEO, name);
    if (mode == energy_normal) {
        DWORD mark = 0, size = sizeof mark;
        if (RegGetValueW(HKEY_LOCAL_MACHINE, path, L"O3Priority", RRF_RT_REG_DWORD, NULL, &mark, &size) != ERROR_SUCCESS)
            return TRUE;
        RegDeleteKeyValueW(HKEY_LOCAL_MACHINE, path, L"CpuPriorityClass");
        RegDeleteKeyValueW(HKEY_LOCAL_MACHINE, path, L"O3Priority");
        RegDeleteKeyW(HKEY_LOCAL_MACHINE, path);
        return TRUE;
    }
    HKEY key;
    if (RegCreateKeyExW(HKEY_LOCAL_MACHINE, path, 0, NULL, 0, KEY_SET_VALUE, NULL, &key, NULL) != ERROR_SUCCESS)
        return FALSE;
    DWORD value = mode == energy_eco ? 1 : 3, mark = 1;
    BOOL ok = RegSetValueExW(key, L"CpuPriorityClass", 0, REG_DWORD, (BYTE *)&value, sizeof value) == ERROR_SUCCESS &&
              RegSetValueExW(key, L"O3Priority", 0, REG_DWORD, (BYTE *)&mark, sizeof mark) == ERROR_SUCCESS;
    RegCloseKey(key);
    return ok;
}

static unsigned energy_apply(DynItem *d, unsigned id, unsigned request, unsigned char *state, unsigned char *source) {
    BYTE wanted = request == request_default ? energy_normal : request == request_on ? energy_max : energy_eco;
    BYTE mark;
    if (wanted == d->current) {
        dynamic_check(id, state, source, &mark);
        return no_change;
    }
    BYTE before = (BYTE)d->current;
    d->current = wanted;
    if (!save_energy_choices() || !persist_energy(d->key, wanted)) {
        d->current = before;
        save_energy_choices();
        persist_energy(d->key, before);
        dynamic_check(id, state, source, &mark);
        return record_failed;
    }
    unsigned count_before, pct_before;
    ULONGLONG memory_before;
    snapshot_processes(TRUE);
    usage_of(d->key, &count_before, &memory_before, &pct_before);

    enforce_energy();
    if (wanted == energy_eco) {
        unsigned count_after, pct_after;
        ULONGLONG memory_after;
        snapshot_processes(TRUE);
        usage_of(d->key, &count_after, &memory_after, &pct_after);
        energy_saved = memory_before > memory_after ? memory_before - memory_after : 0;
    } else energy_saved = 0;
    int running = energy_verify(d);
    unsigned result = running < 0 ? change_failed : running ? changed_live : changed_saved;
    if (running < 0) d->locked = 1;
    dynamic_check(id, state, source, &mark);
    return result;
}

unsigned o3_energy_next(unsigned id) {
    DynItem *d = dynamic_item(id);
    if (!d || d->type != dyn_energy) return 99;
    return d->current == energy_normal ? request_off : d->current == energy_eco ? request_on : request_default;
}

static const char type_letters[] = " STRBEX";

static BOOL load_defaults(WCHAR **text) {
    *text = NULL;
    WCHAR path[MAX_PATH];
    if (!initialize_paths() || lstrlenW(backup_folder) + 24 >= MAX_PATH) return FALSE;
    wsprintfW(path, L"%s\\cpu-ram-baseline.txt", backup_folder);
    HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (file == INVALID_HANDLE_VALUE) return GetLastError() == ERROR_FILE_NOT_FOUND;
    LARGE_INTEGER size;
    DWORD read = 0;
    BOOL ok = GetFileSizeEx(file, &size) && size.QuadPart < 8 * 1024 * 1024;
    char *bytes = ok ? dyn_alloc((SIZE_T)size.QuadPart + 1) : NULL;
    ok = bytes && (!size.QuadPart || (ReadFile(file, bytes, size.LowPart, &read, NULL) && read == size.LowPart));
    CloseHandle(file);
    if (ok && read) {
        int n = MultiByteToWideChar(CP_UTF8, 0, bytes, (int)read, NULL, 0);
        *text = n > 0 ? dyn_alloc((SIZE_T)(n + 1) * sizeof(WCHAR)) : NULL;
        ok = *text && MultiByteToWideChar(CP_UTF8, 0, bytes, (int)read, *text, n) == n;
    }
    if (bytes) HeapFree(GetProcessHeap(), 0, bytes);
    return ok;
}

static BOOL saved_default(const WCHAR *text, const DynItem *d, DWORD *value) {
    int key_length = lstrlenW(d->key);
    for (const WCHAR *line = text; line && *line;) {
        const WCHAR *end = line;
        while (*end && *end != '\n') ++end;
        const WCHAR *stop = end > line && end[-1] == '\r' ? end - 1 : end;
        if (stop - line > 4 && line[0] == (WCHAR)type_letters[d->type] && line[1] == '|') {
            const WCHAR *p = line + 2;
            DWORD v = 0;
            while (p < stop && *p >= '0' && *p <= '9') v = v * 10 + (DWORD)(*p++ - '0');
            if (p < stop && *p == '|' && stop - (p + 1) == key_length &&
                CompareStringOrdinal(p + 1, key_length, d->key, key_length, FALSE) == CSTR_EQUAL) {
                *value = v;
                return TRUE;
            }
        }
        line = *end ? end + 1 : end;
    }
    return FALSE;
}

static BOOL append_defaults(const BYTE *is_new) {
    SIZE_T size = 1;
    unsigned count = 0;
    for (unsigned i = 0; i < found_count; ++i)
        if (is_new[i]) { size += 32 + (SIZE_T)lstrlenW(found_items[i]->key) * 3; ++count; }
    if (!count) return TRUE;
    char *text = dyn_alloc(size);
    if (!text) return FALSE;
    SIZE_T used = 0;
    for (unsigned i = 0; i < found_count; ++i) {
        if (!is_new[i]) continue;
        const DynItem *d = found_items[i];
        used += wsprintfA(text + used, "%c|%u|", type_letters[d->type], d->baseline);
        used += WideCharToMultiByte(CP_UTF8, 0, d->key, -1, text + used, (int)(size - used), NULL, NULL) - 1;
        text[used++] = '\r';
        text[used++] = '\n';
    }
    WCHAR path[MAX_PATH];
    wsprintfW(path, L"%s\\cpu-ram-baseline.txt", backup_folder);
    HANDLE file = CreateFileW(path, FILE_APPEND_DATA, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    DWORD written = 0;
    BOOL ok = file != INVALID_HANDLE_VALUE && WriteFile(file, text, (DWORD)used, &written, NULL) &&
              written == used && FlushFileBuffers(file);
    if (file != INVALID_HANDLE_VALUE) CloseHandle(file);
    HeapFree(GetProcessHeap(), 0, text);
    return ok;
}

static BOOL dyn_on(BYTE type, DWORD value) {
    switch (type) {
        case dyn_service: return value != SERVICE_DISABLED;
        case dyn_startup: return !value || !(value & 1);
        default: return value != 0;
    }
}

static BOOL comes_before(const DynItem *a, const DynItem *b) {
    if (a->group != b->group) return a->group < b->group;
    if (a->weight != b->weight) return a->weight > b->weight;
    if (a->type != b->type) return a->type < b->type;
    return lstrcmpiA(a->title, b->title) < 0;
}

static void dynamic_scan(void) {
    HRESULT com = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (!found_items) found_items = dyn_alloc(sizeof(DynItem *) * dynamic_limit);
    found_count = 0;
    if (!found_items) goto done;
    dyn_elevated = process_elevated();
    snapshot_processes(TRUE);
    scan_services();
    scan_tasks();
    scan_startup();
    scan_background();
    scan_energy();
    scan_programs();
    load_stopped_file();
    scan_pause();
    each_stopped(stopped_visit);

    WCHAR *saved = NULL;
    BYTE *is_new = dyn_alloc(found_count + 1);
    BOOL ok = is_new && load_defaults(&saved);
    for (unsigned i = 0; ok && i < found_count; ++i) {
        DWORD value;
        if (found_items[i]->type == dyn_stop) found_items[i]->baseline = 1;
        if (found_items[i]->type == dyn_energy || found_items[i]->type == dyn_stop) continue;
        if (saved && saved_default(saved, found_items[i], &value)) found_items[i]->baseline = value;
        else is_new[i] = 1;
    }
    ok = ok && append_defaults(is_new);
    for (unsigned i = 0; !ok && i < found_count; ++i) found_items[i]->locked = 2;
    if (saved) HeapFree(GetProcessHeap(), 0, saved);
    if (is_new) HeapFree(GetProcessHeap(), 0, is_new);

    for (unsigned i = 1; i < found_count; ++i) {
        DynItem *d = found_items[i];
        unsigned j = i;
        for (; j > 0 && comes_before(d, found_items[j - 1]); --j) found_items[j] = found_items[j - 1];
        found_items[j] = d;
    }
done:;
    unsigned cpu = 0;
    for (unsigned i = 0; i < found_count; ++i) {
        dyn_items[i] = found_items[i];
        catalogue[item_count + i].title = found_items[i]->title;
        catalogue[item_count + i].detail = found_items[i]->detail;
        if (found_items[i]->group == tab_cpu) ++cpu;
    }
    dyn_count = found_count;
    for (unsigned i = 0; i < found_count; ++i) refresh_detail(found_items[i]);
    catalogue_starts[tab_cpu] = item_count;
    catalogue_counts[tab_cpu] = cpu;
    unsigned energy = 0;
    for (unsigned i = 0; i < found_count; ++i) if (found_items[i]->group == tab_energy) ++energy;
    catalogue_starts[tab_ram] = item_count + cpu;
    catalogue_counts[tab_ram] = found_count - cpu - energy;
    catalogue_starts[tab_energy] = item_count + found_count - energy;
    catalogue_counts[tab_energy] = energy;
    MemoryBarrier();
    o3_item_total = item_count + found_count;
    if (SUCCEEDED(com)) CoUninitialize();
}

static DynItem *dynamic_item(unsigned id) {
    return id > item_count && id - item_count <= dyn_count ? dyn_items[id - item_count - 1] : NULL;
}

static unsigned dynamic_check(unsigned id, unsigned char *state, unsigned char *source, BYTE *mark) {
    DynItem *d = dynamic_item(id);
    *mark = 0;
    if (!d) { *state = unavailable; *source = 0; return unsupported; }
    if (d->type == dyn_energy) {
        *source = (unsigned char)(8 + d->type);
        *state = d->current == energy_max ? on : off;
        if (d->current == energy_normal) *mark = *state = on;
        return d->locked ? read_only : live_toggle;
    }
    *source = (unsigned char)(8 + d->type);

    BOOL shown = dyn_on(d->type, d->current);
    *state = shown ? on : off;
    if (d->locked == 2) { *state = failed; return unreadable; }
    if (d->current == d->baseline) *mark = *state;
    if (d->locked) return read_only;
    if (d->type == dyn_stop) return live_toggle;
    return d->type == dyn_service || d->type == dyn_task ? live_toggle : saved_toggle;
}

static BOOL dynamic_read(DynItem *d, DWORD *value) {
    switch (d->type) {
        case dyn_service: return service_read(d->key, value);
        case dyn_task: return task_access(d->key, -1, value);
        case dyn_startup: {
            const WCHAR *name = d->key;
            while (*name && *name != ':') ++name;
            return *name && startup_read(&startup_places[d->place], name + 1, value);
        }
        default: return background_read(d->key, value);
    }
}

static BOOL dynamic_write(DynItem *d, DWORD value, BOOL *live) {
    switch (d->type) {
        case dyn_service: return service_write(d->key, value, live);
        case dyn_task: {
            DWORD now;
            *live = TRUE;
            return task_access(d->key, value ? 1 : 0, &now);
        }
        case dyn_startup: {
            const WCHAR *name = d->key;
            while (*name && *name != ':') ++name;
            return *name && startup_write(&startup_places[d->place], name + 1, value);
        }
        default: return background_write(d->key, value);
    }
}

static BOOL close_running_program(const DynItem *d) {
    if (!d->image[0] || !lstrcmpiW(d->image, L"explorer.exe")) return FALSE;
    snapshot_processes(TRUE);
    DWORD session = 0, own = 0;
    ProcessIdToSessionId(GetCurrentProcessId(), &own);
    unsigned count = 0;
    ULONGLONG memory = 0;
    for (unsigned i = 0; i < proc_total; ++i)
        if (!lstrcmpiW(proc_list[i].name, d->image) && ProcessIdToSessionId(proc_list[i].pid, &session) && session == own) {
            ++count;
            memory += proc_list[i].ws;
        }
    if (!count) return FALSE;
    WCHAR text[400];
    wsprintfW(text, L"Startup %s sudah dimatikan untuk login berikutnya.\r\n\r\n"
                    L"%s masih berjalan (%u proses, %u MB RAM). Tutup sekarang agar RAM langsung lega?\r\n"
                    L"Pekerjaan yang belum disimpan di aplikasi itu akan hilang.",
              d->image, d->image, count, (unsigned)(memory >> 20));
    HWND owner = FindWindowW(L"OffOrOnWindow", NULL);
    if (MessageBoxW(owner, text, L"Tutup program sekarang?",
                    MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON1 | MB_SETFOREGROUND) != IDYES) return FALSE;
    BOOL closed = TRUE;
    for (unsigned i = 0; i < proc_total; ++i) {
        if (lstrcmpiW(proc_list[i].name, d->image) || !ProcessIdToSessionId(proc_list[i].pid, &session) || session != own)
            continue;
        HANDLE h = OpenProcess(PROCESS_TERMINATE | SYNCHRONIZE, FALSE, proc_list[i].pid);
        if (!h) { closed = FALSE; continue; }
        if (!TerminateProcess(h, 0) && GetLastError() != ERROR_ACCESS_DENIED) closed = FALSE;
        WaitForSingleObject(h, 2000);
        CloseHandle(h);
    }
    return closed;
}

unsigned o3_item_kind(unsigned id) {
    if (id <= item_count || id - item_count > dyn_count) return 0;
    const DynItem *d = dyn_items[id - item_count - 1];
    if (d->type == dyn_stop) return d->place == 0 ? 6 : d->place == 1 ? 7 : d->place == 3 ? 9 : 8;
    return d->type;
}

const char *o3_item_key(unsigned id) {
    static char text[300];
    if (id <= item_count || id - item_count > dyn_count) return "";
    const DynItem *d = dyn_items[id - item_count - 1];
    const WCHAR *key = d->type == dyn_startup && d->image[0] ? d->image : d->key;
    if (d->type == dyn_task) {
        const WCHAR *last = key;
        for (const WCHAR *p = key; *p; ++p) if (*p == '\\') last = p + 1;
        key = last;
    }
    to_ansi(key, text, sizeof text);
    return text;
}

static BOOL usage_all;

static void usage_of(const WCHAR *image, unsigned *count, ULONGLONG *memory, unsigned *pct) {
    *count = *pct = 0;
    *memory = 0;
    if (!image[0]) return;
    for (unsigned i = 0; i < proc_total; ++i) {
        const ProcInfo *p = &proc_list[i];
        if (lstrcmpiW(p->name, image) || (!usage_all && !user_process(p))) continue;
        ++*count;
        *memory += p->ws;
        *pct += p->pct;
    }
}

static void usage_text(char *out, ULONGLONG memory, unsigned pct) {
    wsprintfA(out, "%u MB RAM | CPU %u.%u%%", (unsigned)(memory >> 20), pct / 10, pct % 10);
}

static BOOL refresh_detail(DynItem *d) {
    char text[detail_size], usage[64], name[160];
    unsigned count, pct;
    ULONGLONG memory;
    if (!d->detail) return FALSE;
    if (d->type == dyn_service || (d->type == dyn_stop && d->place == 0)) {
        to_ansi(d->key, name, sizeof name);
        const ProcInfo *p = d->running && d->pid ? process_by_pid(d->pid) : NULL;
        if (!d->running)
            wsprintfA(text, d->type == dyn_stop ? "Service %s | dihentikan sampai restart atau dipanggil lagi"
                                                : "Service %s | tidak berjalan, dapat dipicu otomatis", name);
        else if (p) {
            usage_text(usage, p->ws, p->pct);
            if (d->restarts) wsprintfA(text, "Service %s | dijalankan ulang otomatis oleh Windows | %s", name, usage);
            else if (d->sharing <= 1) wsprintfA(text, "Service %s | berjalan | %s", name, usage);
            else wsprintfA(text, "Service %s | svchost bersama %u service | %s", name, d->sharing, usage);
        } else wsprintfA(text, "Service %s | berjalan", name);
    } else if (d->type == dyn_startup) {
        to_ansi(d->image[0] ? d->image : L"shortcut", name, sizeof name);
        usage_of(d->image, &count, &memory, &pct);
        if (count) {
            usage_text(usage, memory, pct);
            wsprintfA(text, dyn_on(d->type, d->current) ? "Startup %s | %s | berjalan | %s"
                                                         : "Startup %s | %s | OFF, tapi sedang dibuka manual | %s",
                      startup_places[d->place].label, name, usage);
        } else wsprintfA(text, "Startup %s | %s | tidak berjalan", startup_places[d->place].label, name);
    } else if (d->type == dyn_energy || d->type == dyn_stop) {
        usage_all = d->type == dyn_stop && d->place >= 2;
        usage_of(d->key, &count, &memory, &pct);
        usage_all = FALSE;
        if (count) {
            usage_text(usage, memory, pct);
            if (d->type == dyn_stop && d->place == 3)
                wsprintfA(text, d->current ? "Berjalan | %u proses | %s" : "DIBEKUKAN | %u proses | %s | CPU 0%%, klik untuk lanjut", count, usage);
            else if (d->type == dyn_stop && d->restarts)
                wsprintfA(text, "Dijalankan ulang otomatis oleh Windows/aplikasi lain | %s", usage);
            else if (d->type == dyn_stop && d->place == 2)
                wsprintfA(text, "Proses Windows/driver | %u proses | %s", count, usage);
            else if (d->type == dyn_energy && d->current == energy_eco)
                wsprintfA(text, "ECO aktif | %u proses | %s | RAM dilepas tiap 1 menit 20 detik", count, usage);
            else if (d->type == dyn_energy && d->current == energy_max)
                wsprintfA(text, "MAX aktif | %u proses | %s | prioritas High", count, usage);
            else wsprintfA(text, "Program berjalan | %u proses | %s", count, usage);
        } else lstrcpyA(text, d->type == dyn_energy ? "Program tidak berjalan | pilihan berlaku saat program dibuka"
                                                    : "Program ditutup | klik lagi untuk membuka ulang");
    } else return FALSE;
    if (!lstrcmpA(text, d->detail)) return FALSE;
    lstrcpynA(d->detail, text, detail_size);
    return TRUE;
}

static void live_services(void) {
    DWORD count = 0;
    ENUM_SERVICE_STATUS_PROCESSW *list = enum_services(&count);
    if (!list) return;
    for (unsigned i = 0; i < dyn_count; ++i) {
        DynItem *d = dyn_items[i];
        if (d->type != dyn_service && !(d->type == dyn_stop && d->place == 0)) continue;
        for (DWORD j = 0; j < count; ++j) {
            if (lstrcmpiW(list[j].lpServiceName, d->key)) continue;
            DWORD pid = list[j].ServiceStatusProcess.dwProcessId;
            BOOL running = list[j].ServiceStatusProcess.dwCurrentState == SERVICE_RUNNING && pid;
            unsigned sharing = running ? services_in_process(list, count, pid) : 0;
            d->running = (BYTE)running;
            d->pid = pid;
            d->sharing = (BYTE)(sharing > 255 ? 255 : sharing);
            if (d->type == dyn_stop) d->current = running && !stopped_has('S', d->key) ? 1 : 0;
            break;
        }
    }
}

static unsigned service_run(DynItem *d, BOOL run) {
    SC_HANDLE scm = OpenSCManagerW(NULL, NULL, SC_MANAGER_CONNECT);
    if (!scm) return change_failed;
    SC_HANDLE s = OpenServiceW(scm, d->key, SERVICE_QUERY_STATUS | SERVICE_STOP | SERVICE_START);
    unsigned result = change_failed;
    SERVICE_STATUS status;
    if (s && QueryServiceStatus(s, &status)) {
        BOOL now = status.dwCurrentState == SERVICE_RUNNING;
        if (now == run) result = no_change;
        else {
            BOOL sent = run ? StartServiceW(s, 0, NULL) || GetLastError() == ERROR_SERVICE_ALREADY_RUNNING
                            : ControlService(s, SERVICE_CONTROL_STOP, &status) || GetLastError() == ERROR_SERVICE_NOT_ACTIVE;
            DWORD wanted = run ? SERVICE_RUNNING : SERVICE_STOPPED;
            for (unsigned wait = 0; sent && wait < 80; ++wait) {
                if (!QueryServiceStatus(s, &status) || status.dwCurrentState == wanted) break;
                Sleep(100);
            }
            if (sent && QueryServiceStatus(s, &status) && status.dwCurrentState == wanted) result = changed_live;
            for (unsigned wait = 0; !run && result == changed_live && wait < 30; ++wait) {
                Sleep(100);
                if (QueryServiceStatus(s, &status) && status.dwCurrentState != SERVICE_STOPPED &&
                    status.dwCurrentState != SERVICE_STOP_PENDING) result = respawned;
            }
        }
        if (QueryServiceStatus(s, &status)) {
            d->running = (BYTE)(status.dwCurrentState == SERVICE_RUNNING);
            d->current = d->running;
        }
    }
    if (s) CloseServiceHandle(s);
    CloseServiceHandle(scm);
    return result;
}

static unsigned program_run(DynItem *d, BOOL run) {
    unsigned count, pct;
    ULONGLONG memory;
    snapshot_processes(TRUE);
    usage_all = d->place == 2;
    usage_of(d->key, &count, &memory, &pct);
    usage_all = FALSE;
    if (d->place == 2) {
        if (run) return count ? no_change : cannot_change;
        if (!count) return no_change;
        WCHAR text[400], image[64];
        lstrcpynW(image, d->key, ARRAYSIZE(image));
        wsprintfW(text, L"%s adalah proses milik Windows/driver (%u proses, %u MB RAM).\r\n\r\n"
                        L"Menutupnya bisa membuat fitur terkait berhenti sampai Windows menjalankannya lagi "
                        L"atau sampai PC restart. Tutup sekarang?", image, count, (unsigned)(memory >> 20));
        if (MessageBoxW(FindWindowW(L"OffOrOnWindow", NULL), text, L"Tutup proses Windows",
                        MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2 | MB_SETFOREGROUND) != IDYES) return cancelled;
        for (unsigned i = 0; i < proc_total; ++i) {
            const ProcInfo *p = &proc_list[i];
            if (lstrcmpiW(p->name, d->key) || critical_process(p->pid)) continue;
            HANDLE h = OpenProcess(PROCESS_TERMINATE | SYNCHRONIZE, FALSE, p->pid);
            if (!h) continue;
            TerminateProcess(h, 0);
            WaitForSingleObject(h, 2000);
            CloseHandle(h);
        }
        Sleep(1500);
        snapshot_processes(TRUE);
        usage_all = TRUE;
        usage_of(d->key, &count, &memory, &pct);
        usage_all = FALSE;
        d->current = count ? 1 : 0;
        return count ? respawned : changed_live;
    }
    d->current = count ? 1 : 0;
    if (run) {
        if (count) return no_change;
        if (!d->path) return change_failed;

        WCHAR args[MAX_PATH + 4];
        wsprintfW(args, L"\"%s\"", d->path);
        HRESULT com = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
        BOOL started = (INT_PTR)ShellExecuteW(NULL, L"open", L"explorer.exe", args, NULL, SW_SHOWNORMAL) > 32;
        if (SUCCEEDED(com)) CoUninitialize();
        for (unsigned wait = 0; started && !count && wait < 80; ++wait) {
            Sleep(100);
            snapshot_processes(FALSE);
            usage_of(d->key, &count, &memory, &pct);
        }
        d->current = count ? 1 : 0;
        return count ? changed_live : change_failed;
    }
    if (!count) return no_change;
    WCHAR text[400], image[64];
    lstrcpynW(image, d->key, ARRAYSIZE(image));
    wsprintfW(text, L"Tutup %s sekarang?\r\n\r\n%u proses, %u MB RAM akan dilepas.\r\n"
                    L"Pekerjaan yang belum disimpan di program itu akan hilang.",
              image, count, (unsigned)(memory >> 20));
    if (MessageBoxW(FindWindowW(L"OffOrOnWindow", NULL), text, L"Tutup program",
                    MB_YESNO | MB_ICONQUESTION | MB_SETFOREGROUND) != IDYES) return cancelled;
    DWORD closed[128];
    unsigned closed_count = 0;
    for (unsigned i = 0; i < proc_total; ++i) {
        const ProcInfo *p = &proc_list[i];
        if (lstrcmpiW(p->name, d->key) || !user_process(p)) continue;
        HANDLE h = OpenProcess(PROCESS_TERMINATE | SYNCHRONIZE, FALSE, p->pid);
        if (!h) continue;
        if (TerminateProcess(h, 0) && closed_count < ARRAYSIZE(closed)) closed[closed_count++] = p->pid;
        WaitForSingleObject(h, 2000);
        CloseHandle(h);
    }

    for (unsigned wait = 0; wait < 30; ++wait) {
        Sleep(100);
        snapshot_processes(FALSE);
        for (unsigned i = 0; i < proc_total; ++i) {
            const ProcInfo *p = &proc_list[i];
            if (lstrcmpiW(p->name, d->key) || !user_process(p)) continue;
            BOOL old = FALSE;
            for (unsigned k = 0; k < closed_count && !old; ++k) old = closed[k] == p->pid;
            if (old) continue;
            d->current = 1;
            return respawned;
        }
    }
    snapshot_processes(TRUE);
    usage_of(d->key, &count, &memory, &pct);
    d->current = count ? 1 : 0;
    return count ? change_failed : changed_live;
}

static unsigned dynamic_apply(unsigned id, unsigned request, unsigned char *state, unsigned char *source) {
    DynItem *d = dynamic_item(id);
    if (!d || d->locked) return cannot_change;
    if (d->type == dyn_energy) return energy_apply(d, id, request, state, source);
    if (d->type == dyn_stop) {
        BOOL run = request == request_on || (request == request_default && d->baseline);
        unsigned result = d->place == 0 ? service_run(d, run) : d->place == 3 ? pause_run(d, run) : program_run(d, run);
        if (result == changed_live || result == no_change)
            save_stopped(d->place == 0 ? 'S' : d->place == 1 ? 'P' : d->place == 3 ? 'Z' : 'W', d->key, d->path, !run);
        if (result == change_failed) d->locked = 2;
        if (result == respawned) remember_restart(d);
        refresh_detail(d);
        BYTE mark;
        dynamic_check(id, state, source, &mark);
        return result;
    }
    static const DWORD on_values[] = {0, SERVICE_DEMAND_START, 1, 2, 1};
    static const DWORD off_values[] = {0, SERVICE_DISABLED, 0, 3, 0};
    BOOL base_on = dyn_on(d->type, d->baseline);
    DWORD target = request == request_default ? d->baseline
                 : request == request_on ? (base_on ? d->baseline : on_values[d->type])
                 : (base_on ? off_values[d->type] : d->baseline);
    DWORD now;
    if (!dynamic_read(d, &now)) {
        d->locked = 2;
        return cannot_change;
    }
    d->current = now;
    unsigned result = no_change;
    if (now != target) {
        BOOL live = FALSE;
        BOOL written = dynamic_write(d, target, &live);
        if (dynamic_read(d, &now)) d->current = now;
        if (!written || d->current != target) {
            d->locked = 2;
            result = change_failed;
        } else result = live ? changed_live : changed_saved;
    }

    if (d->type == dyn_startup && !dyn_on(d->type, target) && d->running && close_running_program(d)) {
        d->running = 0;
        if (result != change_failed) result = changed_live;
    }
    BYTE mark;
    dynamic_check(id, state, source, &mark);
    return result;
}

void o3_release_efficiency(void) {

    applied_count = o3_eco_count = 0;
}

static void energy_release_all(void) {
    snapshot_processes(FALSE);
    for (unsigned i = 0; i < applied_count; ++i) {
        const ProcInfo *p = process_by_pid(applied[i].pid);
        if (p && !lstrcmpiW(p->name, applied[i].name)) set_energy(applied[i].pid, energy_normal, &applied[i].priority);
    }
    applied_count = o3_eco_count = 0;
}

void o3_self_energy(BOOL trim) {
    static BOOL done;
    if (!done) {
        PROCESS_POWER_THROTTLING_STATE throttle = { PROCESS_POWER_THROTTLING_CURRENT_VERSION,
            PROCESS_POWER_THROTTLING_EXECUTION_SPEED, PROCESS_POWER_THROTTLING_EXECUTION_SPEED };
        SetProcessInformation(GetCurrentProcess(), ProcessPowerThrottling, &throttle, sizeof throttle);
        SetPriorityClass(GetCurrentProcess(), BELOW_NORMAL_PRIORITY_CLASS);
        done = TRUE;
    }
    if (trim) K32EmptyWorkingSet(GetCurrentProcess());
}

static BOOL dynamic_light_tick(void) {
    static HWND window;
    static BOOL trimmed;
    if (!window) window = FindWindowW(L"OffOrOnWindow", NULL);
    if (!window || !IsIconic(window)) { trimmed = FALSE; return FALSE; }
    snapshot_processes(FALSE);
    enforce_energy();
    if (!trimmed) { o3_self_energy(TRUE); trimmed = TRUE; }
    return TRUE;
}

static void enforce_stopped(void);
static BOOL dynamic_refresh(unsigned char *states, unsigned char *sources, unsigned char *availability) {
    BOOL changed = FALSE;
    snapshot_processes(TRUE);
    live_services();
    for (unsigned i = 0; i < dyn_count; ++i) {
        DynItem *d = dyn_items[i];
        if (d->locked != 2) {
            DWORD value;
            if ((d->type == dyn_startup || d->type == dyn_background) && dynamic_read(d, &value)) d->current = value;
            if (d->type == dyn_startup || (d->type == dyn_stop && d->place >= 1)) {
                unsigned count, pct;
                ULONGLONG memory;
                usage_all = d->type == dyn_stop && d->place >= 2;
                usage_of(d->type == dyn_startup ? d->image : d->key, &count, &memory, &pct);
                usage_all = FALSE;
                if (d->type == dyn_startup) d->running = (BYTE)(count != 0);
                else if (d->place == 3) { if (!count) d->current = 1; }
                else if (!(d->current == 0 && stopped_has(d->place == 1 ? 'P' : 'W', d->key) && count))
                    d->current = count ? 1 : 0;
            }
        }
        if (refresh_detail(d)) changed = TRUE;
        unsigned id = item_count + 1 + i;
        unsigned char before = states[id-1], source = sources[id-1], previous = availability[id-1];
        BYTE old_mark = o3_defaults[id-1];
        availability[id-1] = (unsigned char)dynamic_check(id, &states[id-1], &sources[id-1], &o3_defaults[id-1]);
        if (states[id-1] != before || sources[id-1] != source || availability[id-1] != previous ||
            o3_defaults[id-1] != old_mark) changed = TRUE;
    }
    enforce_energy();
    enforce_stopped();
    return changed;
}

static void enforce_stopped(void) {
    static unsigned tick;
    if (++tick < 5) return;
    tick = 0;
    DWORD front = 0;
    GetWindowThreadProcessId(GetForegroundWindow(), &front);
    for (unsigned i = 0; i < dyn_count; ++i) {
        DynItem *d = dyn_items[i];
        if (d->type != dyn_stop || d->locked || d->restarts || d->current) continue;
        if (d->place == 3) continue;
        if (d->place == 0) {
            SC_HANDLE scm = OpenSCManagerW(NULL, NULL, SC_MANAGER_CONNECT);
            SC_HANDLE s = scm ? OpenServiceW(scm, d->key, SERVICE_QUERY_STATUS | SERVICE_STOP) : NULL;
            SERVICE_STATUS status;
            if (s && QueryServiceStatus(s, &status) && status.dwCurrentState == SERVICE_RUNNING)
                ControlService(s, SERVICE_CONTROL_STOP, &status);
            if (s) CloseServiceHandle(s);
            if (scm) CloseServiceHandle(scm);
            continue;
        }
        BOOL opened_by_player = FALSE;
        for (unsigned j = 0; j < proc_total && !opened_by_player; ++j)
            opened_by_player = proc_list[j].pid == front && !lstrcmpiW(proc_list[j].name, d->key);
        if (opened_by_player) {
            d->current = 1;
            save_stopped(d->place == 1 ? 'P' : 'W', d->key, NULL, FALSE);
            continue;
        }
        for (unsigned j = 0; j < proc_total; ++j) {
            const ProcInfo *p = &proc_list[j];
            if (lstrcmpiW(p->name, d->key) || (d->place == 1 && !user_process(p)) || critical_process(p->pid)) continue;
            HANDLE h = OpenProcess(PROCESS_TERMINATE, FALSE, p->pid);
            if (h) { TerminateProcess(h, 0); CloseHandle(h); }
        }
    }
}

static void dynamic_restore(unsigned *restored, unsigned *same, unsigned *failures) {
    for (unsigned i = 0; i < dyn_count; ++i) {
        DynItem *d = dyn_items[i];
        if (d->locked) continue;
        if (d->type == dyn_energy && d->current != energy_normal) persist_energy(d->key, energy_normal);
        if (d->current == d->baseline) { ++*same; continue; }
        unsigned char state, source;
        unsigned result = dynamic_apply(item_count + 1 + i, request_default, &state, &source);
        if (result == changed_live || result == changed_saved) ++*restored;
        else if (result == no_change) ++*same;
        else ++*failures;
    }
    energy_release_all();
    for (unsigned i = 0; i < dyn_count; ++i)
        if (dyn_items[i]->type == dyn_stop && dyn_items[i]->place == 3 && !dyn_items[i]->current) pause_run(dyn_items[i], TRUE);
    WCHAR path[MAX_PATH];
    if (stopped_path(path)) DeleteFileW(path);
    load_stopped_file();
}

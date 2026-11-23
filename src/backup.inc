typedef struct { const char *title, *detail; } BackupFeature;
typedef struct { HANDLE file; BOOL ok; } JsonFile;

static void json_bytes(JsonFile *out, const char *data, DWORD size) {
    DWORD written;
    if (out->ok && size)
        out->ok = WriteFile(out->file, data, size, &written, NULL) && written == size;
}

static void json_text(JsonFile *out, const char *text) {
    json_bytes(out, text, lstrlenA(text));
}

static void json_string(JsonFile *out, const char *text) {
    if (!text) text = "";
    json_text(out, "\"");
    const char *start = text;
    for (; *text; ++text) {
        unsigned char ch = (unsigned char)*text;
        if (ch >= 32 && ch != '"' && ch != '\\') continue;
        json_bytes(out, start, (DWORD)(text - start));
        char escape[8];
        wsprintfA(escape, "\\u%04x", (unsigned)ch);
        json_text(out, escape);
        start = text + 1;
    }
    json_bytes(out, start, (DWORD)(text - start));
    json_text(out, "\"");
}

static void json_number(JsonFile *out, DWORD value) {
    char number_text[16];
    wsprintfA(number_text, "%u", value);
    json_text(out, number_text);
}

static void json_hex(JsonFile *out, const BYTE *bytes, DWORD length) {
    static const char digits[] = "0123456789abcdef";
    char chunk[128];
    json_text(out, "\"");
    while (length) {
        DWORD n = length > 64 ? 64 : length;
        for (DWORD i = 0; i < n; ++i) {
            chunk[2*i] = digits[bytes[i] >> 4];
            chunk[2*i+1] = digits[bytes[i] & 15];
        }
        json_bytes(out, chunk, 2*n);
        bytes += n;
        length -= n;
    }
    json_text(out, "\"");
}

static void backup_registry(JsonFile *out, const Preference *pref) {
    json_text(out, ",\"registry\":{\"hive\":\"HKCU\",\"key\":");
    json_string(out, pref->subkey);
    json_text(out, ",\"name\":");
    json_string(out, pref->name);
    HKEY key;
    LONG error = RegOpenKeyExA(HKEY_CURRENT_USER, pref->subkey, 0, KEY_QUERY_VALUE, &key);
    if (error == ERROR_SUCCESS) {
        WCHAR name[128];
        DWORD type, size = 0;
        MultiByteToWideChar(CP_ACP, 0, pref->name, -1, name, ARRAYSIZE(name));
        error = RegQueryValueExW(key, name, NULL, &type, NULL, &size);
        if (error == ERROR_SUCCESS) {
            BYTE *bytes = HeapAlloc(GetProcessHeap(), 0, size ? size : 1);
            error = bytes ? RegQueryValueExW(key, name, NULL, &type, bytes, &size) : ERROR_OUTOFMEMORY;
            if (error == ERROR_SUCCESS) {
                json_text(out, ",\"exists\":true,\"type\":");
                json_number(out, type);
                json_text(out, ",\"bytes_hex\":");
                json_hex(out, bytes, size);
                if (type == REG_DWORD && size == sizeof(DWORD)) {
                    json_text(out, ",\"dword\":");
                    json_number(out, *(const DWORD *)bytes);
                }
            }
            if (bytes) HeapFree(GetProcessHeap(), 0, bytes);
        }
        RegCloseKey(key);
    }
    if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND)
        json_text(out, ",\"exists\":false");
    else if (error != ERROR_SUCCESS) {
        json_text(out, ",\"read_error\":");
        json_number(out, (DWORD)error);
    }
    json_text(out, "}");
}

static void backup_input(JsonFile *out, const InputSetting *setting) {
    InputValue value = {0};
    BOOL enabled;
    if (!read_input(setting, &value, &enabled)) {
        json_text(out, ",\"windows_parameters\":null");
        return;
    }
    if (setting->id == 66) {
        json_text(out, ",\"animation_effects\":{");
        for (unsigned i = 0; i < ARRAYSIZE(effects); ++i) {
            if (i) json_text(out, ",");
            json_string(out, effects[i].name);
            json_text(out, value.animations[i] ? ":true" : ":false");
        }
        json_text(out, "}");
        return;
    }
    DWORD size = setting->size ? setting->size : sizeof(UINT);
    if (setting->mode == accel_param) size = sizeof value.mouse;
    json_text(out, ",\"windows_parameters\":{\"spi_get\":");
    json_number(out, setting->get);
    json_text(out, ",\"bytes_hex\":");
    json_hex(out, (const BYTE *)&value, size);
    json_text(out, "}");
}

static BOOL existing_backup(const WCHAR *path) {
    WIN32_FILE_ATTRIBUTE_DATA data;
    return GetFileAttributesExW(path, GetFileExInfoStandard, &data) &&
           !(data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
           (data.nFileSizeHigh || data.nFileSizeLow);
}

static const char *const state_names[] = {"unknown","on","off","inherited","unavailable","read_error","pending","unverified"};
static const char *const source_names[] = {"none","powershell","cmd","win32","windows_default","emulator_config","browser_preference","windows_settings","android"};
static const char *const availability_names[] = {"no_toggle","live","saved","unsupported","unreadable","read_only","choose_app"};
static const BackupFeature *backup_features;
static WCHAR backup_folder[MAX_PATH], baseline_path[MAX_PATH], extra_path[MAX_PATH], count_path[MAX_PATH];
static DWORD current_launch;
static BOOL paths_ready;

static BOOL set_paths(const WCHAR *directory) {
    if (lstrlenW(directory) + 64 >= MAX_PATH) return FALSE;
    lstrcpyW(backup_folder, directory);
    lstrcatW(backup_folder, L"DO NOT DELETE THIS backupfirst");
    lstrcpyW(baseline_path, backup_folder);
    lstrcatW(baseline_path, L"\\first-run.json");
    lstrcpyW(extra_path, backup_folder);
    lstrcatW(extra_path, L"\\added-items.json");
    lstrcpyW(count_path, directory);
    lstrcatW(count_path, L"launch-count.json");
    return TRUE;
}

static BOOL folder_writable(void) {
    WCHAR probe[MAX_PATH];
    if (!CreateDirectoryW(backup_folder, NULL) && GetLastError() != ERROR_ALREADY_EXISTS) return FALSE;
    if (!GetTempFileNameW(backup_folder, L"O3W", 0, probe)) return FALSE;
    DeleteFileW(probe);
    return TRUE;
}

static BOOL initialize_paths(void) {
    if (paths_ready) return TRUE;
    WCHAR directory[MAX_PATH];
    DWORD n = GetModuleFileNameW(NULL, directory, ARRAYSIZE(directory));
    if (n && n < ARRAYSIZE(directory)) {
        while (n && directory[n-1] != L'\\') --n;
        directory[n] = 0;
        if (n && set_paths(directory) && (existing_backup(baseline_path) || folder_writable()))
            return paths_ready = TRUE;
    }
    n = GetEnvironmentVariableW(L"LOCALAPPDATA", directory, MAX_PATH - 80);
    if (!n || n >= MAX_PATH - 80) return FALSE;
    lstrcatW(directory, L"\\OffOrOn\\");
    CreateDirectoryW(directory, NULL);
    return paths_ready = set_paths(directory) && folder_writable();
}

static BOOL previous_launches(DWORD *count) {
    HANDLE file = CreateFileW(count_path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        if (GetLastError() != ERROR_FILE_NOT_FOUND) return FALSE;
        *count = existing_backup(baseline_path) ? 1 : 0;
        return TRUE;
    }
    char digits[16];
    DWORD length = GetFileSize(file, NULL), read = 0;
    BOOL ok = length && length < sizeof digits && ReadFile(file, digits, length, &read, NULL) && read == length;
    CloseHandle(file);
    if (!ok) return FALSE;
    if (digits[length-1] == '\n') --length;
    if (!length || digits[0] == '0') return FALSE;
    for (DWORD i = 0; i < length; ++i)
        if (digits[i] < '0' || digits[i] > '9') return FALSE;
    digits[length] = 0;
    return number(digits, count);
}

DWORD o3_record_launch(void) {
    if (current_launch || !initialize_paths()) return current_launch;
    HANDLE lock = CreateMutexW(NULL, FALSE, L"Global\\OffOrOn.FirstRun.Count");
    if (!lock) return 0;
    DWORD wait = WaitForSingleObject(lock, 5000);
    if (wait == WAIT_OBJECT_0 || wait == WAIT_ABANDONED) {
        DWORD prior;
        WCHAR temp[MAX_PATH];
        lstrcpyW(temp, count_path);
        lstrcatW(temp, L".tmp");
        if (previous_launches(&prior) && prior < MAXDWORD) {
            HANDLE file = CreateFileW(temp, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
            if (file != INVALID_HANDLE_VALUE) {
                char digits[16];
                DWORD size = wsprintfA(digits, "%u\n", prior+1), written;
                BOOL ok = WriteFile(file, digits, size, &written, NULL) && size == written && FlushFileBuffers(file);
                CloseHandle(file);
                if (ok && MoveFileExW(temp, count_path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
                    current_launch = prior+1;
                else DeleteFileW(temp);
            }
        }
        ReleaseMutex(lock);
    }
    CloseHandle(lock);
    return current_launch;
}

static void utc_stamp(char *text) {
    SYSTEMTIME utc;
    GetSystemTime(&utc);
    wsprintfA(text, "%04u-%02u-%02uT%02u:%02u:%02uZ",
              utc.wYear, utc.wMonth, utc.wDay, utc.wHour, utc.wMinute, utc.wSecond);
}

static void write_record(JsonFile *out, unsigned id, BYTE state, BYTE source, BYTE availability,
                         const char *stamp) {
    json_text(out, "{\"id\":");
    json_number(out, id);
    json_text(out, ",\"name\":");
    json_string(out, backup_features ? backup_features[id-1].title : "");
    json_text(out, ",\"state\":");
    json_string(out, state_names[state < ARRAYSIZE(state_names) ? state : 0]);
    json_text(out, ",\"source\":");
    json_string(out, source_names[source < ARRAYSIZE(source_names) ? source : 0]);
    json_text(out, ",\"availability\":");
    json_string(out, availability_names[availability < ARRAYSIZE(availability_names) ? availability : 0]);
    if (stamp) {
        json_text(out, ",\"captured_utc\":");
        json_string(out, stamp);
    }
    const Preference *pref = preference_for(id);
    const InputSetting *input = input_for(id);
    if (pref) backup_registry(out, pref);
    if (input) backup_input(out, input);
    json_text(out, "}");
}

static BOOL begin_json(JsonFile *out, WCHAR *temp, const char *header_extra) {
    out->file = INVALID_HANDLE_VALUE;
    out->ok = FALSE;
    if (!CreateDirectoryW(backup_folder, NULL) && GetLastError() != ERROR_ALREADY_EXISTS) return FALSE;
    if (!GetTempFileNameW(backup_folder, L"O3B", 0, temp)) return FALSE;
    out->file = CreateFileW(temp, GENERIC_WRITE, 0, NULL, TRUNCATE_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (out->file == INVALID_HANDLE_VALUE) {
        DeleteFileW(temp);
        return FALSE;
    }
    out->ok = TRUE;
    char stamp[24], header[160];
    utc_stamp(stamp);
    wsprintfA(header, "{\n\"schema_version\":2,\"captured_utc\":\"%s\"%s,\"settings\":[\n", stamp, header_extra);
    json_text(out, header);
    return TRUE;
}

static BOOL finish_json(JsonFile *out, const WCHAR *temp, const WCHAR *target, BOOL replace) {
    json_text(out, "\n]}\n");
    BOOL ok = out->ok && FlushFileBuffers(out->file);
    CloseHandle(out->file);
    if (ok) ok = replace ? ReplaceFileW(target, temp, NULL, 0, NULL, NULL)
                         : MoveFileExW(temp, target, MOVEFILE_WRITE_THROUGH);
    if (!ok) DeleteFileW(temp);
    return ok;
}

static BOOL write_baseline(const BYTE *states, const BYTE *sources, const BYTE *availability, BOOL recreated) {
    WCHAR temp[MAX_PATH];
    JsonFile out;
    if (!begin_json(&out, temp, recreated ? ",\"recreated\":true" : "")) return FALSE;
    for (unsigned i = 0; i < item_count; ++i) {
        if (i) json_text(&out, ",\n");
        write_record(&out, i + 1, states[i], sources[i], availability[i], NULL);
    }
    return finish_json(&out, temp, baseline_path, FALSE) || existing_backup(baseline_path);
}

typedef struct { JsonDoc base, extra; BOOL base_ok, extra_ok; } Records;

static int skip_value(const JsonDoc *doc, int item) {
    int i = item + 1;
    while (i < doc->count && doc->tokens[i].start < doc->tokens[item].end) ++i;
    return i;
}

static BOOL token_is(const JsonDoc *doc, int token, const char *text) {
    if (token < 0) return FALSE;
    const jsmntok_t *t = doc->tokens + token;
    int n = t->end - t->start;
    return (t->type == JSMN_STRING || t->type == JSMN_PRIMITIVE) && n == lstrlenA(text) &&
           bytes_match(doc->bytes + t->start, text, n);
}

static BOOL token_number(const JsonDoc *doc, int token, DWORD *value) {
    if (token < 0 || doc->tokens[token].type != JSMN_PRIMITIVE) return FALSE;
    int n = doc->tokens[token].end - doc->tokens[token].start;
    char text[12];
    if (n < 1 || n > 10) return FALSE;
    for (int i = 0; i < n; ++i) text[i] = doc->bytes[doc->tokens[token].start + i];
    text[n] = 0;
    return number(text, value);
}

static int settings_array(const JsonDoc *doc) {
    int list = json_member(doc, 0, "settings", 8);
    return list >= 0 && doc->tokens[list].type == JSMN_ARRAY ? list : -1;
}

static int find_record(const JsonDoc *doc, unsigned id) {
    int list = settings_array(doc);
    if (list < 0) return -1;
    for (int i = list + 1; i < doc->count && doc->tokens[i].start < doc->tokens[list].end;) {
        int item = i;
        DWORD value;
        i = skip_value(doc, item);
        if (doc->tokens[item].type == JSMN_OBJECT &&
            token_number(doc, json_member(doc, item, "id", 2), &value) && value == id) return item;
    }
    return -1;
}

static BOOL usable_record(const JsonDoc *doc, int record) {
    if (record < 0) return FALSE;
    int registry = json_member(doc, record, "registry", 8);
    if (registry >= 0) return json_member(doc, registry, "exists", 6) >= 0;
    if (json_member(doc, record, "animation_effects", 17) >= 0) return TRUE;
    int state = json_member(doc, record, "state", 5);
    return token_is(doc, state, "on") || token_is(doc, state, "off");
}

static void open_records(Records *r) {
    r->base_ok = open_doc(&r->base, baseline_path, FALSE) && settings_array(&r->base) >= 0;
    r->extra_ok = open_doc(&r->extra, extra_path, FALSE) && settings_array(&r->extra) >= 0;
}

static void close_records(Records *r) {
    close_doc(&r->base);
    close_doc(&r->extra);
}

static const JsonDoc *record_for(const Records *r, unsigned id, int *index) {
    if (r->extra_ok && usable_record(&r->extra, *index = find_record(&r->extra, id))) return &r->extra;
    if (r->base_ok && usable_record(&r->base, *index = find_record(&r->base, id))) return &r->base;
    return NULL;
}

static BOOL restorable(unsigned id) {
    return id <= item_count && id != 26 && target_group(id) < 0;
}

static BOOL wants_record(unsigned id, BYTE state, BYTE availability) {
    return restorable(id) && (availability == live_toggle || availability == saved_toggle) &&
           (state == on || state == off);
}

static BOOL record_missing(const BYTE *states, const BYTE *sources, const BYTE *availability, unsigned only) {
    Records r = {0};
    BYTE missing[item_count];
    unsigned count = 0;
    open_records(&r);
    for (unsigned id = 1; id <= item_count; ++id) {
        int index;
        missing[id-1] = (BYTE)((!only || id == only) && wants_record(id, states[id-1], availability[id-1]) &&
                               !record_for(&r, id, &index));
        count += missing[id-1];
    }
    if (!count) {
        close_records(&r);
        return TRUE;
    }
    BOOL exists = GetFileAttributesW(extra_path) != INVALID_FILE_ATTRIBUTES;
    WCHAR temp[MAX_PATH];
    JsonFile out;

    if (exists && !r.extra_ok) {
        WCHAR aside[MAX_PATH];
        lstrcpyW(aside, extra_path);
        wsprintfW(aside + lstrlenW(aside) - 5, L".unreadable-%u.json", GetTickCount());
        close_doc(&r.extra);
        r.extra = (JsonDoc){0};
        if (!MoveFileExW(extra_path, aside, MOVEFILE_WRITE_THROUGH)) {
            close_records(&r);
            return FALSE;
        }
        exists = FALSE;
    }
    if (!begin_json(&out, temp, ",\"kind\":\"added_items\"")) {
        close_records(&r);
        return FALSE;
    }
    unsigned written = 0;
    if (r.extra_ok) {
        const JsonDoc *d = &r.extra;
        int list = settings_array(d);
        for (int i = list + 1; i < d->count && d->tokens[i].start < d->tokens[list].end;) {
            int item = i;
            i = skip_value(d, item);
            if (written++) json_text(&out, ",\n");
            json_bytes(&out, d->bytes + d->tokens[item].start,
                       (DWORD)(d->tokens[item].end - d->tokens[item].start));
        }
    }
    char stamp[24];
    utc_stamp(stamp);
    for (unsigned id = 1; id <= item_count; ++id) {
        if (!missing[id-1]) continue;
        if (written++) json_text(&out, ",\n");
        write_record(&out, id, states[id-1], sources[id-1], availability[id-1], stamp);
    }
    close_records(&r);
    return finish_json(&out, temp, extra_path, exists);
}

DWORD o3_backup_first(const BackupFeature *features, const BYTE *states,
                      const BYTE *sources, const BYTE *availability) {
    backup_features = features;
    if (!initialize_paths()) return 0;
    DWORD result = 1;
    if (!existing_backup(baseline_path)) {
        BOOL recreated = current_launch >= 2;
        if (!write_baseline(states, sources, availability, recreated)) return 0;
        if (recreated) result = 2;
    }
    return record_missing(states, sources, availability, 0) ? result : 0;
}

static BOOL record_before_change(unsigned id) {
    if (!restorable(id)) return TRUE;
    if (!initialize_paths()) return FALSE;
    Records r = {0};
    int index;
    open_records(&r);
    BOOL known = record_for(&r, id, &index) != NULL;
    close_records(&r);
    if (known) return TRUE;
    BYTE states[item_count], sources[item_count], availability[item_count];
    states[id-1] = manual;
    sources[id-1] = 0;
    availability[id-1] = (BYTE)check_option(id, &states[id-1], &sources[id-1], older_windows());
    return record_missing(states, sources, availability, id);
}

static unsigned restore_registry(const JsonDoc *doc, int reg, const Preference *pref) {
    int exists = json_member(doc, reg, "exists", 6);
    if (!token_is(doc, json_member(doc, reg, "name", 4), pref->name)) return cannot_change;
    BOOL present = token_is(doc, exists, "true");
    if (!present && !token_is(doc, exists, "false")) return cannot_change;
    DWORD wanted = 0, type = 0;
    if (present && (!token_number(doc, json_member(doc, reg, "type", 4), &type) || type != REG_DWORD ||
                    !token_number(doc, json_member(doc, reg, "dword", 5), &wanted)))
        return cannot_change;

    HKEY key;
    LONG rc = present
        ? RegCreateKeyExA(HKEY_CURRENT_USER, pref->subkey, 0, NULL, 0, KEY_QUERY_VALUE | KEY_SET_VALUE, NULL, &key, NULL)
        : RegOpenKeyExA(HKEY_CURRENT_USER, pref->subkey, 0, KEY_QUERY_VALUE | KEY_SET_VALUE, &key);
    if (!present && (rc == ERROR_FILE_NOT_FOUND || rc == ERROR_PATH_NOT_FOUND)) return no_change;
    if (rc != ERROR_SUCCESS) return cannot_change;

    DWORD current = 0, current_type = 0, size = sizeof current;
    rc = RegQueryValueExA(key, pref->name, NULL, &current_type, (BYTE *)&current, &size);
    BOOL matches = present ? rc == ERROR_SUCCESS && current_type == REG_DWORD && size == sizeof current && current == wanted
                           : rc == ERROR_FILE_NOT_FOUND;
    unsigned result = no_change;
    if (!matches) {
        rc = present ? RegSetValueExA(key, pref->name, 0, REG_DWORD, (const BYTE *)&wanted, sizeof wanted)
                     : RegDeleteValueA(key, pref->name);
        size = sizeof current;
        LONG check = RegQueryValueExA(key, pref->name, NULL, &current_type, (BYTE *)&current, &size);
        BOOL verified = present ? check == ERROR_SUCCESS && current_type == REG_DWORD && current == wanted
                                : check == ERROR_FILE_NOT_FOUND;
        result = rc == ERROR_SUCCESS && verified ? changed_saved : change_failed;
    }
    RegCloseKey(key);
    if (result == changed_saved) {
        DWORD_PTR ignored;
        SendMessageTimeoutA(HWND_BROADCAST, WM_SETTINGCHANGE, 0, (LPARAM)pref->subkey,
                            SMTO_ABORTIFHUNG | SMTO_BLOCK, 100, &ignored);
    }
    return result;
}

static unsigned restore_animations(const JsonDoc *doc, int flags) {
    BOOL wanted[ARRAYSIZE(effects)], current[ARRAYSIZE(effects)];
    BOOL differs = FALSE;
    for (unsigned i = 0; i < ARRAYSIZE(effects); ++i) {
        int t = json_member(doc, flags, effects[i].name, lstrlenA(effects[i].name));
        if (!token_is(doc, t, "true") && !token_is(doc, t, "false")) return cannot_change;
        wanted[i] = token_is(doc, t, "true");
        if (!read_effect(&effects[i], &current[i])) return cannot_change;
        if (!!current[i] != wanted[i]) differs = TRUE;
    }
    if (!differs) return no_change;
    for (unsigned i = 0; i < ARRAYSIZE(effects); ++i) {
        BOOL actual = FALSE;
        if (!!current[i] != wanted[i] && !set_effect(&effects[i], wanted[i])) return change_failed;
        if (!read_effect(&effects[i], &actual) || !!actual != wanted[i]) return change_failed;
    }
    return changed_live;
}

static void dynamic_restore(unsigned *restored, unsigned *same, unsigned *failures);

const char *o3_restore(BYTE *states, BYTE *sources, BYTE *availability) {
    static char message[192];
    Records r = {0};
    if (!initialize_paths()) return "Folder backup tidak dapat dibuka.";
    open_records(&r);
    if (!r.base_ok && !r.extra_ok) {
        close_records(&r);
        return "Backup tidak ditemukan atau rusak.";
    }
    BOOL old_windows = older_windows();
    unsigned restored = 0, same = 0, failures = 0, busy = 0, skipped = 0;
    for (unsigned id = 1; id <= item_count; ++id) {
        int record;
        const JsonDoc *doc = restorable(id) ? record_for(&r, id, &record) : NULL;
        if (!doc) continue;
        BYTE state = manual, source = 0;
        unsigned available = check_option(id, &state, &source, old_windows);
        if (available != live_toggle && available != saved_toggle) {
            if (available != read_only && available != no_toggle) ++skipped;
            continue;
        }
        const Preference *pref = preference_for(id);
        int registry = json_member(doc, record, "registry", 8);
        int flags = json_member(doc, record, "animation_effects", 17);
        unsigned result;
        if (pref && registry >= 0) result = restore_registry(doc, registry, pref);
        else if (id == 66 && flags >= 0) result = restore_animations(doc, flags);
        else {
            int saved = json_member(doc, record, "state", 5);
            result = token_is(doc, saved, "on") || token_is(doc, saved, "off")
                ? apply_setting(id, token_is(doc, saved, "on"), &state, &source) : cannot_change;
        }
        if (result == changed_live || result == changed_saved) ++restored;
        else if (result == no_change) ++same;
        else if (result == browser_busy || result == emulator_busy) ++busy;
        else ++failures;
    }
    close_records(&r);
    dynamic_restore(&restored, &same, &failures);
    o3_options(states, sources, availability, NULL);
    if (!restored && !same && !failures && !busy)
        return "Tidak ada setelan tercatat yang bisa dipulihkan saat ini.";
    int n = wsprintfA(message, "Dipulihkan %u, sudah sesuai %u, gagal %u", restored, same, failures + busy);
    if (skipped) n += wsprintfA(message + n, ", dilewati %u", skipped);
    lstrcpyA(message + n, busy ? ". Tutup browser/emulator lalu ulangi." : ".");
    return message;
}

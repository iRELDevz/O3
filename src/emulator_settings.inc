typedef struct { unsigned id; BOOL per_instance; const char *key; } EmulatorSwitch;
static const EmulatorSwitch emulator_switches[] = {
    {3, TRUE, "enable_fps_display"}, {6, TRUE, "game_controls_enabled"},
    {7, FALSE, "key_controls_overlay_enabled"}, {8, FALSE, "enable_gamepad_detection"},
    {9, FALSE, "enable_gamepad_vibration"}, {10, TRUE, "enable_notifications"},
    {11, TRUE, "android_sound_while_tapping"}, {12, FALSE, "create_desktop_shortcuts"},
    {13, FALSE, "enable_discord_integration"}, {14, FALSE, "enable_programmatic_ads"},
    {15, FALSE, "enable_esc_fullscreen"}
};
static char emulator_file[512], emulator_instance[64];
static const EmulatorSwitch *emulator_switch(unsigned id) {
    for (unsigned i=0; i<ARRAYSIZE(emulator_switches); ++i)
        if (emulator_switches[i].id == id) return &emulator_switches[i];
    return NULL;
}

static LONG bit_offset(const char *data, DWORD size, const char *key) {
    DWORD key_length = lstrlenA(key);
    LONG found = -1;
    for (DWORD start = 0; start < size;) {
        DWORD end = start;
        while (end < size && data[end] != '\n') ++end;
        DWORD limit = end;
        if (limit > start && data[limit-1] == '\r') --limit;
        if (!start && size >= 3 && (BYTE)data[0]==0xef && (BYTE)data[1]==0xbb && (BYTE)data[2]==0xbf) start=3;
        if (limit-start > key_length && prefix(data+start,key) && data[start+key_length]=='=') {
            DWORD pos = start+key_length+2;
            if (found >= 0 || limit-start != key_length+4 || data[pos-1]!='"' ||
                data[pos+1]!='"' || (data[pos]!='0' && data[pos]!='1')) return -1;
            found = (LONG)pos;
        }
        start=end+1;
    }
    return found;
}

static BOOL emulator_setting(const EmulatorSwitch *entry, int requested, unsigned char *state) {
    if (!emulator_file[0] || (entry->per_instance && !emulator_instance[0])) return FALSE;
    char key[152];
    if (entry->per_instance) wsprintfA(key,"bst.instance.%s.%s",emulator_instance,entry->key);
    else wsprintfA(key,"bst.%s",entry->key);
    HANDLE file = CreateFileA(emulator_file, GENERIC_READ | (requested>=0 ? GENERIC_WRITE : 0),
                              requested>=0 ? 0 : FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,
                              NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file==INVALID_HANDLE_VALUE) return FALSE;
    LARGE_INTEGER size;
    DWORD n=0;
    BOOL ok=GetFileSizeEx(file,&size) && size.QuadPart>0 && size.QuadPart<=config_limit;
    char *bytes=ok ? HeapAlloc(GetProcessHeap(),0,(SIZE_T)size.QuadPart+1) : NULL;
    ok=bytes && ReadFile(file,bytes,size.LowPart,&n,NULL) && n==size.LowPart;
    LONG pos=-1;
    if (ok) {
        bytes[n]=0;
        pos=bit_offset(bytes,n,key);
        ok=pos>=0;
        if (ok && entry->id == 14) {
            LONG feature = bit_offset(bytes,n,"bst.feature.programmatic_ads");
            LONG visible = bit_offset(bytes,n,"bst.feature.show_programmatic_ads_preference");
            ok = feature >= 0 && visible >= 0 && bytes[feature]=='1' && bytes[visible]=='1';
        }
    }
    if (ok && requested>=0 && bytes[pos] != '0'+requested) {
        char bit=(char)('0'+requested);
        ok=SetFilePointer(file,pos,NULL,FILE_BEGIN)!=(DWORD)-1 &&
           WriteFile(file,&bit,1,&n,NULL) && n==1 && FlushFileBuffers(file) &&
           SetFilePointer(file,pos,NULL,FILE_BEGIN)!=(DWORD)-1 &&
           ReadFile(file,bytes+pos,1,&n,NULL) && n==1 && bytes[pos]==bit;
    }
    if (ok) *state=bytes[pos]=='1' ? on : off;
    if (bytes) HeapFree(GetProcessHeap(),0,bytes);
    CloseHandle(file);
    return ok;
}

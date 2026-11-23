#define SENDERS L"Software\\Microsoft\\Windows\\CurrentVersion\\Notifications\\Settings"
#define TRAY_ENTRIES L"Control Panel\\NotifyIconSettings"
#define STARTUP_APPROVAL L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\StartupApproved\\Run"
#define STARTUP_RUN L"Software\\Microsoft\\Windows\\CurrentVersion\\Run"

extern const char *o3_audio_label(void);
extern BOOL o3_audio_choose(HWND owner);
extern int o3_audio_access(int requested);

typedef struct { WCHAR name[256]; char label[768]; } AppTarget;
static AppTarget selected_targets[3];

static int target_group(unsigned id) {
    if (id >= 33 && id <= 37) return 0;
    if (id == 58) return 1;
    if (id == 88) return 2;
    return -1;
}

const char *o3_target_label(unsigned id) {
    if (id==26) return o3_audio_label();
    int group = target_group(id);
    if (group < 0) return NULL;
    return selected_targets[group].name[0] ? selected_targets[group].label : "Klik nama fitur untuk memilih aplikasi";
}

static const WCHAR *target_root(int group) {
    return group == 0 ? SENDERS : group == 1 ? TRAY_ENTRIES : STARTUP_APPROVAL;
}

static BOOL has_startup_command(const WCHAR *name) {
    return RegGetValueW(HKEY_CURRENT_USER,STARTUP_RUN,name,RRF_RT_REG_SZ|RRF_RT_REG_EXPAND_SZ,NULL,NULL,NULL) == ERROR_SUCCESS;
}

static BOOL has_target_root(int group) {
    HKEY key;
    if (RegOpenKeyExW(HKEY_CURRENT_USER,target_root(group),0,KEY_READ,&key) != ERROR_SUCCESS) return FALSE;
    DWORD keys=0,values=0;
    LONG rc=RegQueryInfoKeyW(key,NULL,NULL,NULL,&keys,NULL,NULL,&values,NULL,NULL,NULL,NULL);
    RegCloseKey(key);
    return rc==ERROR_SUCCESS && (group==2 ? values : keys)>0;
}

BOOL o3_choose_target(HWND owner, unsigned id) {
    if (id==26) return o3_audio_choose(owner);
    int group=target_group(id);
    if (group<0) return FALSE;
    HKEY key;
    if (RegOpenKeyExW(HKEY_CURRENT_USER,target_root(group),0,KEY_READ,&key)!=ERROR_SUCCESS) return FALSE;
    DWORD subkeys=0, values=0;
    RegQueryInfoKeyW(key,NULL,NULL,NULL,&subkeys,NULL,NULL,&values,NULL,NULL,NULL,NULL);
    DWORD total=group==2 ? values : subkeys;
    if (total>8192) total=8192;
    typedef struct {WCHAR name[256], display[512];} Choice;
    Choice *choices=HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,sizeof(Choice)*total);
    HMENU menu=CreatePopupMenu();
    unsigned count=0, chosen=0;
    if (choices && menu) {
        for (DWORD i=0; i<total; ++i) {
            Choice *item=&choices[count];
            DWORD size=ARRAYSIZE(item->name);
            LONG rc=group==2 ? RegEnumValueW(key,i,item->name,&size,NULL,NULL,NULL,NULL) :
                                RegEnumKeyExW(key,i,item->name,&size,NULL,NULL,NULL,NULL);
            if (rc!=ERROR_SUCCESS || !size) continue;
            if (group==2 && !has_startup_command(item->name)) continue;
            lstrcpyW(item->display,item->name);
            if (group==1) {
                WCHAR path[MAX_PATH];
                size=sizeof path;
                if (RegGetValueW(key,item->name,L"ExecutablePath",RRF_RT_REG_SZ,NULL,path,&size)!=ERROR_SUCCESS) continue;
                WCHAR *base=path;
                for (WCHAR *c=path;*c;++c) if (*c=='\\') base=c+1;
                if (!*base) continue;
                if (lstrlenW(base)+lstrlenW(item->name)+5 >= (int)ARRAYSIZE(item->display)) continue;
                wsprintfW(item->display,L"%s  [%s]",base,item->name);
            }
            UINT flags=MF_STRING;
            if (!lstrcmpW(selected_targets[group].name,item->name)) flags|=MF_CHECKED;
            if (AppendMenuW(menu,flags,count+1,item->display)) ++count;
        }
        POINT point;
        if (count && GetCursorPos(&point)) chosen=TrackPopupMenu(menu,TPM_RETURNCMD|TPM_NONOTIFY|TPM_RIGHTBUTTON,
                                                                 point.x,point.y,0,owner,NULL);
        if (chosen && chosen<=count) {
            Choice *item=&choices[chosen-1];
            lstrcpyW(selected_targets[group].name,item->name);
            WideCharToMultiByte(CP_ACP,0,item->display,-1,selected_targets[group].label,sizeof selected_targets[group].label,NULL,NULL);
        }
    }
    if (choices) HeapFree(GetProcessHeap(),0,choices);
    if (menu) DestroyMenu(menu);
    RegCloseKey(key);
    return chosen!=0;
}

static unsigned sender_sound(HKEY key,const WCHAR *sender,int requested,BYTE *state) {
    WCHAR sound[1024];
    DWORD size=sizeof sound;
    LONG rc=RegGetValueW(key,NULL,L"SoundFile",RRF_RT_REG_SZ,NULL,sound,&size);
    if (rc!=ERROR_SUCCESS && rc!=ERROR_FILE_NOT_FOUND) return cannot_change;
    *state=rc==ERROR_FILE_NOT_FOUND || sound[0] ? on : off;
    if (requested<0 || *state==(requested?on:off)) return no_change;
    const WCHAR *cache=L"Software\\OffOrOn\\NotificationSounds";
    if (requested) {
        size=sizeof sound;
        rc=RegGetValueW(HKEY_CURRENT_USER,cache,sender,RRF_RT_REG_SZ,NULL,sound,&size);
        if (rc==ERROR_FILE_NOT_FOUND || rc==ERROR_PATH_NOT_FOUND) rc=RegDeleteValueW(key,L"SoundFile");
        else if (rc==ERROR_SUCCESS) rc=RegSetValueExW(key,L"SoundFile",0,REG_SZ,(BYTE*)sound,size);
    } else {
        if (rc==ERROR_SUCCESS) {
            HKEY backup;
            rc=RegCreateKeyExW(HKEY_CURRENT_USER,cache,0,NULL,0,KEY_SET_VALUE,NULL,&backup,NULL);
            if (rc!=ERROR_SUCCESS) return change_failed;
            rc=RegSetValueExW(backup,sender,0,REG_SZ,(BYTE*)sound,size);
            RegCloseKey(backup);
            if (rc!=ERROR_SUCCESS) return change_failed;
        } else {
            rc=RegDeleteKeyValueW(HKEY_CURRENT_USER,cache,sender);
            if (rc!=ERROR_SUCCESS && rc!=ERROR_FILE_NOT_FOUND && rc!=ERROR_PATH_NOT_FOUND) return change_failed;
        }
        rc=RegSetValueExW(key,L"SoundFile",0,REG_SZ,(const BYTE*)L"",sizeof(WCHAR));
    }
    if (rc!=ERROR_SUCCESS || sender_sound(key,sender,-1,state)!=no_change) return change_failed;
    return *state==(requested?on:off) ? changed_saved : change_failed;
}

static unsigned startup_access(HKEY key,const WCHAR *name,int requested,BYTE *state) {
    BYTE data[12];
    DWORD size=sizeof data;
    if (!has_startup_command(name) || RegGetValueW(key,NULL,name,RRF_RT_REG_BINARY,NULL,data,&size)!=ERROR_SUCCESS ||
        size!=sizeof data || (data[0]!=2 && data[0]!=3) || data[1] || data[2] || data[3]) return cannot_change;
    *state=data[0]==2 ? on : off;
    if (requested<0 || *state==(requested?on:off)) return no_change;
    FILETIME updated={0};
    if (!requested) GetSystemTimeAsFileTime(&updated);
    for (unsigned i=0; i<sizeof updated; ++i) data[4+i]=((const BYTE*)&updated)[i];
    data[0]=requested ? 2 : 3;
    if (RegSetValueExW(key,name,0,REG_BINARY,data,size)!=ERROR_SUCCESS ||
        startup_access(key,name,-1,state)!=no_change) return change_failed;
    return *state==(requested?on:off) ? changed_saved : change_failed;
}

static unsigned target_access(unsigned id,int requested,BYTE *state,BYTE *source) {
    int group=target_group(id);
    const WCHAR *name=selected_targets[group].name;
    if (!*name) return cannot_change;
    HKEY parent,key;
    REGSAM access=KEY_QUERY_VALUE|(requested>=0 ? KEY_SET_VALUE : 0);
    if (RegOpenKeyExW(HKEY_CURRENT_USER,target_root(group),0,group==2 ? access : KEY_READ,&parent)!=ERROR_SUCCESS)
        return cannot_change;
    key=parent;
    if (group!=2 && RegOpenKeyExW(parent,name,0,access,&key)!=ERROR_SUCCESS) {RegCloseKey(parent);return cannot_change;}
    unsigned result=cannot_change;
    *source=3;
    if (id==88) result=startup_access(key,name,requested,state);
    else if (id==37) result=sender_sound(key,name,requested,state);
    else {
        static const WCHAR *const values[]={L"Enabled",L"ShowBanner",L"ShowInActionCenter",L"AllowContentAboveLock"};
        const WCHAR *value_name=id==58 ? L"IsPromoted" : values[id-33];
        DWORD value,size=sizeof value;
        LONG rc=RegGetValueW(key,NULL,value_name,RRF_RT_REG_DWORD,NULL,&value,&size);
        if (rc==ERROR_FILE_NOT_FOUND) {value=id==58 ? 0 : 1;*source=4;}
        if ((rc==ERROR_FILE_NOT_FOUND || rc==ERROR_SUCCESS) && value<=1) {
            DWORD invert=id==36;
            *state=(value^invert) ? on : off;
            if (requested<0 || *state==(requested?on:off)) result=no_change;
            else {
                value=(DWORD)requested^invert;
                result=change_failed;
                if (RegSetValueExW(key,value_name,0,REG_DWORD,(BYTE*)&value,sizeof value)==ERROR_SUCCESS &&
                    RegGetValueW(key,NULL,value_name,RRF_RT_REG_DWORD,NULL,&value,&size)==ERROR_SUCCESS && value<=1) {
                    *source=3;*state=(value^invert) ? on : off;
                    if (*state==(requested?on:off)) result=changed_saved;
                }
            }
        }
    }
    if (key!=parent) RegCloseKey(key);
    RegCloseKey(parent);
    return result;
}

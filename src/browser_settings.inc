#define JSMN_STATIC
#define JSMN_STRICT
#include "vendor/jsmn.h"
#include <tlhelp32.h>
typedef struct {
    unsigned id; BOOL profile; const char *path, *yes, *no; BYTE fallback; const char *policy;
} BrowserSetting;
static const BrowserSetting browser_settings[] = {
    {93,0,"startup_boost.enabled","true","false",off,"StartupBoostEnabled"},
    {94,0,"background_mode.enabled","true","false",on,"BackgroundModeEnabled"},
    {95,1,"edge.sleeping_tabs.enabled","true","false",on,"SleepingTabsEnabled"},
    {96,1,"edge.sleeping_tabs.fade_tabs","true","false",on,"SleepingTabsFadeEnabled"},
    {97,0,"edge.perf_center.efficiency_mode_toggle","true","false",off,"EfficiencyModeEnabled"},
    {98,0,"performance_tuning.intervention_notification.enabled","true","false",on,"PerformanceInterventionNotificationsEnabled"},
    {99,0,"background_mode.enabled","true","false",on,"BackgroundModeEnabled"},
    {100,0,"performance_tuning.high_efficiency_mode.state","2","0",off,"HighEfficiencyModeEnabled"},
    {101,0,"performance_tuning.battery_saver_mode.state","1","0",on,"BatterySaverModeAvailability"},
    {102,1,"net.network_prediction_options","0","2",on,"NetworkPredictionOptions"},
    {103,0,"performance_tuning.intervention_notification.enabled","true","false",on,"PerformanceInterventionNotificationsEnabled"},
    {104,0,"browser.hovercard.memory_usage_enabled","true","false",on,"HoverCardMemoryUsageEnabled"}
};
typedef struct { HANDLE file; char *bytes; DWORD size; jsmntok_t *tokens; int count; } JsonDoc;

static void close_doc(JsonDoc *doc) {
    if (doc->file && doc->file!=INVALID_HANDLE_VALUE) CloseHandle(doc->file);
    if (doc->tokens) HeapFree(GetProcessHeap(),0,doc->tokens);
    if (doc->bytes) HeapFree(GetProcessHeap(),0,doc->bytes);
}

static BOOL open_doc(JsonDoc *doc, const WCHAR *path, BOOL editing) {
    DWORD read;
    LARGE_INTEGER size;
    doc->file=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_DELETE|(editing?0:FILE_SHARE_WRITE),
                          NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
    if (doc->file==INVALID_HANDLE_VALUE || !GetFileSizeEx(doc->file,&size) ||
        size.QuadPart<2 || size.QuadPart>32*1024*1024) return FALSE;
    doc->size=size.LowPart;
    doc->bytes=HeapAlloc(GetProcessHeap(),0,doc->size+1);
    if (!doc->bytes || !ReadFile(doc->file,doc->bytes,doc->size,&read,NULL) || read!=doc->size) return FALSE;
    doc->bytes[doc->size]=0;
    jsmn_parser parser;
    jsmn_init(&parser);
    int count=jsmn_parse(&parser,doc->bytes,doc->size,NULL,0);
    if (count<1 || count>2000000) return FALSE;
    doc->tokens=HeapAlloc(GetProcessHeap(),0,(SIZE_T)count*sizeof(*doc->tokens));
    if (!doc->tokens) return FALSE;
    jsmn_init(&parser);
    doc->count=jsmn_parse(&parser,doc->bytes,doc->size,doc->tokens,count);
    if (doc->count!=count || doc->tokens[0].type!=JSMN_OBJECT) return FALSE;
    for (DWORD i=(DWORD)doc->tokens[0].end; i<doc->size; ++i) {
        char c=doc->bytes[i];
        if (c!=' ' && c!='\t' && c!='\r' && c!='\n') return FALSE;
    }
    return TRUE;
}

static BOOL bytes_match(const char *a, const char *b, int n) {
    for (int i=0;i<n;++i) if (a[i]!=b[i]) return FALSE;
    return TRUE;
}

static int json_member(const JsonDoc *doc,int object,const char *name,int len) {
    const jsmntok_t *tokens=doc->tokens;
    if (tokens[object].type!=JSMN_OBJECT) return -2;
    int result=-1;
    for (int i=object+1;i<doc->count && tokens[i].start<tokens[object].end;) {
        int key=i++, value=i++;
        if (value>=doc->count || tokens[key].type!=JSMN_STRING) return -2;
        if (tokens[key].end-tokens[key].start==len && bytes_match(doc->bytes+tokens[key].start,name,len)) {
            if (result!=-1) return -2;
            result=value;
        }
        while (i<doc->count && tokens[i].start<tokens[value].end) ++i;
    }
    return result;
}

static BOOL process_named(const WCHAR *name) {
    HANDLE h=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);
    if (h==INVALID_HANDLE_VALUE) return TRUE;
    PROCESSENTRY32W p={0}; p.dwSize=sizeof p;
    BOOL running=FALSE;
    if (!Process32FirstW(h,&p)) running=TRUE;
    else do { if (!lstrcmpiW(p.szExeFile,name)) {running=TRUE;break;} } while (Process32NextW(h,&p));
    CloseHandle(h);
    return running;
}

static BOOL browser_locked(BOOL edge,const char *policy) {
    char subkey[96];
    lstrcpyA(subkey,edge?"Software\\Policies\\Microsoft\\Edge":"Software\\Policies\\Google\\Chrome");
    HKEY roots[]={HKEY_CURRENT_USER,HKEY_LOCAL_MACHINE};
    for (unsigned i=0;i<ARRAYSIZE(roots);++i) {
        LONG rc=RegGetValueA(roots[i],subkey,policy,RRF_RT_ANY,NULL,NULL,NULL);
        if (rc!=ERROR_FILE_NOT_FOUND && rc!=ERROR_PATH_NOT_FOUND) return TRUE;
    }
    return FALSE;
}

static BOOL browser_file(const WCHAR *root,BOOL profile,WCHAR *file) {
    if (lstrlenW(root)>MAX_PATH-80) return FALSE;
    lstrcpyW(file,root); lstrcatW(file,L"\\Local State");
    if (!profile) return TRUE;
    JsonDoc doc={0}; BOOL ok=open_doc(&doc,file,FALSE);
    WCHAR name[64]=L"Default";
    if (ok) {
        int p=json_member(&doc,0,"profile",7);
        int i=p<0?p:json_member(&doc,p,"last_used",9);
        if (i==-2) ok=FALSE;
        if (i>=0) {
            jsmntok_t token=doc.tokens[i]; int n=token.end-token.start;
            ok=token.type==JSMN_STRING && n>0 && n<64;
            if (ok) {
                for (int j=0;j<n;++j) name[j]=(BYTE)doc.bytes[token.start+j];
                name[n]=0;
                if (lstrcmpW(name,L"Default")) {
                    ok=n>8 && bytes_match((const char *)name,(const char *)L"Profile ",8*sizeof(WCHAR));
                    for (int j=8;j<n;++j) if (name[j]<'0'||name[j]>'9') ok=FALSE;
                }
            }
        }
    }
    close_doc(&doc);
    if (ok) {lstrcpyW(file,root);lstrcatW(file,L"\\");lstrcatW(file,name);lstrcatW(file,L"\\Preferences");}
    return ok;
}

static BOOL write_piece(HANDLE file,const char *bytes,DWORD size) {
    DWORD written;
    return WriteFile(file,bytes,size,&written,NULL) && written==size;
}

static BOOL publish_pref(const WCHAR *file,const JsonDoc *doc,int begin,int end,const char *replacement) {
    WCHAR temp[MAX_PATH],folder[MAX_PATH];
    lstrcpyW(folder,file);
    WCHAR *slash=folder+lstrlenW(folder);
    while (slash>folder && slash[-1]!='\\') --slash;
    *slash=0;
    if (!GetTempFileNameW(folder,L"O3P",0,temp)) return FALSE;
    HANDLE h=CreateFileW(temp,GENERIC_WRITE,0,NULL,TRUNCATE_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
    BOOL ok=FALSE;
    if (h!=INVALID_HANDLE_VALUE) {
        ok=write_piece(h,doc->bytes,begin) && write_piece(h,replacement,lstrlenA(replacement)) &&
           write_piece(h,doc->bytes+end,doc->size-end) && FlushFileBuffers(h);
        CloseHandle(h);
    }
    if (ok) ok=ReplaceFileW(file,temp,NULL,0,NULL,NULL);
    if (!ok) DeleteFileW(temp);
    return ok;
}

static unsigned browser_value(const JsonDoc *doc,int index,const BrowserSetting *s) {
    const jsmntok_t *t=doc->tokens+index;
    int len=t->end-t->start;
    const char *v=doc->bytes+t->start;
    if (t->type!=JSMN_PRIMITIVE) return failed;
    if (len==lstrlenA(s->no) && bytes_match(v,s->no,len)) return off;
    if (len==lstrlenA(s->yes) && bytes_match(v,s->yes,len)) return on;
    if (len==1 && ((s->id==100 && *v=='1') || (s->id==101 && (*v=='2'||*v=='3')) || (s->id==102 && (*v=='1'||*v=='3')))) return on;
    return failed;
}

static unsigned access_pref(const WCHAR *file,const BrowserSetting *s,int requested,BYTE *source) {
    JsonDoc doc={0}; unsigned state=failed;
    if (open_doc(&doc,file,requested>=0)) {
        const char *path=s->path;
        int parent=0;
        for (;;) {
            int n=0;
            while (path[n] && path[n]!='.') ++n;
            int item=json_member(&doc,parent,path,n);
            if (item==-2) break;
            if (item==-1 || !path[n]) {
                state=item==-1?s->fallback:browser_value(&doc,item,s);
                *source=item==-1?4:6;
                if (requested<0 || state==failed || state==(unsigned)(requested?on:off)) break;
                const char *value=requested?s->yes:s->no;
                char insert[256];
                int a,b;
                if (item>=0) {a=doc.tokens[item].start;b=doc.tokens[item].end;lstrcpyA(insert,value);}
                else {
                    a=b=doc.tokens[parent].end-1;
                    char *dst=insert;
                    if (doc.tokens[parent].size) *dst++=',';
                    unsigned nested=0;
                    do {
                        *dst++='"';
                        while (*path && *path!='.') *dst++=*path++;
                        *dst++='"'; *dst++=':';
                        if (!*path) break;
                        ++path;*dst++='{';++nested;
                    } while (*path);
                    lstrcpyA(dst,value);dst+=lstrlenA(value);
                    while (nested--) *dst++='}';
                    *dst=0;
                }
                state=publish_pref(file,&doc,a,b,insert)?(unsigned)(requested?on:off):failed;
                break;
            }
            parent=item; path+=n+1;
        }
    }
    close_doc(&doc);return state;
}

static unsigned browser_access(unsigned id,int requested,BYTE *state,BYTE *source) {
    const BrowserSetting *s=&browser_settings[id-93];
    BOOL edge=id<=98;
    WCHAR root[MAX_PATH],file[MAX_PATH];
    DWORD len=GetEnvironmentVariableW(L"LOCALAPPDATA",root,ARRAYSIZE(root));
    if (!len || len>MAX_PATH-100 || browser_locked(edge,s->policy)) return cannot_change;
    lstrcatW(root,edge?L"\\Microsoft\\Edge\\User Data":L"\\Google\\Chrome\\User Data");
    if (!browser_file(root,s->profile,file)) return cannot_change;
    *state=(BYTE)access_pref(file,s,-1,source);
    if (*state!=on && *state!=off) return cannot_change;
    if (requested<0 || *state==(requested?on:off)) return no_change;
    if (process_named(edge?L"msedge.exe":L"chrome.exe")) return browser_busy;
    if (access_pref(file,s,requested,source)==failed) return change_failed;
    *state=(BYTE)access_pref(file,s,-1,source);
    return *state==(requested?on:off)?changed_saved:change_failed;
}

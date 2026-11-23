#define _WIN32_WINNT 0x0A00
#include <windows.h>
#include <mmdeviceapi.h>
#include <audiopolicy.h>

static WCHAR selected[2048];
static char caption[512];
template<class T> struct ComRef {
    T *ptr=nullptr;
    ~ComRef(){if(ptr) ptr->Release();}
    T *operator->() const{return ptr;}
};
struct Apartment {
    HRESULT status=CoInitializeEx(nullptr,COINIT_MULTITHREADED);
    ~Apartment(){if(SUCCEEDED(status)) CoUninitialize();}
};
struct Text {
    WCHAR *ptr=nullptr;
    ~Text(){CoTaskMemFree(ptr);}
};
struct Sessions {
    ComRef<IMMDeviceEnumerator> devices;
    ComRef<IMMDevice> device;
    ComRef<IAudioSessionManager2> manager;
    ComRef<IAudioSessionEnumerator> list;
    int count=0;
    bool open(){
        return SUCCEEDED(CoCreateInstance(__uuidof(MMDeviceEnumerator),nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&devices.ptr))) &&
            SUCCEEDED(devices->GetDefaultAudioEndpoint(eRender,eMultimedia,&device.ptr)) &&
            SUCCEEDED(device->Activate(__uuidof(IAudioSessionManager2),CLSCTX_INPROC_SERVER,nullptr,(void**)&manager.ptr)) &&
            SUCCEEDED(manager->GetSessionEnumerator(&list.ptr)) && SUCCEEDED(list->GetCount(&count));
    }
    bool item(int index,ComRef<IAudioSessionControl2> &control,Text &identity,DWORD &pid) {
        ComRef<IAudioSessionControl> base;
        AudioSessionState state;
        return SUCCEEDED(list->GetSession(index,&base.ptr)) && base.ptr &&
            SUCCEEDED(base->QueryInterface(IID_PPV_ARGS(&control.ptr))) &&
            SUCCEEDED(control->GetState(&state)) && state!=AudioSessionStateExpired &&
            SUCCEEDED(control->GetProcessId(&pid)) && pid &&
            SUCCEEDED(control->GetSessionInstanceIdentifier(&identity.ptr)) && identity.ptr;
    }
};
extern "C" const char *o3_audio_label(){
    return selected[0] ? caption : "Klik nama fitur untuk memilih sesi audio";
}
extern "C" BOOL o3_audio_choose(HWND owner){
    Apartment apt; Sessions sessions;
    if (!sessions.open() || sessions.count<=0 || sessions.count>4096) return FALSE;
    struct Choice {WCHAR *identity; WCHAR title[360];};
    Choice *choices=(Choice*)HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,sessions.count*sizeof(Choice));
    if (!choices) return FALSE;
    HMENU menu=CreatePopupMenu();
    int count=0,pick=0;
    for (int i=0;menu && i<sessions.count;++i) {
        ComRef<IAudioSessionControl2> control; Text identity; DWORD pid=0;
        if (!sessions.item(i,control,identity,pid) || lstrlenW(identity.ptr)>=(int)ARRAYSIZE(selected)) continue;
        WCHAR exe[MAX_PATH]; DWORD len=ARRAYSIZE(exe);
        HANDLE process=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid);
        if (!process) continue;
        BOOL ok=QueryFullProcessImageNameW(process,0,exe,&len);
        CloseHandle(process);
        if (!ok) continue;
        const WCHAR *base=exe;
        for(const WCHAR *p=exe;*p;++p) if (*p=='\\') base=p+1;
        wsprintfW(choices[count].title,L"%s (PID %lu, sesi %d)",base,pid,i+1);
        if (!AppendMenuW(menu,MF_STRING|(!lstrcmpW(selected,identity.ptr)?MF_CHECKED:0),count+1,choices[count].title)) continue;
        choices[count++].identity=identity.ptr; identity.ptr=nullptr;
    }
    POINT point;
    if (count && GetCursorPos(&point)) pick=TrackPopupMenu(menu,TPM_RETURNCMD|TPM_NONOTIFY,point.x,point.y,0,owner,nullptr);
    if (pick>0 && pick<=count) {
        lstrcpyW(selected,choices[pick-1].identity);
        WideCharToMultiByte(CP_ACP,0,choices[pick-1].title,-1,caption,sizeof caption,nullptr,nullptr);
    }
    if(menu) DestroyMenu(menu);
    for(int i=0;i<count;++i) CoTaskMemFree(choices[i].identity);
    HeapFree(GetProcessHeap(),0,choices);
    return pick>0;
}
extern "C" int o3_audio_access(int requested){
    if (!selected[0]) return -1;
    Apartment apt; Sessions sessions;
    if (!sessions.open()) return -1;
    for(int i=0;i<sessions.count;++i){
        ComRef<IAudioSessionControl2> control; Text identity; DWORD pid=0;
        if (!sessions.item(i,control,identity,pid) || lstrcmpW(selected,identity.ptr)) continue;
        ComRef<ISimpleAudioVolume> volume;
        BOOL muted=FALSE;
        if (FAILED(control->QueryInterface(IID_PPV_ARGS(&volume.ptr))) || FAILED(volume->GetMute(&muted))) return -1;
        if (requested<0 || !!muted==requested) return !!muted;
        if (FAILED(volume->SetMute(requested,nullptr)) || FAILED(volume->GetMute(&muted))) return -1;
        return !!muted;
    }
    return -1;
}

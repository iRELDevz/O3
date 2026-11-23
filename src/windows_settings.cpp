#define _WIN32_WINNT 0x0A00
#include <windows.h>
#include <winstring.h>

#ifdef __MINGW32__
#define ____FIReference_1_boolean_INTERFACE_DEFINED__
#endif
#include <windows.foundation.h>
#include <roapi.h>
#include <windows.networking.connectivity.h>
#include <stdlib.h>
using ABI::Windows::Foundation::IPropertyValue;
using ABI::Windows::Foundation::IPropertyValueStatics;

struct SettingItem : IInspectable {
    virtual HRESULT STDMETHODCALLTYPE Id(HSTRING*)=0;
    virtual HRESULT STDMETHODCALLTYPE Type(int*)=0;
    virtual HRESULT STDMETHODCALLTYPE Policy(boolean*)=0;
    virtual HRESULT STDMETHODCALLTYPE Enabled(boolean*)=0;
    virtual HRESULT STDMETHODCALLTYPE Applicable(boolean*)=0;
    virtual HRESULT STDMETHODCALLTYPE Description(HSTRING*)=0;
    virtual HRESULT STDMETHODCALLTYPE Updating(boolean*)=0;
    virtual HRESULT STDMETHODCALLTYPE GetValue(HSTRING,IInspectable**)=0;
    virtual HRESULT STDMETHODCALLTYPE SetValue(HSTRING,IInspectable*)=0;
};
struct SettingsDatabase : IInspectable {
    virtual HRESULT STDMETHODCALLTYPE GetSetting(HSTRING,SettingItem**)=0;
};
static const GUID database_id={0xd68a97b7,0x4f47,0x4cb6,{0x9e,0x1f,0xa0,0x1c,0x12,0x32,0xf7,0x55}};
template<class T> struct Reference {
    T *ptr=nullptr;
    ~Reference(){if(ptr) ptr->Release();}
    T *operator->() const {return ptr;}
};
struct String {
    HSTRING value=nullptr;
    explicit String(const WCHAR *s){WindowsCreateString(s,lstrlenW(s),&value);}
    ~String(){WindowsDeleteString(value);}
};

struct Entry {int id; const WCHAR *name;};
static const Entry entries[]={
    {38,L"SystemSettings_Notifications_DoNotDisturb_ToggleLegacy"},
    {40,L"SystemSettings_QuietMoments_On_Presentation_Enabled"},
    {41,L"SystemSettings_QuietMoments_On_Game_Enabled"},
    {42,L"SystemSettings_QuietMoments_On_FullScreen_Enabled"},
    {43,L"SystemSettings_QuietMoments_On_OOBE_Enabled"},
    {44,L"SystemSettings_FocusAssist_FocusTheme_FocusSession_Enabled"},
    {45,L"SystemSettings_FocusAssist_FocusTheme_HideTaskbarBadges_Enabled"},
    {46,L"SystemSettings_FocusAssist_FocusTheme_HideTaskbarFlashes_Enabled"},
    {47,L"SystemSettings_FocusAssist_FocusTheme_MuteNotifications_Enabled"},
    {64,L"SystemSettings_Start_PlacesSettings"},
    {85,L"SystemSettings_Accessibility_Caption_LiveCaptionsQuickAction"},
    {91,L"SystemSettings_DeliveryOptimization_IsEnabled2"},
    {92,L"SystemSettings_MusUpdate_AllowAutoWindowsUpdateDownloadOverMeteredNetwork"}
};

static bool read_bool(SettingItem *item,HSTRING key,boolean &value) {
    Reference<IInspectable> boxed;
    Reference<IPropertyValue> property;
    return SUCCEEDED(item->GetValue(key,&boxed.ptr)) && boxed.ptr &&
        SUCCEEDED(boxed->QueryInterface(IID_PPV_ARGS(&property.ptr))) &&
        SUCCEEDED(property->GetBoolean(&value));
}

static int run(const Entry &entry,int requested) {
    String class_name(L"SystemSettings.DataModel.SettingsDatabase"), name(entry.name), key(L"Value");
    Reference<IInspectable> object;
    Reference<SettingsDatabase> db;
    Reference<SettingItem> item;
    if (FAILED(RoActivateInstance(class_name.value,&object.ptr)) ||
        FAILED(object->QueryInterface(database_id,(void**)&db.ptr)) ||
        FAILED(db->GetSetting(name.value,&item.ptr)) || !item.ptr) return 30;

    ULONGLONG deadline=GetTickCount64()+2500;
    boolean enabled=0, applicable=0, policy=0, updating=0, value=0;
    int type=0;
    do {
        if (SUCCEEDED(item->Type(&type)) && type==3 && SUCCEEDED(item->Applicable(&applicable)) && applicable &&
            SUCCEEDED(item->Updating(&updating)) && !updating && read_bool(item.ptr,key.value,value)) break;
        if (GetTickCount64()>=deadline) return 30;
        Sleep(25);
    } while(true);
    bool writable=SUCCEEDED(item->Policy(&policy)) && !policy && SUCCEEDED(item->Enabled(&enabled)) && enabled;
    if (requested<0 || !writable || value==requested) return (writable?10:20)+(value?1:2);
    String factory_name(L"Windows.Foundation.PropertyValue");
    Reference<IPropertyValueStatics> factory;
    Reference<IInspectable> boxed;
    if (FAILED(RoGetActivationFactory(factory_name.value,IID_PPV_ARGS(&factory.ptr))) ||
        FAILED(factory->CreateBoolean((boolean)requested,&boxed.ptr)) ||
        FAILED(item->SetValue(key.value,boxed.ptr))) return 31;
    deadline=GetTickCount64()+1400;
    do {
        if (SUCCEEDED(item->Updating(&updating)) && !updating && read_bool(item.ptr,key.value,value) &&
            value==requested) return value?11:12;
        Sleep(35);
    } while(GetTickCount64()<deadline);
    return 31;
}

static int connection_cost(int requested) {
    using namespace ABI::Windows::Networking::Connectivity;
    String name(L"Windows.Networking.Connectivity.NetworkInformation");
    Reference<INetworkInformationStatics> network;
    Reference<IConnectionProfile> profile;
    Reference<INetworkAdapter> adapter;
    GUID id={};
    if (FAILED(RoGetActivationFactory(name.value,IID_PPV_ARGS(&network.ptr))) ||
        FAILED(network->GetInternetConnectionProfile(&profile.ptr)) || !profile.ptr ||
        FAILED(profile->get_NetworkAdapter(&adapter.ptr)) || !adapter.ptr ||
        FAILED(adapter->get_NetworkAdapterId(&id))) return 30;
    HSTRING profile_name=nullptr;
    if (FAILED(profile->get_ProfileName(&profile_name)) || !profile_name) return 30;
    const WCHAR *target=WindowsGetStringRawBuffer(profile_name,nullptr);

    struct Cost {DWORD flags,source;};
    HMODULE library=LoadLibraryExW(L"wcmapi.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
    int result=30;
    if (library) {
        using Query=DWORD (WINAPI *)(const GUID*,LPCWSTR,int,void*,DWORD*,BYTE**);
        using Set=DWORD (WINAPI *)(const GUID*,LPCWSTR,int,void*,DWORD,const BYTE*);
        using Free=void (WINAPI *)(void*);
        auto query=(Query)(void*)GetProcAddress(library,"WcmQueryProperty");
        auto set=(Set)(void*)GetProcAddress(library,"WcmSetProperty");
        auto release=(Free)(void*)GetProcAddress(library,"WcmFreeMemory");
        for (int pass=0;query && set && release && pass<2;++pass) {
            BYTE *bytes=nullptr; DWORD size=0;
            if (query(&id,target,4,nullptr,&size,&bytes)!=0) break;
            if (!bytes || size!=sizeof(Cost)) {if(bytes) release(bytes);break;}
            Cost cost=*(const Cost*)bytes;release(bytes);
            DWORD kind=cost.flags&0xffff;
            if (kind!=1 && kind!=2 && kind!=4) break;
            bool enabled=kind!=1;
            bool writable=cost.source==0 || cost.source==2;
            result=(writable?10:20)+(enabled?1:2);
            if (requested<0 || enabled==(bool)requested || !writable || pass!=0) break;
            cost.flags=requested?2:1;
            cost.source=2;
            result=31;
            if (set(&id,target,4,nullptr,sizeof cost,(BYTE*)&cost)!=0) break;
        }
        FreeLibrary(library);
    }
    WindowsDeleteString(profile_name);
    return result;
}

int main(int argc,char **argv) {
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX|SEM_NOOPENFILEERRORBOX);
    if (argc!=3 || (lstrcmpA(argv[2],"read") && lstrcmpA(argv[2],"on") && lstrcmpA(argv[2],"off"))) return 30;
    int id=atoi(argv[1]), result=30;
    if (FAILED(RoInitialize(RO_INIT_MULTITHREADED))) return 30;
    if (id==90) result=connection_cost(!lstrcmpA(argv[2],"read")?-1:!lstrcmpA(argv[2],"on"));
    for (const auto &entry:entries) if (entry.id==id) {
        result=run(entry,!lstrcmpA(argv[2],"read")?-1:!lstrcmpA(argv[2],"on"));break;
    }
    RoUninitialize();
    return result;
}

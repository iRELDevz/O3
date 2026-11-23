static BOOL adb_transfer(SOCKET s,char *buf,int n,BOOL writing) {
    while (n>0) {
        int bytes=writing ? send(s,buf,n,0) : recv(s,buf,n,0);
        if (bytes<=0) return FALSE;
        buf+=bytes;n-=bytes;
    }
    return TRUE;
}
static BOOL adb_request(SOCKET s,const char *command_text) {
    char packet[160],response[4];
    int n=lstrlenA(command_text);
    if (n>(int)sizeof packet-6) return FALSE;
    wsprintfA(packet,"%04x%s",n,command_text);
    return adb_transfer(s,packet,n+4,TRUE) && adb_transfer(s,response,4,FALSE) && bytes_match(response,"OKAY",4);
}
static BOOL show_taps(const char *transport,int requested,BYTE *state) {
    SOCKET sock=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
    if (sock==INVALID_SOCKET) return FALSE;
    DWORD timeout=1000;
    setsockopt(sock,SOL_SOCKET,SO_RCVTIMEO,(char*)&timeout,sizeof timeout);
    setsockopt(sock,SOL_SOCKET,SO_SNDTIMEO,(char*)&timeout,sizeof timeout);
    struct sockaddr_in server={0};
    server.sin_family=AF_INET;server.sin_port=htons(5037);server.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
    const char *cmd=requested<0 ? "shell:settings get system show_touches" :
        requested ? "shell:settings put system show_touches 1" : "shell:settings put system show_touches 0";
    BOOL ok=connect(sock,(struct sockaddr*)&server,sizeof server)==0 && adb_request(sock,transport) && adb_request(sock,cmd);
    char output_text[32];
    int size=0;
    while(ok && size<(int)sizeof output_text-1) {
        int got=recv(sock,output_text+size,sizeof output_text-1-size,0);
        if (!got) break;
        if (got<0) {ok=FALSE;break;}
        size+=got;
    }
    closesocket(sock);
    if (!ok || size==(int)sizeof output_text-1) return FALSE;
    while(size && (output_text[size-1]=='\r'||output_text[size-1]=='\n')) --size;
    output_text[size]=0;
    if (requested>=0) return size==0;
    if (!lstrcmpA(output_text,"1")) *state=on;
    else if (!lstrcmpA(output_text,"0") || !lstrcmpA(output_text,"null")) *state=off;
    else return FALSE;
    return TRUE;
}
static unsigned android_touch(int requested,BYTE *state,BYTE *source) {
    *state=unavailable;*source=0;
    if (!*emulator_instance || !process_named(L"HD-Player.exe") || !load_config(emulator_file)) return cannot_change;
    DWORD count=lstrlenA(config_text);
    LONG adb=bit_offset(config_text,count,"bst.enable_adb_access");
    if (adb<0 || config_text[adb]!='1') return cannot_change;
    char key[128],transport[64];
    wsprintfA(key,"bst.instance.%s.status.adb_port=\"",emulator_instance);
    unsigned port=0;
    for (const char *line=config_text;*line;) {
        if (prefix(line,key)) {
            if (port) return cannot_change;
            const char *c=line+lstrlenA(key); unsigned digits=0;
            while (*c>='0' && *c<='9' && digits<5) {port=port*10+*c++-'0';++digits;}
            if (!port || port>65535 || *c++!='"' || (*c && *c!='\r' && *c!='\n')) return cannot_change;
        }
        while (*line && *line!='\n') ++line;
        if (*line) ++line;
    }
    if (!port) return cannot_change;
    wsprintfA(transport,"host:transport:127.0.0.1:%u",port);
    WSADATA data;
    if (WSAStartup(MAKEWORD(2,2),&data)) return cannot_change;
    unsigned result=cannot_change;
    if (show_taps(transport,-1,state)) {
        *source=8;
        if (requested<0 || *state==(requested?on:off)) result=no_change;
        else if (show_taps(transport,requested,state) && show_taps(transport,-1,state) && *state==(requested?on:off)) result=changed_live;
        else result=change_failed;
    }
    WSACleanup();
    return result;
}

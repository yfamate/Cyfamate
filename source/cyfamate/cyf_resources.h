// Included in Cyfamate's namespace. Threads is the total compute-worker budget.
struct CpuBudget { int total, alpha, mate, share; };
// [MT-SHARE 2026-10-14] share=0: Zyfamate threads are reserved out of Threads (alpha=Threads-mate, legacy).
//   share=1: alpha=Threads; the last `mate` helpers are elastic and park only while a Zyfamate solve is running.
//   share=2: alpha=Threads; Zyfamate runs as extra threads at below-normal OS priority.
inline CpuBudget cpu_budget(int total, bool enabled, int requested=0, int share=0){
    total=std::max(1,total);
    const int wanted=requested>0?requested:std::clamp(total/16,1,8);
    const int mate=enabled && total>1?std::min(wanted,total-1):0;
    share=mate>0?std::clamp(share,0,2):0;
    return {total,share?total:total-mate,mate,share};
}
inline CpuBudget& active_cpu_budget(){static CpuBudget b{2,2,0,0};return b;}
// Zyfamate solves currently consuming a CPU (service lanes, MateWorker, MPON, portfolio children).
inline std::atomic<int>& zyf_cpu_busy(){static std::atomic<int> n{0};return n;}
struct ZyfCpuGuard {
    int n;
    explicit ZyfCpuGuard(int k=1):n(k){zyf_cpu_busy().fetch_add(n,std::memory_order_relaxed);}
    ~ZyfCpuGuard(){zyf_cpu_busy().fetch_sub(n,std::memory_order_relaxed);}
    ZyfCpuGuard(const ZyfCpuGuard&)=delete; ZyfCpuGuard& operator=(const ZyfCpuGuard&)=delete;
};
// share=2: a Zyfamate compute thread yields to search threads.
inline void zyf_thread_priority(){
    SetThreadPriority(GetCurrentThread(),active_cpu_budget().share==2?THREAD_PRIORITY_BELOW_NORMAL:THREAD_PRIORITY_NORMAL);
}

// Each child owns all Zyfamate globals and tables. The coordinator only waits;
// it does not run an additional solver. No shell and no shared solver state.
struct ZyfProcess {
    HANDLE process=nullptr, output=nullptr, job=nullptr;
    std::string text;
    bool start(const std::string& sfen,int variant,int hash,long nodes,double seconds){
        char exe[32768];if(!GetModuleFileNameA(nullptr,exe,sizeof(exe)))return false;
        if(sfen.find_first_of("\"\r\n")!=std::string::npos)return false;
        SECURITY_ATTRIBUTES sa{sizeof(sa),nullptr,TRUE};HANDLE write=nullptr;
        if(!CreatePipe(&output,&write,&sa,0))return false;
        SetHandleInformation(output,HANDLE_FLAG_INHERIT,0);
        HANDLE input=CreateFileA("NUL",GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,&sa,OPEN_EXISTING,0,nullptr);
        STARTUPINFOA si{};si.cb=sizeof(si);si.dwFlags=STARTF_USESTDHANDLES;
        si.hStdInput=input;si.hStdOutput=write;si.hStdError=write;
        PROCESS_INFORMATION pi{};
        std::string cmd="\""+std::string(exe)+"\" --cyf-zyf-worker "+std::to_string(variant)+" "+
            std::to_string(hash)+" "+std::to_string(nodes)+" "+std::to_string(seconds)+" \""+sfen+"\"";
        const bool ok=CreateProcessA(exe,cmd.data(),nullptr,nullptr,TRUE,CREATE_NO_WINDOW|CREATE_SUSPENDED,
                                     nullptr,nullptr,&si,&pi)!=0;
        CloseHandle(write);if(input!=INVALID_HANDLE_VALUE)CloseHandle(input);
        if(!ok){CloseHandle(output);output=nullptr;return false;}
        process=pi.hProcess;job=CreateJobObjectA(nullptr,nullptr);
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
        limits.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        if(!job || !SetInformationJobObject(job,JobObjectExtendedLimitInformation,&limits,sizeof(limits))
                || !AssignProcessToJobObject(job,process)){
            TerminateProcess(process,1);CloseHandle(pi.hThread);return false;
        }
        ResumeThread(pi.hThread);CloseHandle(pi.hThread);return true;
    }
    bool poll(){
        DWORD available=0,read=0;char buffer[4096];
        while(PeekNamedPipe(output,nullptr,0,nullptr,&available,nullptr)&&available){
            if(!ReadFile(output,buffer,std::min<DWORD>(available,sizeof(buffer)),&read,nullptr))break;
            text.append(buffer,read);if(text.size()>65536)text.erase(0,text.size()-65536);
        }
        return WaitForSingleObject(process,0)==WAIT_OBJECT_0;
    }
    ~ZyfProcess(){
        if(process){if(WaitForSingleObject(process,0)==WAIT_TIMEOUT)TerminateProcess(process,1);
            WaitForSingleObject(process,5000);CloseHandle(process);}
        if(output)CloseHandle(output);
        if(job)CloseHandle(job);
    }
};

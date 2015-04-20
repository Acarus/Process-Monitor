#include "ProcessMonitor.h"

namespace monitor {

ProcessMonitor::ProcessMonitor(std::wstring path, std::wstring args)
	: _cmd_line(path+L" "+args), 
	_logger(nullptr),
	_event_manager(nullptr),
	_monitor_thread(),
	_hande(NULL),
	_m_mutex() {				
}


ProcessMonitor::~ProcessMonitor(void) {
	if(this->_logger != nullptr) this->_logger->Release();
	if(this->_event_manager != nullptr) this->_event_manager->Release();

	this->_watching = false;
	if(this->_monitor_thread.joinable()) this->_monitor_thread.join();
	
	CloseHandle(this->_hande);
}

void ProcessMonitor::Start(std::wstring path, std::wstring args) { 
	// if we want start new process
	if(path.size() > 0) { 
		this->_cmd_line = path+L" "+args;
	} else { 
		// resume already stared process
		// get current state and process id
		this->_m_mutex.lock();
		State state = this->_state;
		DWORD p_id = this->_p_id;
		this->_m_mutex.unlock();

		// resume process
		if(state == State::STOPPED) {
			std::wstringstream ss;

			if(this->_StopResumeProcess(p_id, true)) {
				// set state "Working"
				this->_m_mutex.lock();
				this->_state = State::WORKING;
				this->_m_mutex.unlock();

				ss << L"Successfully resumed process(id = " << p_id << L")";
				LOG_IT(ss.str());
				EMIT(L"OnProcResumed", p_id);
			} else {
				ss << L"Could not resume process(id = " << p_id << L")";
				LOG_IT(ss.str());
			}
			return;
		}
	}

	// start process
	STARTUPINFO si = {};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi = {};

	if (CreateProcess(0, (LPWSTR)this->_cmd_line.c_str(), 0, FALSE, 0, 0, 0, 0, &si, &pi)) {		
		this->_m_mutex.lock();
		this->_state = State::WORKING;
		this->_p_id = pi.dwProcessId;
		this->_hande = pi.hProcess;
		this->_m_mutex.unlock();

		// start monitoring thread
		if(!this->_monitor_thread.joinable()) {
			this->_watching = true;
			this->_monitor_thread = std::thread(&ProcessMonitor::_MonitoringProcess, this);
		}

		LOG_IT(L"Started process( cmd line = "+this->_cmd_line+L")");
		EMIT(L"OnProcStart", this->_p_id);

	} else {
		LOG_IT(L"Can't start process");		
	}
}
  
ProcessInfo ProcessMonitor::GetProcessInfo() {
	this->_m_mutex.lock();
	ProcessInfo info = ProcessInfo(this->_hande, this->_p_id, this->_state);
	this->_m_mutex.unlock();

	return info;
}
 
void ProcessMonitor::SetLogger(Logger *logger) {
	this->_m_mutex.lock();
	this->_logger = logger;
	this->_logger->AddRef();
	this->_m_mutex.unlock();
}

void ProcessMonitor::SetEventManager(EventManager *event_manager) {
	this->_m_mutex.lock();
	this->_event_manager = event_manager;
	this->_event_manager->AddRef();
	// set events that we want generate
	this->_event_manager->AddEvent(L"OnProcStart");
	this->_event_manager->AddEvent(L"OnProcExitFailure");
	this->_event_manager->AddEvent(L"OnProcExitSuccess");
	this->_event_manager->AddEvent(L"OnProcAttached");
	this->_event_manager->AddEvent(L"OnProcRestart");
	this->_event_manager->AddEvent(L"OnProcStopped");
	this->_event_manager->AddEvent(L"OnProcResumed");

	this->_m_mutex.unlock();
}

void ProcessMonitor::_MonitoringProcess() {
	DWORD exit_code = STILL_ACTIVE;
	DWORD old_exit_code = STILL_ACTIVE;

	while(1) {
		this->_m_mutex.lock();

		if(!this->_watching){
			this->_m_mutex.unlock();
			break;
		}

		std::wstringstream ss;		
		// looking if state was changed
		if(GetExitCodeProcess(this->_hande, &exit_code) && exit_code != old_exit_code) {
			switch(exit_code) { 
				case EXIT_SUCCESS:
					this->_state = State::SUCCESS_EXIT;
					ss << L"The process(id = ";
					ss << this->_p_id;
					ss << L") is not completed successfully"; 
					LOG_IT(ss.str());
					ss.clear();
					EMIT(L"OnProcExitSuccess", this->_p_id);
				break;

				case EXIT_FAILURE:
					this->_state = State::FAILE_EXIT;
					ss << L"The process(id = ";
					ss << this->_p_id;
					ss << L") is not completed successfully"; 
					LOG_IT(ss.str());
					EMIT(L"OnProcExitFailure", this->_p_id);
				break;
			}
		
			old_exit_code = exit_code;
			// if process was exited that restart it
			if( (exit_code == EXIT_SUCCESS) || (exit_code == EXIT_FAILURE) ) {
				this->_m_mutex.unlock();
				_Restart();	
				continue;
			}
		}

		this->_m_mutex.unlock();
	}
}

void ProcessMonitor::_Restart() {
	CloseHandle(this->_hande);

	this->_m_mutex.lock();
	this->_state = State::RESTARTING;
	this->_m_mutex.unlock();
	
	LOG_IT(L"Restarting process( cmd line = "+this->_cmd_line+L")");
	EMIT(L"OnProcRestart", this->_p_id);
	Start();
}


void ProcessMonitor::Restart() {
	// if process isn't started - Start it
	if(!this->_monitor_thread.joinable()) {
		Start();
	} else { 
	// simply terminate the process and _MonitoringProcess restart it 
		TerminateProcess(this->_hande, 0);
	}
}

void ProcessMonitor::AttachTo(DWORD p_id) {
	// open process
    HANDLE handle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, p_id);
    DWORD err = 0;
    if (handle == NULL) {
		std::wstringstream ss;
		ss << L"Can't open process(ID = " << p_id << L")";
		LOG_IT((std::wstring)ss.str());
		return;
	}

    // determine if 64 or 32-bit processor
    SYSTEM_INFO si;
    GetNativeSystemInfo(&si);

    // determine if this process is running on WOW64
    BOOL wow;
    IsWow64Process(GetCurrentProcess(), &wow);

    DWORD process_parameters_offset = si.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64 ? 0x20 : 0x10;
    DWORD command_line_offset = si.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64 ? 0x70 : 0x40;

    // read basic info to get process parameters address
    DWORD peb_size = process_parameters_offset + 8;
    PBYTE peb = new BYTE[peb_size];
    ZeroMemory(peb, peb_size);

    // read basic info to get command line address 
    DWORD pp_size = command_line_offset + 16;
    PBYTE pp = new BYTE[pp_size];
    ZeroMemory(pp, pp_size);

    PWSTR cmd_line;

    if (wow) {
        // we're running as a 32-bit process in a 64-bit OS
		PROCESS_BASIC_INFORMATION_WOW64 pbi;
        ZeroMemory(&pbi, sizeof(pbi));

        // get process information from 64-bit world
        _NtQueryInformationProcess query = (_NtQueryInformationProcess)GetProcAddress(GetModuleHandleA("ntdll.dll"), "NtWow64QueryInformationProcess64");
        err = query(handle, 0, &pbi, sizeof(pbi), NULL);
        if (err != 0) {
            LOG_IT(L"NtWow64QueryInformationProcess64 failed");
            CloseHandle(handle);
            return;
        }

        // read PEB from 64-bit address space
        _NtWow64ReadVirtualMemory64 read = (_NtWow64ReadVirtualMemory64)GetProcAddress(GetModuleHandleA("ntdll.dll"), "NtWow64ReadVirtualMemory64");
        err = read(handle, pbi.PebBaseAddress, peb, peb_size, NULL);
        if (err != 0) {
            LOG_IT(L"NtWow64ReadVirtualMemory64 PEB failed");
            CloseHandle(handle);
            return;
        }

        // read process parameters from 64-bit address space
        PBYTE* parameters = (PBYTE*)*(LPVOID*)(peb + process_parameters_offset); // address in remote process adress space
        err = read(handle, parameters, pp, pp_size, NULL);
        if (err != 0) {
            LOG_IT(L"NtWow64ReadVirtualMemory64 Parameters failed");
            CloseHandle(handle);
            return;
        }

        // read command line
        UNICODE_STRING_WOW64* p_command_line = (UNICODE_STRING_WOW64*)(pp + command_line_offset);
        cmd_line = (PWSTR)malloc(p_command_line->MaximumLength);
        err = read(handle, p_command_line->Buffer, cmd_line, p_command_line->MaximumLength, NULL);
        if (err != 0) {
            LOG_IT(L"NtWow64ReadVirtualMemory64 Parameters failed");
            CloseHandle(handle);
            return;
        }

    } else {
        // we're running as a 32-bit process in a 32-bit OS, or as a 64-bit process in a 64-bit OS
        PROCESS_BASIC_INFORMATION pbi;
        ZeroMemory(&pbi, sizeof(pbi));

        // get process information
        _NtQueryInformationProcess query = (_NtQueryInformationProcess)GetProcAddress(GetModuleHandleA("ntdll.dll"), "NtQueryInformationProcess");
        err = query(handle, 0, &pbi, sizeof(pbi), NULL);
        if (err != 0) {
            LOG_IT(L"NtQueryInformationProcess failed");
            CloseHandle(handle);
            return;
        }

        // read PEB
        if (!ReadProcessMemory(handle, pbi.PebBaseAddress, peb, peb_size, NULL)) {
            LOG_IT(L"ReadProcessMemory PEB failed");
            CloseHandle(handle);
            return;
        }

        // read process parameters
        PBYTE* parameters = (PBYTE*)*(LPVOID*)(peb + process_parameters_offset); // address in remote process adress space
        if (!ReadProcessMemory(handle, parameters, pp, pp_size, NULL)) {
            LOG_IT(L"ReadProcessMemory Parameters failed\n");
            CloseHandle(handle);
            return;
        }

        // read command line
        UNICODE_STRING* p_command_line = (UNICODE_STRING*)(pp + command_line_offset);
        cmd_line = (PWSTR)new wchar_t(p_command_line->MaximumLength);
        if (!ReadProcessMemory(handle, p_command_line->Buffer, cmd_line, p_command_line->MaximumLength, NULL)) {
            LOG_IT(L"ReadProcessMemory Parameters failed");
            CloseHandle(handle);
            return;
        }
    }

	std::wstringstream ss;
	ss << L"Attached to process(pId = " << p_id << L")";
	LOG_IT((std::wstring)ss.str());
	EMIT(L"OnProcAttached", p_id);

	this->_m_mutex.lock();
	
	this->_cmd_line = cmd_line;
	if(this->_hande != NULL) CloseHandle(this->_hande);
	this->_hande = handle;
	this->_p_id = p_id;
	this->_state = State::WORKING;
	
	this->_m_mutex.unlock();
	
	// start monitoring thread
	if(!this->_monitor_thread.joinable()) {
		this->_watching = true;
		this->_monitor_thread = std::thread(&ProcessMonitor::_MonitoringProcess, this);
	}
}

bool ProcessMonitor::_StopResumeProcess(DWORD p_id, bool resume)
{ 
    HANDLE        thread_snap = NULL; 
    THREADENTRY32 te32        = {0}; 
	bool return_value;

    // Take a snapshot of all threads currently in the system. 
    thread_snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0); 
    if (thread_snap == INVALID_HANDLE_VALUE) 
        return false; 
 
    // fill in the size of the structure  
    te32.dwSize = sizeof(THREADENTRY32); 
	
    // walk the thread snapshot to find all threads of the process 
    if (Thread32First(thread_snap, &te32)) 
    { 
        do 
        { 
            if (te32.th32OwnerProcessID == p_id) 
            {
				HANDLE thread_handle = OpenThread(THREAD_SUSPEND_RESUME, false, te32.th32ThreadID);
				if (resume)
				{
					ResumeThread(thread_handle);
				}
				else
				{
					SuspendThread(thread_handle);
				}
				CloseHandle(thread_handle);
            } 
        }
        while (Thread32Next(thread_snap, &te32)); 
        
		return_value = true;
    } else {
		return_value = false;
	}

	CloseHandle (thread_snap);

    return return_value; 
}

void ProcessMonitor::Stop() {
	// get current process id
	this->_m_mutex.lock();
	DWORD p_id = this->_p_id;
	this->_m_mutex.unlock();

	std::wstringstream ss;

	if(this->_StopResumeProcess(p_id, false)) {
		// set state "Stopped"
		this->_m_mutex.lock();
		this->_state = State::STOPPED;
		this->_m_mutex.unlock();

		ss << L"Process(id = " << p_id << L") stopped";
		LOG_IT(ss.str());
		EMIT(L"OnProcStopped", p_id);
	} else {
		ss << L"Unable to stop the process(id = " << p_id;
		LOG_IT(ss.str());
	}
}

}
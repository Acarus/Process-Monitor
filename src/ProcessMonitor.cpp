#include "ProcessMonitor.h"

namespace monitor {

ProcessMonitor::ProcessMonitor(std::wstring path, std::wstring args)
	: _cmd_line(path+L" "+args), 
	_logger(nullptr),
	_event_manager(nullptr),
	_monitor_thread(),
	_handle(NULL),
	_m_mutex() {				
}


ProcessMonitor::~ProcessMonitor(void) {
	this->_watching = false;
	if(this->_monitor_thread.joinable()) this->_monitor_thread.join();
	if(this->_handle != NULL) CloseHandle(this->_handle);
}

void ProcessMonitor::Start(std::wstring path, std::wstring args) { 
	// resume already started process
	if(this->_state == State::STOPPED && path.size() == 0) {
		this->_m_mutex.lock();
		if(this->_StopResumeProcess(this->_p_id, true)) {
				this->_state = State::WORKING;
				LOG_IT(_FormatedString(L"Successfully resumed process(id = :p_id)", this->_p_id));
				EMIT(L"OnProcResumed", this->_p_id);
			} else {
				LOG_IT(_FormatedString(L"Could not resume process(id = :p_id)", this->_p_id));
			}
		this->_m_mutex.unlock();

	} else {
		// If the process is already running close handle
		if(this->_state == State::WORKING) {
			this->_m_mutex.lock();
			CloseHandle(this->_handle);
			this->_handle = NULL;
			this->_m_mutex.unlock();
		}

		// set cmd line
		if(path.size() > 0)  this->_cmd_line = path+L" "+args;

		// start process
		STARTUPINFO si = {};
		si.cb = sizeof(si);
		PROCESS_INFORMATION pi = {};

		if (CreateProcess(0, (LPWSTR)this->_cmd_line.c_str(), 0, FALSE, 0, 0, 0, 0, &si, &pi)) {		
			this->_m_mutex.lock();
		
			this->_state = State::WORKING;
			this->_p_id = pi.dwProcessId;
			this->_handle = pi.hProcess;
			
			// start monitoring thread
			if(!this->_monitor_thread.joinable()) {
				this->_watching = true;
				this->_monitor_thread = std::thread(&ProcessMonitor::_MonitoringProcess, this);
			}
			this->_m_mutex.unlock();

			LOG_IT(L"Started process( cmd line = "+this->_cmd_line+L")");
			EMIT(L"OnProcStart", this->_p_id);

		} else {
			LOG_IT(L"Can't start process");		
		}
	}
}
  
ProcessInfo ProcessMonitor::GetProcessInfo() {
	std::lock_guard<std::mutex> lock(this->_m_mutex);
	ProcessInfo info = ProcessInfo(this->_handle, this->_p_id, this->_state);
	return info;
}
 
void ProcessMonitor::SetLogger(std::shared_ptr<Logger> logger) {
	this->_logger = logger;
}

void ProcessMonitor::SetEventManager(std::shared_ptr<EventManager> event_manager) {
	this->_event_manager = event_manager;
	// set events that we want generate
	this->_event_manager->AddEvent(L"OnProcStart");
	this->_event_manager->AddEvent(L"OnProcExitFailure");
	this->_event_manager->AddEvent(L"OnProcExitSuccess");
	this->_event_manager->AddEvent(L"OnProcAttached");
	this->_event_manager->AddEvent(L"OnProcAttachFailure");
	this->_event_manager->AddEvent(L"OnProcRestart");
	this->_event_manager->AddEvent(L"OnProcStopped");
	this->_event_manager->AddEvent(L"OnProcResumed");
}

void ProcessMonitor::_MonitoringProcess() {
	DWORD exit_code;

	while(true) {
		// wait end of process
		if(this->_handle != NULL && WaitForSingleObject(this->_handle, 1000) == WAIT_OBJECT_0) {
			// get exit code
			if(GetExitCodeProcess(this->_handle, &exit_code)) {
				std::lock_guard<std::mutex> lock(this->_m_mutex);
				if(exit_code == EXIT_SUCCESS) {
					this->_state = State::SUCCESS_EXIT;
					LOG_IT(_FormatedString(L"The process(id = :p_id) is completed successfully", this->_p_id));
					EMIT(L"OnProcExitSuccess", this->_p_id);
				} else {
					this->_state = State::FAILE_EXIT;
					LOG_IT(_FormatedString(L"The process(id = :p_id) isn't completed successfully", this->_p_id));
					EMIT(L"OnProcExitFailure", this->_p_id);
				}		
			}
			_Restart();
		}

		if(!this->_watching) break; 
	}
}

void ProcessMonitor::_Restart() {
	this->_m_mutex.lock();
	
	if(this->_handle != NULL) {
		CloseHandle(this->_handle);
		this->_handle = NULL;
	}
	this->_state = State::RESTARTING;
	LOG_IT(L"Restarting process( cmd line = "+this->_cmd_line+L")");
	EMIT(L"OnProcRestart", this->_p_id);

	this->_m_mutex.unlock();
	
	Start();
}


void ProcessMonitor::Restart() {
	// simply terminate process and _MonitoringProcess restart it 
	if(this->_state == State::WORKING){ 
		TerminateProcess(this->_handle, 0);
	} else {
	// if process isn't started - Start it
		Start();
	}
}

void ProcessMonitor::AttachTo(DWORD p_id) {
	// open process
    HANDLE handle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, p_id);
    DWORD err = 0;
    if (handle == NULL) {
		LOG_IT(_FormatedString(L"Can't open process(Id = :p_id)", p_id));
		EMIT(L"OnProcAttachFailure", p_id);
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
            EMIT(L"OnProcAttachFailure", p_id);
			CloseHandle(handle);
            return;
        }

        // read PEB from 64-bit address space
        _NtWow64ReadVirtualMemory64 read = (_NtWow64ReadVirtualMemory64)GetProcAddress(GetModuleHandleA("ntdll.dll"), "NtWow64ReadVirtualMemory64");
        err = read(handle, pbi.PebBaseAddress, peb, peb_size, NULL);
        if (err != 0) {
            LOG_IT(L"NtWow64ReadVirtualMemory64 PEB failed");
			EMIT(L"OnProcAttachFailure", p_id);
            CloseHandle(handle);
            return;
        }

        // read process parameters from 64-bit address space
        PBYTE* parameters = (PBYTE*)*(LPVOID*)(peb + process_parameters_offset); // address in remote process adress space
        err = read(handle, parameters, pp, pp_size, NULL);
        if (err != 0) {
            LOG_IT(L"NtWow64ReadVirtualMemory64 Parameters failed");
			EMIT(L"OnProcAttachFailure", p_id);
            CloseHandle(handle);
            return;
        }

        // read command line
        UNICODE_STRING_WOW64* p_command_line = (UNICODE_STRING_WOW64*)(pp + command_line_offset);
        cmd_line = (PWSTR)malloc(p_command_line->MaximumLength);
        err = read(handle, p_command_line->Buffer, cmd_line, p_command_line->MaximumLength, NULL);
        if (err != 0) {
            LOG_IT(L"NtWow64ReadVirtualMemory64 Parameters failed");
			EMIT(L"OnProcAttachFailure", p_id);
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
			EMIT(L"OnProcAttachFailure", p_id);
            CloseHandle(handle);
            return;
        }

        // read PEB
        if (!ReadProcessMemory(handle, pbi.PebBaseAddress, peb, peb_size, NULL)) {
            LOG_IT(L"ReadProcessMemory PEB failed");
			EMIT(L"OnProcAttachFailure", p_id);
            CloseHandle(handle);
            return;
        }

        // read process parameters
        PBYTE* parameters = (PBYTE*)*(LPVOID*)(peb + process_parameters_offset); // address in remote process adress space
        if (!ReadProcessMemory(handle, parameters, pp, pp_size, NULL)) {
            LOG_IT(L"ReadProcessMemory Parameters failed");
			EMIT(L"OnProcAttachFailure", p_id);
            CloseHandle(handle);
            return;
        }

        // read command line
        UNICODE_STRING* p_command_line = (UNICODE_STRING*)(pp + command_line_offset);
        cmd_line = (PWSTR)new wchar_t(p_command_line->MaximumLength);
        if (!ReadProcessMemory(handle, p_command_line->Buffer, cmd_line, p_command_line->MaximumLength, NULL)) {
            LOG_IT(L"ReadProcessMemory Parameters failed");
			EMIT(L"OnProcAttachFailure", p_id);
            CloseHandle(handle);
            return;
        }
    }
	LOG_IT(_FormatedString(L"Attached to process(pId = :p_id)", p_id));
	EMIT(L"OnProcAttached", p_id);

	this->_m_mutex.lock();
	
	this->_cmd_line = cmd_line;
	if(this->_handle != NULL) CloseHandle(this->_handle);
	this->_handle = handle;
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
    HANDLE thread_snap = NULL; 
    THREADENTRY32 te32 = {0}; 
	bool return_value;

    // Take a snapshot of all threads currently in the system. 
    thread_snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0); 
    if (thread_snap == INVALID_HANDLE_VALUE)  return false; 
 
    te32.dwSize = sizeof(THREADENTRY32); 
	
    // walk the thread snapshot to find all threads of the process 
    if (Thread32First(thread_snap, &te32)) { 
		do { 
			if (te32.th32OwnerProcessID == p_id) {
				HANDLE thread_handle = OpenThread(THREAD_SUSPEND_RESUME, false, te32.th32ThreadID);
				if (resume) {
					ResumeThread(thread_handle);
				} else {
					SuspendThread(thread_handle);
				}
				CloseHandle(thread_handle);
            } 
        } while (Thread32Next(thread_snap, &te32)); 
        
		return_value = true;
    } else {
		return_value = false;
	}
	CloseHandle (thread_snap);

    return return_value; 
}

void ProcessMonitor::Stop() {
	std::lock_guard<std::mutex> lock(this->_m_mutex);
	if(this->_StopResumeProcess(this->_p_id, false)) {
		this->_state = State::STOPPED;
		LOG_IT(_FormatedString(L"Process(id = :p_id) stopped", this->_p_id));
		EMIT(L"OnProcStopped", this->_p_id);
	} else {
		LOG_IT(_FormatedString(L"Unable to stop the process(id = :p_id)", this->_p_id));
	}
}

std::wstring ProcessMonitor::_FormatedString(std::wstring source, int p_id) {
	size_t pos = source.find(L":p_id");
	if(pos != std::wstring::npos) {
		std::wstringstream ss;
		ss << p_id;
		source.replace(pos, 5, ss.str());
	}
	return source;
}

}
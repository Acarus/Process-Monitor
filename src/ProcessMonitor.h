#ifndef MONITOR_PROCESSMONITOR_H
#define MONITOR_PROCESSMONITOR_H

#include "Logger.h"
#include "EventManager.h"
#include "Nt.h"
#include <tlhelp32.h>

#include <string>
#include <sstream>
#include <thread>
#include <mutex>

#define LOG_IT(msg) if(this->_logger != nullptr) this->_logger->Log(msg)
#define EMIT(e, p_id) if(this->_event_manager != nullptr) this->_event_manager->GenerateEvent(e, p_id)

namespace monitor {

enum State {
 RESTARTING, ///< Process is restarting
 WORKING, ///< Process is working
 STOPPED, ///< Process was stopped
 SUCCESS_EXIT, ///< Process was successfully exited
 FAILE_EXIT ///< Process was not successfully exited
};

struct ProcessInfo {
 HANDLE handle; ///< Windows process handle
 DWORD id;   ///< Windows process id
 State state; ///< Process state(working, restarting, stopped, exit)
 ProcessInfo(HANDLE lHandle, DWORD lId, State lState) {
	handle = lHandle;
	id = lId;
	state = lState;
 }
};

/**
@brief Class that allow monitoring and controll process 	
@detailed Class allow: 
 - start and restart process;
 - retrieve process info (handle, id, status (is working, restarting, stopped));
 - stop process via method call (without restart) and start it again;
 - log all events;
 - add callbacks to all events;
*/
class ProcessMonitor {
  std::wstring _cmd_line;
  HANDLE _hande;
  State _state;
  DWORD _p_id;
  Logger *_logger;
  EventManager *_event_manager;
  bool _watching;
  std::thread _monitor_thread;
  std::mutex _m_mutex;

  void _MonitoringProcess();
  void _Restart();
  bool _StopResumeProcess(DWORD p_id, bool resume);
 public:
  ProcessMonitor(std::wstring path = L"", std::wstring args = L"");
  ~ProcessMonitor(void);

/**
@brief Start program with arguments
@param path path to program
@param args command line arguments
*/
  void Start(std::wstring path = L"", std::wstring args = L"");

/**
@brief Restart windows process
*/
  void Restart();

/**
@return Information(state, id, handle) about process that monitoring
*/
  ProcessInfo GetProcessInfo();

/**
@brief Freeze all threads of process that monitoring
*/
  void Stop();

/**
@brief Retrieve process information and start monitoring it
@param p_id windows process id
*/ 
  void AttachTo(DWORD p_id);

  void SetLogger(Logger *logger);
 
  void SetEventManager(EventManager *event_manager);

 };
}

#endif
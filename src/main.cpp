#include "EventManager.h"
#include "Logger.h"
#include "LoggerFileOutput.h"
#include "ProcessMonitor.h"
#include <iostream>

using namespace std;
using namespace monitor;


/**
Please do not use complex code in the callback body as this may lead to delay processing of other events and make them not valid!
*/

// OnProcStart event callback
void OnProcStart(DWORD p_id) {
	wcout << L"Proces started! (id = " << p_id << L")" << endl; 
}

// OnProcRestart event callback
void OnProcRestart(DWORD p_id) {
	wcout << L"Process restarting... (id = " << p_id << L")" << endl; 
}

// OnProcAttached event callback
void OnProcAttached(DWORD p_id) {
	wcout << L"Successfully attached to process(id = " <<p_id << L")" << endl;
}

// OnProcExitSuccess event callback
void OnProcExitSuccess(DWORD p_id) {
	wcout << L"Process(id = " << p_id << L") is completed successfully" << endl;
}

// OnProcExitFailure event callback
void OnProcExitFailure(DWORD p_id) {
	wcout << L"Process(id = " << p_id << L") is not completed successfully" << endl;
}

// OnProcExitSuccess event callback
void OnProcStopped(DWORD p_id) {
	wcout << L"Process(id = " << p_id << L") stopped" << endl;
}

// OnProcExitFailure event callback
void OnProcResumed(DWORD p_id) {
	wcout << L"Process(id = " << p_id << L") resumed" << endl;
}

int main(int argc, wchar_t** argv) {
	ProcessMonitor monitor;

	// instance logger
	Logger *logger = new Logger();
	logger->SetOutputTo(new LoggerFileOutput(L"log.txt"));
	logger->SetFormat(L":day/:month/:year :hour::minute::second [Process Monitor]: :msg");
	
	monitor.SetLogger(logger);

	// instance event manager
	EventManager *event_manager = new EventManager();
	monitor.SetEventManager(event_manager);

	// set events that we want 
	event_manager->Bind(L"OnProcStart", OnProcStart);
	event_manager->Bind(L"OnProcRestart", OnProcRestart); 
	event_manager->Bind(L"OnProcAttached", OnProcAttached); 
	event_manager->Bind(L"OnProcExitSuccess", OnProcExitSuccess);
	event_manager->Bind(L"OnProcExitFailure", OnProcExitFailure);
	event_manager->Bind(L"OnProcStopped", OnProcStopped);
	event_manager->Bind(L"OnProcResumed", OnProcResumed);

	// start/attach process
	// monitor.AttachTo(3940);
	monitor.Start(L"C:\\windows\\system32\\calc.exe"/*, L"log.txt"*/);
	getchar();
	
	// restart this process
	monitor.Restart();
	getchar();

	// stop(freeze) this process...
	monitor.Stop();
	getchar();

	// and start it again
	monitor.Start();
	getchar();

	// get process information
	ProcessInfo info = monitor.GetProcessInfo();

	system("notepad log.txt");
	return 0;
}
#include "EventManager.h"

namespace monitor {

EventManager::EventManager() :  _work(true), _event_mutex() {
	this->_event_thread = std::thread(&EventManager::_EventManagerFunc, this);
}

EventManager::~EventManager() {
	this->_work = false;
	if(this->_event_thread.joinable()) this->_event_thread.join();
}

void EventManager::AddEvent(const std::wstring& e) {
	std::lock_guard<std::mutex> lock(this->_event_mutex);
	// create list of function, that waiting for event
	if(this->_events.count(e) == 0) this->_events[e] = std::vector<callback>();
}

void EventManager::Bind(const std::wstring& e, callback func) {
	std::lock_guard<std::mutex> lock(this->_event_mutex);
	// push callback to list
	if(this->_events.count(e) > 0) this->_events[e].push_back(func);
}

void EventManager::GenerateEvent(const std::wstring& e, DWORD p_id) {
	std::lock_guard<std::mutex> lock(this->_event_mutex);
	// add event to queue
	if(this->_events.count(e) > 0) this->_event_queue.push(std::make_pair(e, p_id));
}

void EventManager::_EventManagerFunc() {
	while(true) {	
		if(!this->_work) break;
		
		while(!this->_event_queue.empty()) {
			this->_event_mutex.lock();
			// get next event from queue
			std::pair<std::wstring, DWORD> event = this->_event_queue.front();
			this->_event_queue.pop();
			
			// call all functions that waiting for this event
			for(auto f: this->_events[event.first]) f(event.second);
			this->_event_mutex.unlock();
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}
}

}
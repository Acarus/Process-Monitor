#include "EventManager.h"

namespace monitor {

EventManager::EventManager() :  _work(true), _event_mutex(), _count_of_link(0) {
	this->_event_thread = std::thread(&EventManager::_EventManagerFunc, this);
}

EventManager::~EventManager() {
	this->_work = false;
	if(this->_event_thread.joinable()) this->_event_thread.join();
}

void EventManager::AddEvent(const std::wstring& e) {
	this->_event_mutex.lock();
	// create list of function, that waiting for event
	if(this->_events.count(e) == 0) this->_events[e] = std::vector<callback>();
	this->_event_mutex.unlock();
}

void EventManager::Bind(const std::wstring& e, callback func) {
	this->_event_mutex.lock();
	// push callback to list
	if(this->_events.count(e) > 0) this->_events[e].push_back(func);
	this->_event_mutex.unlock();
}

void EventManager::GenerateEvent(const std::wstring& e, DWORD p_id) {
	this->_event_mutex.lock();
	// add event to queue
	if(this->_events.count(e) > 0) this->_event_queue.push(std::make_pair(e, p_id));
	this->_event_mutex.unlock();
}

void EventManager::_EventManagerFunc() {
	while(1) {
		this->_event_mutex.lock();
		
		if(!this->_work) {
			this->_event_mutex.unlock();
			break;
		}

		while(!this->_event_queue.empty()) {
			// get next event from queue
			std::pair<std::wstring, DWORD> event = this->_event_queue.front();
			this->_event_queue.pop();
			
			// call all functions that waiting for this event
			for(auto f: this->_events[event.first]) f(event.second);
		}

		this->_event_mutex.unlock();
	}
}

void EventManager::AddRef() {
	this->_count_of_link++;
}

void EventManager::Release() {
	this->_count_of_link--;

	if(this->_count_of_link == 0) delete this; 
}

}
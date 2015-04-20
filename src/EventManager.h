#ifndef MONITOR_EVENTMANAGER_H
#define MONITOR_EVENTMANAGER_H

#include <functional>
#include <map>
#include <vector>
#include <queue>

#include <string>
#include <thread>
#include <mutex>
#include <algorithm>

namespace monitor {

typedef std::function<void(DWORD)> callback;

/**
@brief Class that allow to create and call event
*/
class EventManager {
  std::map<std::wstring, std::vector<callback>> _events;
  std::queue<std::pair<std::wstring, DWORD>> _event_queue;
  std::thread _event_thread;
  std::mutex _event_mutex;
  bool _work;
  int _count_of_link;

  void _EventManagerFunc();
 public:
   EventManager();
  ~EventManager();

  void GenerateEvent(const std::wstring& e, DWORD p_id);
/**
@brief Add event to the list of allowed
@param e event name
*/
  void AddEvent(const std::wstring& e);

/**
@brief Connects event and callback
@param e name of event
@param func callback function
*/
  void Bind(const std::wstring& e, callback func);

/**
@brief Increment count of links to object  
*/
  void AddRef();
 
/**
@brief Decrement count of links to object. If count equal zero object would be deleted  
*/
  void Release();
};

}

#endif
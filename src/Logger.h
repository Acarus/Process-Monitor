#ifndef MONITOR_LOGGER_H
#define MONITOR_LOGGER_H

#include <sstream>
#include <Windows.h>
#include "LoggerOutput.h"

namespace monitor {
/**
@brief Class that allow to make log
*/
class Logger {
  std::wstring _log_format;
  LoggerOutput *_output;
  int _count_of_link;

  std::wstring _ToString(int num);

 public:
  Logger();
  ~Logger();

/**
@brief Set format for logs. Allowed next keys: ":year", ":month", ":day", ":hour", ":minute", ":second", ":msg"(log message) 
@param log_format format of logs  
*/
  void SetFormat(const std::wstring& log_format);

/**
@brief Make log 
@param msg message string  
*/
  void Log(const std::wstring& msg);

/**
@brief Set output device 
@param output output device  
*/
  void SetOutputTo(LoggerOutput *output);

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
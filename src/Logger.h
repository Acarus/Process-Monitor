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
};

}

#endif
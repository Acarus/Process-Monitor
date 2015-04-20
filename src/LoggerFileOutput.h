#ifndef MONITOR_LOGGERFILEOUTPUT_H
#define MONITOR_LOGGERFILEOUTPUT_H

#include "LoggerOutput.h"
#include <fstream>

namespace monitor {

/**
@brief Class that allow to write logs to file
*/
class LoggerFileOutput : public LoggerOutput {
  std::wofstream _log_file;

 public:
  LoggerFileOutput(std::wstring file_name = L"");
  ~LoggerFileOutput();
/**
@brief Write log into file
@param str log message string
*/
  void Write(const std::wstring& str);

/**
@brief Set path to file, where we will write logs
@param log_file path to file
*/
  void SetFile(const std::wstring& file_name);
  
};

}

#endif
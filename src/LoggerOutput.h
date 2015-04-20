#ifndef MONITOR_LOGGEROUTPUT_H
#define MONITOR_LOGGEROUTPUT_H

#include <string>

namespace monitor {

/**
@brief Abstract output device class 
*/
class LoggerOutput {
 public:

 /**
 @brief Method allow to write log into some device
 @param str log message string
 */
  virtual void Write(const std::wstring& str) = 0;

  virtual ~LoggerOutput(){}
};

}

#endif
#include "LoggerFileOutput.h"

namespace monitor {

LoggerFileOutput::LoggerFileOutput(std::wstring file_name) { 
	// open file
	if(file_name.size() > 0) this->_log_file.open(file_name, std::ios_base::app);
}

LoggerFileOutput::~LoggerFileOutput() {
	if(this->_log_file.is_open()) this->_log_file.close();
}

void LoggerFileOutput::Write(const std::wstring& str) {
	if(this->_log_file.is_open()) this->_log_file << str << std::endl;
}

void LoggerFileOutput::SetFile(const std::wstring& file_name) {
	if(this->_log_file.is_open()) this->_log_file.close();
	this->_log_file.open(file_name, std::ios_base::app);
}

}
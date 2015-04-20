#include "LoggerFileOutput.h"

namespace monitor {

LoggerFileOutput::LoggerFileOutput(std::wstring log_file) : _log_file(log_file) { }

void LoggerFileOutput::Write(const std::wstring& str) {

	std::wofstream log_file;
	// open file
	log_file.open(this->_log_file, std::ios_base::app);

	if(log_file.is_open()) {
		log_file << str << std::endl;
		log_file.close();
	}
}

void LoggerFileOutput::SetFile(const std::wstring& log_file) {
	this->_log_file = log_file;
}

}
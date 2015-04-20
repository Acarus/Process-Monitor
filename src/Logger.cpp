#include "Logger.h"

namespace monitor {

Logger::Logger() : _log_format(L":day.:month.:year :hour::minute::second : :msg"), _output(nullptr), _count_of_link(0) { }

Logger::~Logger() {
	if(this->_output != nullptr)
		delete this->_output;
}


void Logger::SetFormat(const std::wstring& log_format) {
	this->_log_format = log_format;
}


void Logger::Log(const std::wstring& msg) {
	std::wstring formated_string = this->_log_format;

	// Get system time
	SYSTEMTIME date_time;
	ZeroMemory(&date_time, sizeof(SYSTEMTIME));
	GetSystemTime(&date_time);

	// Replace keys to values
	size_t pos = formated_string.find(L":day");
	if(pos != std::wstring::npos)
		formated_string.replace(pos, 4, this->_ToString(date_time.wDay));

	pos = formated_string.find(L":month");
	if(pos != std::wstring::npos)
		formated_string.replace(pos, 6, this->_ToString(date_time.wMonth));

	pos = formated_string.find(L":year");
	if(pos != std::wstring::npos)
		formated_string.replace(pos, 5, this->_ToString(date_time.wYear));

	pos = formated_string.find(L":msg");
	if(pos != std::wstring::npos)
		formated_string.replace(pos, 4, msg);

	pos = formated_string.find(L":minute");
	if(pos != std::wstring::npos)
		formated_string.replace(pos, 7, this->_ToString(date_time.wMinute));

	pos = formated_string.find(L":hour");
	if(pos != std::wstring::npos)
		formated_string.replace(pos, 5, this->_ToString(date_time.wHour));

	pos = formated_string.find(L":second");
	if(pos != std::wstring::npos)
		formated_string.replace(pos, 7, this->_ToString(date_time.wSecond));

	// Write log to output device
	if(this->_output != nullptr)
		this->_output->Write(formated_string);

}

std::wstring Logger::_ToString(int num) {
	std::wstringstream ss;
	ss << num;
	std::wstring result = static_cast<std::wstring>(ss.str());
	return (num > 9)?result:L"0"+result;
}

void Logger::SetOutputTo(LoggerOutput *output) {
	this->_output = output;
}

void Logger::AddRef() {
	this->_count_of_link++;
}

void Logger::Release() {
	this->_count_of_link--;

	if(this->_count_of_link == 0) delete this; 
}

}

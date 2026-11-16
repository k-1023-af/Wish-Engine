#include "Logger.h"


namespace Logger {
	//void Log(const std::string& message) {
	//	
	//	//OutputDebugStringA("Hello,DirectX!\n");
	//	//
	//	//Log(std::format("now:{:%Y/%m/%d %H:%M:%S}\n", localTime));
	//	//
	//	//std::filesystem::create_directory("logs");
	//	//
	//	//Log(logStream, std::format("start time (local time): {:%Y/%m/%d %H:%M:%S}", localTime));
	//	
	//}

	void Log(const std::string& message) {
		OutputDebugStringA(message.c_str());
	}

	void Log(const std::wstring& message) {
		Log(ConvertString(message));
	}

	void Log(std::ostream& os, const std::string& message) {
		os << message << std::endl;
		OutputDebugStringA(message.c_str());
	}
}


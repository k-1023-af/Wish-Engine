#include "StringUtility.h"
#include "Windows.h"

namespace StringUtility {
	std::wstring ConvertString(const std::string& str) {
		//return std::wstring(str.begin(), str.end());

		if (str.empty()) {
			return std::wstring();
		}
		
		int sizeNeeded = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
		if (sizeNeeded <= 0) {
			return std::wstring();
		}
		std::wstring result(sizeNeeded - 1, 0);
		MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &result[0], sizeNeeded);
		return result;
	}

	std::string ConvertString(const std::wstring& wstr) {
		///return std::string(wstr.begin(), wstr.end());


		if (wstr.empty()) {
			return std::string();
		}
		
		int sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);
		if (sizeNeeded <= 0) {
			return std::string();
		}
		std::string result(sizeNeeded - 1, 0);
		WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &result[0], sizeNeeded, nullptr, nullptr);
		return result;
	}
}
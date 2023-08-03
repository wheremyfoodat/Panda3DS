#pragma once
#include <vector>
#include <string>
#include <sstream>

namespace Helpers {
	static std::vector<std::string> split(const std::string& s, const char c) {
		std::istringstream tmp(s);
		std::vector<std::string> result(1);

		while (std::getline(tmp, *result.rbegin(), c)) {
			result.emplace_back();
		}

		// Remove temporary slot
		result.pop_back();
		return result;
	}
} // namespace Helpers

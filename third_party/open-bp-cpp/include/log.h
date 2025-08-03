#include <fstream>
#include <vector>
#include <chrono>

class RequestQueue {
public:
	unsigned int id = 1;
	std::string requestType;
};

class Logger {
public:
	Logger() {
		start = std::chrono::system_clock::now();
	}

	~Logger() {
		logFile.close();
	}

	void init(std::string filename);
	void logSentMessage(std::string rqType, unsigned int id);
	void logReceivedMessage(std::string repType, unsigned int id);

private:
	std::fstream logFile;
	std::vector<RequestQueue> rQueue;

	// Using time point and system_clock
	std::chrono::time_point<std::chrono::system_clock> start, end;
};
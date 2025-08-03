#include "../include/log.h"

void Logger::init(std::string filename) {
	logFile.open(filename, std::fstream::out);
}

void Logger::logSentMessage(std::string rqType, unsigned int id) {
	end = std::chrono::system_clock::now();
	std::chrono::duration<double> elapsed_seconds = end - start;
	RequestQueue tempQ;
	tempQ.id = id;
	tempQ.requestType = rqType;
	rQueue.push_back(tempQ);

	logFile << elapsed_seconds.count() << " s, Request type sent: " << rqType << ", ID: " << id << std::endl;
}

void Logger::logReceivedMessage(std::string repType, unsigned int id) {
	//end = std::chrono::system_clock::now();
}
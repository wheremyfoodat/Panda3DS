#include "services/service_manager.hpp"

ServiceManager::ServiceManager(std::array<u32, 16>& regs, Memory& mem) : regs(regs), mem(mem) {}

void ServiceManager::reset() {

}

// Match IPC messages to a "srv:" command based on their header
namespace Commands {
	enum : u32 {
		RegisterClient = 0x00010002,
		EnableNotification = 0x00020000,
		RegisterService = 0x00030100,
		UnregisterService = 0x000400C0,
		GetServiceHandle = 0x00050100,
		RegisterPort = 0x000600C2,
		UnregisterPort = 0x000700C0,
		GetPort = 0x00080100,
		Subscribe = 0x00090040,
		Unsubscribe = 0x000A0040,
		ReceiveNotification = 0x000B0000,
		PublishToSubscriber = 0x000C0080,
		PublishAndGetSubscriber = 0x000D0040,
		IsServiceRegistered = 0x000E00C0
	};
}

// Handle an IPC message issued using the SendSyncRequest SVC
// The parameters are stored in thread-local storage in this format: https://www.3dbrew.org/wiki/IPC#Message_Structure
// TLSPointer: The base pointer for this thread's thread-local storage
void ServiceManager::handleSyncRequest(u32 TLSPointer) {
	// The message is stored at TLS+0x80 in this format: https://www.3dbrew.org/wiki/IPC#Message_Structure
	const u32 messagePointer = TLSPointer + 0x80;
	const u32 header = mem.read32(messagePointer);

	switch (header) {
		case Commands::RegisterClient: registerClient(messagePointer); break;
		case Commands::GetServiceHandle: getServiceHandle(messagePointer); break;
		default: Helpers::panic("Unknown \"srv:\" command: %08X", header);
	}
}

void ServiceManager::registerClient(u32 messagePointer) {
	printf("srv: registerClient (Stubbed)\n");
}

void ServiceManager::getServiceHandle(u32 messagePointer) {
	printf("srv: getServiceHandle\n");

	std::string myBalls;
	myBalls.resize(8);

	for (int i = 0; i < 8; i++) {
		myBalls[i] = mem.read8(messagePointer + 4 + i);
	}

	std::cout << "Requested handle for service " << myBalls << "\n";
}
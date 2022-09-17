#include "kernel.hpp"
#include <cstring>

Handle Kernel::makePort(const char* name) {
	Handle ret = makeObject(KernelObjectType::Port);
	portHandles.push_back(ret); // Push the port handle to our cache of port handles

	objects[ret].data = new PortData();
	const auto data = static_cast<PortData*>(objects[ret].data);

	// If the name is empty (ie the first char is the null terminator) then the port is private
	data->isPublic = name[0] != '\0';
	std::strncpy(data->name, name, PortData::maxNameLen);

	// printf("Created %s port \"%s\" with handle %d\n", data->isPublic ? "public" : "private", data->name, ret);
	return ret;
}

// Get the handle of a port based on its name
// If there's no such port, return nullopt
std::optional<Handle> Kernel::getPortHandle(const char* name) {
	for (auto handle : portHandles) {
		const auto data = static_cast<PortData*>(objects[handle].data);
		if (std::strncmp(name, data->name, PortData::maxNameLen) == 0) {
			return handle;
		}
	}

	return std::nullopt;
}

// Result ConnectToPort(Handle* out, const char* portName)
void Kernel::connectToPort() {
	const u32 handlePointer = regs[0];
	const char* port = static_cast<const char*>(mem.getReadPointer(regs[1]));
	printf("ConnectToPort(handle pointer = %08X, port = \"%s\")\n", handlePointer, port);

	// TODO: This is unsafe
	if (std::strlen(port) > PortData::maxNameLen) {
		Helpers::panic("ConnectToPort: Port name too long\n");
		regs[0] = SVCResult::PortNameTooLong;
		return;
	}

	const auto portHandle = getPortHandle(port);
	if (!portHandle.has_value()) {
		Helpers::panic("ConnectToPort: Port doesn't exist\n");
		regs[0] = SVCResult::ObjectNotFound;
		return;
	}
	
	// TODO: Actually create session
	Handle sessionHandle = makeObject(KernelObjectType::Session);

	regs[0] = SVCResult::Success;
	regs[1] = sessionHandle;
}
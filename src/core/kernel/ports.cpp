#include "kernel.hpp"
#include <cstring>

Handle Kernel::makePort(const char* name) {
	Handle ret = makeObject(KernelObjectType::Port);
	portHandles.push_back(ret); // Push the port handle to our cache of port handles
	objects[ret].data = new Port(name);

	return ret;
}

Handle Kernel::makeSession(Handle portHandle) {
	const auto port = getObject(portHandle, KernelObjectType::Port);
	if (port == nullptr) [[unlikely]] {
		Helpers::panic("Trying to make session for non-existent port");
	}

	// Allocate data for session
	const Handle ret = makeObject(KernelObjectType::Session);
	objects[ret].data = new Session(portHandle);
	return ret;
}

// Get the handle of a port based on its name
// If there's no such port, return nullopt
std::optional<Handle> Kernel::getPortHandle(const char* name) {
	for (auto handle : portHandles) {
		const auto data = objects[handle].getData<Port>();
		if (std::strncmp(name, data->name, Port::maxNameLen) == 0) {
			return handle;
		}
	}

	return std::nullopt;
}

// Result ConnectToPort(Handle* out, const char* portName)
void Kernel::connectToPort() {
	const u32 handlePointer = regs[0];
	// Read up to max + 1 characters to see if the name is too long
	std::string port = mem.readString(regs[1], Port::maxNameLen + 1);
	logSVC("ConnectToPort(handle pointer = %X, port = \"%s\")\n", handlePointer, port.c_str());

	if (port.size() > Port::maxNameLen) {
		Helpers::panic("ConnectToPort: Port name too long\n");
		regs[0] = SVCResult::PortNameTooLong;
		return;
	}

	// Try getting a handle to the port
	std::optional<Handle> optionalHandle = getPortHandle(port.c_str());
	if (!optionalHandle.has_value()) [[unlikely]] {
		Helpers::panic("ConnectToPort: Port doesn't exist\n");
		regs[0] = SVCResult::ObjectNotFound;
		return;
	}

	Handle portHandle = optionalHandle.value();

	const auto portData = objects[portHandle].getData<Port>();
	if (!portData->isPublic) {
		Helpers::panic("ConnectToPort: Attempted to connect to private port");
	}

	// TODO: Actually create session
	Handle sessionHandle = makeSession(portHandle);

	regs[0] = SVCResult::Success;
	regs[1] = sessionHandle;
}

// Result SendSyncRequest(Handle session)
// Send an IPC message to a port (typically "srv:") or a service
void Kernel::sendSyncRequest() {
	const auto handle = regs[0];
	u32 messagePointer = getTLSPointer() + 0x80; // The message is stored starting at TLS+0x80

	logSVC("SendSyncRequest(session handle = %X)\n", handle);

	// The sync request is being sent at a service rather than whatever port, so have the service manager intercept it
	if (KernelHandles::isServiceHandle(handle)) {
		serviceManager.sendCommandToService(messagePointer, handle);
		regs[0] = SVCResult::Success;
		return;
	}

	// Check if our sync request is targetting a file instead of a service
	bool isFileOperation = getObject(handle, KernelObjectType::File) != nullptr;
	if (isFileOperation) {
		handleFileOperation(messagePointer, handle);
		regs[0] = SVCResult::Success;
		return;
	}

	// If we're actually communicating with a port
	const auto session = getObject(handle, KernelObjectType::Session);
	if (session == nullptr) [[unlikely]] {
		Helpers::panic("SendSyncRequest: Invalid handle");
		regs[0] = SVCResult::BadHandle;
		return;
	}

	const auto sessionData = static_cast<Session*>(session->data);
	const Handle portHandle = sessionData->portHandle;

	if (portHandle == srvHandle) { // Special-case SendSyncRequest targetting the "srv: port"
		serviceManager.handleSyncRequest(messagePointer);
	} else if (portHandle == errorPortHandle) { // Special-case "err:f" for juicy logs too
		handleErrorSyncRequest(messagePointer);
	} else {
		const auto portData = objects[portHandle].getData<Port>();
		Helpers::panic("SendSyncRequest targetting port %s\n", portData->name);
	}

	regs[0] = SVCResult::Success;
}
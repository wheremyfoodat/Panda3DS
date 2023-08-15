#pragma once
#include <optional>
#include "helpers.hpp"
#include "kernel_types.hpp"
#include "logger.hpp"
#include "memory.hpp"

// Circular dependencies go br
class Kernel;

class Y2RService {
	Handle handle = KernelHandles::Y2R;
	Memory& mem;
	Kernel& kernel;
	MAKE_LOG_FUNCTION(log, y2rLogger)

	std::optional<Handle> transferEndEvent;
	bool transferEndInterruptEnabled;

	enum class BusyStatus : u32 {
		NotBusy = 0,
		Busy = 1
	};

	enum class InputFormat : u32 {
		YUV422_Individual8 = 0,
		YUV420_Individual8 = 1,
		YUV422_Individual16 = 2,
		YUV420_Individual16 = 3,
		YUV422_Batch = 4,
	};

	enum class OutputFormat : u32 {
		RGB32 = 0,
		RGB24 = 1,
		RGB15 = 2,
		RGB565 = 3
	};

	// Clockwise rotation
	enum class Rotation : u32 {
		None = 0,
		Rotate90 = 1,
		Rotate180 = 2,
		Rotate270 = 3
	};

	enum class BlockAlignment : u32 {
		Line = 0, // Output buffer's pixels are arranged linearly. Used when outputting to the framebuffer.
		Block8x8 = 1, // Output buffer's pixels are morton swizzled. Used when outputting to a GPU texture.
	};

	InputFormat inputFmt;
	OutputFormat outputFmt;
	Rotation rotation;
	BlockAlignment alignment;

	bool spacialDithering;
	bool temporalDithering;
	u16 alpha;
	u16 inputLineWidth;
	u16 inputLines;

	// Service commands
	void driverInitialize(u32 messagePointer);
	void driverFinalize(u32 messagePointer);
	void isBusyConversion(u32 messagePointer);
	void pingProcess(u32 messagePointer);
	void setTransferEndInterrupt(u32 messagePointer);
	void getTransferEndEvent(u32 messagePointer);

	void setAlpha(u32 messagePointer);
	void setBlockAlignment(u32 messagePointer);
	void setInputFormat(u32 messagePointer);
	void setInputLineWidth(u32 messagePointer);
	void setInputLines(u32 messagePointer);
	void setOutputFormat(u32 messagePointer);
	void setPackageParameter(u32 messagePointer);
	void setReceiving(u32 messagePointer);
	void setRotation(u32 messagePointer);
	void setSendingY(u32 messagePointer);
	void setSendingU(u32 messagePointer);
	void setSendingV(u32 messagePointer);
	void setSpacialDithering(u32 messagePointer);
	void setStandardCoeff(u32 messagePointer);
	void setTemporalDithering(u32 messagePointer);

	void startConversion(u32 messagePointer);
	void stopConversion(u32 messagePointer);

public:
	Y2RService(Memory& mem, Kernel& kernel) : mem(mem), kernel(kernel) {}
	void reset();
	void handleSyncRequest(u32 messagePointer);
};
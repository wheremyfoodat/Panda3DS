#include "services/y2r.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <vector>

#include "ipc.hpp"
#include "kernel.hpp"

namespace Y2RCommands {
	enum : u32 {
		SetInputFormat = 0x00010040,
		GetInputFormat = 0x00020000,
		SetOutputFormat = 0x00030040,
		GetOutputFormat = 0x00040000,
		SetRotation = 0x00050040,
		SetBlockAlignment = 0x00070040,
		GetBlockAlignment = 0x00080000,
		SetSpacialDithering = 0x00090040,
		SetTemporalDithering = 0x000B0040,
		SetTransferEndInterrupt = 0x000D0040,
		GetTransferEndEvent = 0x000F0000,
		SetSendingY = 0x00100102,
		SetSendingU = 0x00110102,
		SetSendingV = 0x00120102,
		SetSendingYUV = 0x00130102,
		IsFinishedSendingYUV = 0x00140000,
		IsFinishedSendingY = 0x00150000,
		IsFinishedSendingU = 0x00160000,
		IsFinishedSendingV = 0x00170000,
		SetReceiving = 0x00180102,
		IsFinishedReceiving = 0x00190000,
		SetInputLineWidth = 0x001A0040,
		GetInputLineWidth = 0x001B0000,
		SetInputLines = 0x001C0040,
		GetInputLines = 0x001D0000,
		SetCoefficientParams = 0x001E0100,
		GetCoefficientParams = 0x001F0000,
		SetStandardCoeff = 0x00200040,
		GetStandardCoefficientParams = 0x00210040,
		SetAlpha = 0x00220040,
		StartConversion = 0x00260000,
		StopConversion = 0x00270000,
		IsBusyConversion = 0x00280000,
		SetPackageParameter = 0x002901C0,
		PingProcess = 0x002A0000,
		DriverInitialize = 0x002B0000,
		DriverFinalize = 0x002C0000,
	};
}

// The Y2R hardware works through the input image in strips that are 8 lines tall. A strip is
// converted into RGB32 tiles of 8x8 pixels, which are then rotated, remapped and packed into the
// output format. Neither the strip height nor the tile layout is configurable.
namespace {
	using InputFormat = Y2RService::InputFormat;
	using OutputFormat = Y2RService::OutputFormat;
	using Rotation = Y2RService::Rotation;
	using ConversionBuffer = Y2RService::ConversionBuffer;
	using CoefficientSet = Y2RService::CoefficientSet;

	constexpr u32 stripHeight = 8;
	constexpr u32 tileSize = stripHeight * stripHeight;  // Pixels per 8x8 tile
	using ImageTile = std::array<u32, tileSize>;

	// Maps a tile's pixels to where they are stored in the output buffer. Line alignment keeps the
	// pixels in row-major order, 8x8 block alignment uses the morton swizzling the PICA expects.
	constexpr std::array<u8, tileSize> linearLut = {
		// clang-format off
		 0,  1,  2,  3,  4,  5,  6,  7,
		 8,  9, 10, 11, 12, 13, 14, 15,
		16, 17, 18, 19, 20, 21, 22, 23,
		24, 25, 26, 27, 28, 29, 30, 31,
		32, 33, 34, 35, 36, 37, 38, 39,
		40, 41, 42, 43, 44, 45, 46, 47,
		48, 49, 50, 51, 52, 53, 54, 55,
		56, 57, 58, 59, 60, 61, 62, 63,
		// clang-format on
	};

	constexpr std::array<u8, tileSize> mortonLut = {
		// clang-format off
		 0,  1,  4,  5, 16, 17, 20, 21,
		 2,  3,  6,  7, 18, 19, 22, 23,
		 8,  9, 12, 13, 24, 25, 28, 29,
		10, 11, 14, 15, 26, 27, 30, 31,
		32, 33, 36, 37, 48, 49, 52, 53,
		34, 35, 38, 39, 50, 51, 54, 55,
		40, 41, 44, 45, 56, 57, 60, 61,
		42, 43, 46, 47, 58, 59, 62, 63,
		// clang-format on
	};

	// The rotations below rotate each tile on its own. The hardware processes strips independently,
	// so rotations of 90 and 270 degrees turn every tile of a strip into a separate 8 x height image.
	void rotateTile0(const ImageTile& input, ImageTile& output, u32 height, const std::array<u8, tileSize>& map) {
		for (u32 i = 0; i < height * 8; i++) {
			output[map[i]] = input[i];
		}
	}

	void rotateTile90(const ImageTile& input, ImageTile& output, u32 height, const std::array<u8, tileSize>& map) {
		u32 outIndex = 0;
		for (u32 x = 0; x < 8; x++) {
			for (u32 y = height - 1; y != u32(-1); y--) {
				output[map[outIndex++]] = input[y * 8 + x];
			}
		}
	}

	void rotateTile180(const ImageTile& input, ImageTile& output, u32 height, const std::array<u8, tileSize>& map) {
		u32 outIndex = 0;
		for (u32 i = height * 8 - 1; i != u32(-1); i--) {
			output[map[outIndex++]] = input[i];
		}
	}

	void rotateTile270(const ImageTile& input, ImageTile& output, u32 height, const std::array<u8, tileSize>& map) {
		u32 outIndex = 0;
		for (u32 x = 8 - 1; x != u32(-1); x--) {
			for (u32 y = 0; y < height; y++) {
				output[map[outIndex++]] = input[y * 8 + x];
			}
		}
	}

	// Turns one strip of decoded YUV samples into 8x8 tiles of RGB32 pixels. The arithmetic is
	// bit-exact with the hardware, as far as it has been tested.
	template <InputFormat format>
	void convertStrip(
		const u8* inputY, const u8* inputU, const u8* inputV, ImageTile* output, u32 width, u32 height, const CoefficientSet& coefficients
	) {
		for (u32 y = 0; y < height; y++) {
			for (u32 x = 0; x < width; x++) {
				s32 Y, U, V;

				if constexpr (format == InputFormat::YUV422_Individual8 || format == InputFormat::YUV422_Individual16) {
					Y = inputY[y * width + x];
					U = inputU[(y * width + x) / 2];
					V = inputV[(y * width + x) / 2];
				} else if constexpr (format == InputFormat::YUV420_Individual8 || format == InputFormat::YUV420_Individual16) {
					Y = inputY[y * width + x];
					U = inputU[((y / 2) * width + x) / 2];
					V = inputV[((y / 2) * width + x) / 2];
				} else {
					// Interleaved YUYV pairs
					Y = inputY[(y * width + x) * 2];
					U = inputY[(y * width + (x / 2) * 2) * 2 + 1];
					V = inputY[(y * width + (x / 2) * 2) * 2 + 3];
				}

				const s32 cY = coefficients[0] * Y;
				s32 r = cY + coefficients[1] * V;
				s32 g = cY - coefficients[2] * V - coefficients[3] * U;
				s32 b = cY + coefficients[4] * U;

				constexpr s32 roundingOffset = 0x18;
				r = (r >> 3) + coefficients[5] + roundingOffset;
				g = (g >> 3) + coefficients[6] + roundingOffset;
				b = (b >> 3) + coefficients[7] + roundingOffset;

				const u32 red = std::clamp(r >> 5, 0, 0xFF);
				const u32 green = std::clamp(g >> 5, 0, 0xFF);
				const u32 blue = std::clamp(b >> 5, 0, 0xFF);
				output[x / 8][y * 8 + (x % 8)] = (red << 24) | (green << 16) | (blue << 8);
			}
		}
	}

	// Simulates an incoming CDMA transfer: the data of one buffer is moved in units of transferUnit
	// bytes, with gap bytes skipped in between. N is the number of bytes per sample, which
	// automatically reduces 16-bit formats to the 8-bit values the conversion works on.
	template <u32 N>
	void receiveData(Memory& mem, ConversionBuffer& buffer, u8* output, u32 amountOfData) {
		const u32 outputUnit = buffer.transferUnit / N;
		if (outputUnit == 0) {
			Helpers::warn("Y2R: Source buffer was not set up, skipping conversion\n");
			return;
		}

		while (amountOfData > 0) {
			const u8* input = static_cast<const u8*>(mem.getReadPointer(buffer.address));
			if (input == nullptr) {
				Helpers::warn("Y2R: Source buffer at %08X cannot be read\n", buffer.address);
				return;
			}

			const u32 count = std::min(outputUnit, amountOfData);
			for (u32 i = 0; i < count; i++) {
				output[i] = input[i * N];
			}

			output += outputUnit;
			buffer.address += buffer.transferUnit + buffer.gap;
			buffer.imageSize -= buffer.transferUnit;
			amountOfData -= count;
		}
	}

	// Packs the intermediate RGB32 pixels into the output format while simulating an outgoing CDMA
	// transfer, which again moves transferUnit bytes at a time and skips gap bytes in between.
	void sendData(Memory& mem, OutputFormat format, const u32* input, ConversionBuffer& buffer, s32 amountOfData, u8 alpha) {
		if (buffer.transferUnit == 0) {
			Helpers::warn("Y2R: Destination buffer was not set up, skipping conversion\n");
			return;
		}

		while (amountOfData > 0) {
			const u32 unitStart = buffer.address;
			u8* output = static_cast<u8*>(mem.getWritePointer(unitStart));
			if (output == nullptr) {
				Helpers::warn("Y2R: Destination buffer at %08X cannot be written\n", unitStart);
				return;
			}

			const u8* unitBegin = output;
			const u8* unitEnd = output + buffer.transferUnit;
			while (output < unitEnd && amountOfData > 0) {
				const u32 color = *input++;
				const u8 r = (color >> 24) & 0xFF;
				const u8 g = (color >> 16) & 0xFF;
				const u8 b = (color >> 8) & 0xFF;

				// The PICA formats used by the Y2R output store their channels in BGR order in memory
				switch (format) {
					case OutputFormat::RGB32:  // RGBA8, stored as A, B, G, R
						output[0] = alpha;
						output[1] = b;
						output[2] = g;
						output[3] = r;
						output += 4;
						break;

					case OutputFormat::RGB24:  // Stored as B, G, R
						output[0] = b;
						output[1] = g;
						output[2] = r;
						output += 3;
						break;

					case OutputFormat::RGB15: {  // RGBA5551, stored little-endian
						const u16 value = ((r & 0xF8) << 8) | ((g & 0xF8) << 3) | ((b & 0xF8) >> 2) | (alpha >> 7);
						output[0] = value & 0xFF;
						output[1] = value >> 8;
						output += 2;
						break;
					}

					case OutputFormat::RGB565: {  // Little-endian as well
						const u16 value = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | ((b & 0xF8) >> 3);
						output[0] = value & 0xFF;
						output[1] = value >> 8;
						output += 2;
						break;
					}
				}

				amountOfData -= 1;
			}

			// The pixels went into guest memory through a raw pointer, which skips the write functions that
			// tell the renderer about changes, so report the range here or textures decoded from the output
			// buffer keep showing the first frame
			mem.notifyGuestWriteRange(unitStart, u32(output - unitBegin));

			output += buffer.gap;
			buffer.address += buffer.transferUnit + buffer.gap;
			buffer.imageSize -= buffer.transferUnit;
		}
	}
}  // namespace

void Y2RService::reset() {
	transferEndInterruptEnabled = false;
	transferEndEvent = std::nullopt;

	alignment = BlockAlignment::Line;
	inputFmt = InputFormat::YUV422_Individual8;
	outputFmt = OutputFormat::RGB32;
	rotation = Rotation::None;

	spacialDithering = false;
	temporalDithering = false;

	srcY = ConversionBuffer{};
	srcU = ConversionBuffer{};
	srcV = ConversionBuffer{};
	srcYUYV = ConversionBuffer{};
	dst = ConversionBuffer{};

	alpha = 0xFF;
	inputLines = 69;
	inputLineWidth = 420;

	conversionCoefficients.fill(0);
	isBusy = false;
}

// Performs the YUV to RGB conversion the hardware would do. Only the settings combinations the
// games in the compatibility list use are fully handled; anything unexpected is warned about and
// skipped rather than producing garbage.
void Y2RService::convert() {
	const u32 width = inputLineWidth;
	const u32 height = inputLines;

	if (width % 8 != 0) {
		Helpers::warn("Y2R: Input line width %d is not a multiple of 8, skipping conversion\n", width);
		return;
	}

	if (alignment == BlockAlignment::Block8x8 && height % 8 != 0) {
		Helpers::warn("Y2R: 8x8 block alignment requires a line count of a multiple of 8, skipping conversion\n");
		return;
	}

	// The conversion walks through the buffers as it moves data, but the service keeps them as they
	// were configured, so each conversion starts at the beginning of the image again.
	ConversionBuffer srcYBuf = srcY, srcUBuf = srcU, srcVBuf = srcV, srcYUYVBuf = srcYUYV, dstBuf = dst;

	const u32 tileCount = width / 8;
	// Holds the incoming YUV data and the outgoing RGB32 data of one strip
	std::vector<u8> dataBuffer(size_t(width) * stripHeight * 4);
	std::vector<ImageTile> tiles(tileCount);
	ImageTile tmpTile;
	const std::array<u8, tileSize>& tileMap = (alignment == BlockAlignment::Block8x8) ? mortonLut : linearLut;

	for (u32 line = 0; line < height; line += stripHeight) {
		const u32 rowHeight = std::min(stripHeight, height - line);
		const u32 rowDataSize = rowHeight * width;  // Pixels in this strip

		u8* inputY = dataBuffer.data();
		u8* inputU = inputY + stripHeight * width;
		u8* inputV = inputU + stripHeight * width / 2;

		switch (inputFmt) {
			case InputFormat::YUV422_Individual8:
				receiveData<1>(mem, srcYBuf, inputY, rowDataSize);
				receiveData<1>(mem, srcUBuf, inputU, rowDataSize / 2);
				receiveData<1>(mem, srcVBuf, inputV, rowDataSize / 2);
				convertStrip<InputFormat::YUV422_Individual8>(
					inputY, inputU, inputV, tiles.data(), width, rowHeight, conversionCoefficients
				);
				break;

			case InputFormat::YUV420_Individual8:
				receiveData<1>(mem, srcYBuf, inputY, rowDataSize);
				receiveData<1>(mem, srcUBuf, inputU, rowDataSize / 4);
				receiveData<1>(mem, srcVBuf, inputV, rowDataSize / 4);
				convertStrip<InputFormat::YUV420_Individual8>(
					inputY, inputU, inputV, tiles.data(), width, rowHeight, conversionCoefficients
				);
				break;

			case InputFormat::YUV422_Individual16:
				receiveData<2>(mem, srcYBuf, inputY, rowDataSize);
				receiveData<2>(mem, srcUBuf, inputU, rowDataSize / 2);
				receiveData<2>(mem, srcVBuf, inputV, rowDataSize / 2);
				convertStrip<InputFormat::YUV422_Individual16>(
					inputY, inputU, inputV, tiles.data(), width, rowHeight, conversionCoefficients
				);
				break;

			case InputFormat::YUV420_Individual16:
				receiveData<2>(mem, srcYBuf, inputY, rowDataSize);
				receiveData<2>(mem, srcUBuf, inputU, rowDataSize / 4);
				receiveData<2>(mem, srcVBuf, inputV, rowDataSize / 4);
				convertStrip<InputFormat::YUV420_Individual16>(
					inputY, inputU, inputV, tiles.data(), width, rowHeight, conversionCoefficients
				);
				break;

			case InputFormat::YUV422_Interleaved:
				receiveData<1>(mem, srcYUYVBuf, inputY, rowDataSize * 2);
				convertStrip<InputFormat::YUV422_Interleaved>(
					inputY, nullptr, nullptr, tiles.data(), width, rowHeight, conversionCoefficients
				);
				break;
		}

		u32* outputBuffer = reinterpret_cast<u32*>(dataBuffer.data());

		for (u32 i = 0; i < tileCount; i++) {
			u32 stripWidth = 0;   // Width of the image the tile belongs to, in pixels
			u32 outputStride = 0;

			switch (rotation) {
				case Rotation::None:
					rotateTile0(tiles[i], tmpTile, rowHeight, tileMap);
					stripWidth = width;
					outputStride = 8;
					break;

				case Rotation::Rotate90:
					rotateTile90(tiles[i], tmpTile, rowHeight, tileMap);
					stripWidth = 8;
					outputStride = 8 * rowHeight;
					break;

				case Rotation::Rotate180:
					// The tiles are rotated individually, so their order in the strip has to be
					// reversed as well for 180 and 270 degrees
					rotateTile180(tiles[tileCount - i - 1], tmpTile, rowHeight, tileMap);
					stripWidth = width;
					outputStride = 8;
					break;

				case Rotation::Rotate270:
					rotateTile270(tiles[tileCount - i - 1], tmpTile, rowHeight, tileMap);
					stripWidth = 8;
					outputStride = 8 * rowHeight;
					break;
			}

			// Copy the tile's pixels out, either as part of a linearly stored image or as a single
			// swizzled block
			if (alignment == BlockAlignment::Block8x8) {
				for (u32 y = 0; y < 8; y++) {
					for (u32 x = 0; x < 8; x++) {
						outputBuffer[y * 8 + x] = tmpTile[y * 8 + x];
					}
				}
				outputBuffer += tileSize;
			} else {
				for (u32 y = 0; y < rowHeight; y++) {
					for (u32 x = 0; x < 8; x++) {
						outputBuffer[y * stripWidth + x] = tmpTile[y * 8 + x];
					}
				}
				outputBuffer += outputStride;
			}
		}

		sendData(mem, outputFmt, reinterpret_cast<const u32*>(dataBuffer.data()), dstBuf, static_cast<s32>(rowDataSize), alpha);
	}
}

void Y2RService::handleSyncRequest(u32 messagePointer) {
	const u32 command = mem.read32(messagePointer);
	switch (command) {
		case Y2RCommands::DriverInitialize: driverInitialize(messagePointer); break;
		case Y2RCommands::DriverFinalize: driverFinalize(messagePointer); break;
		case Y2RCommands::GetBlockAlignment: getBlockAlignment(messagePointer); break;
		case Y2RCommands::GetInputFormat: getInputFormat(messagePointer); break;
		case Y2RCommands::GetInputLines: getInputLines(messagePointer); break;
		case Y2RCommands::GetInputLineWidth: getInputLineWidth(messagePointer); break;
		case Y2RCommands::GetOutputFormat: getOutputFormat(messagePointer); break;
		case Y2RCommands::GetTransferEndEvent: getTransferEndEvent(messagePointer); break;
		case Y2RCommands::GetStandardCoefficientParams: getStandardCoefficientParams(messagePointer); break;
		case Y2RCommands::IsBusyConversion: isBusyConversion(messagePointer); break;
		case Y2RCommands::IsFinishedReceiving: isFinishedReceiving(messagePointer); break;
		case Y2RCommands::IsFinishedSendingY: isFinishedSendingY(messagePointer); break;
		case Y2RCommands::IsFinishedSendingU: isFinishedSendingU(messagePointer); break;
		case Y2RCommands::IsFinishedSendingV: isFinishedSendingV(messagePointer); break;
		case Y2RCommands::IsFinishedSendingYUV: isFinishedSendingYUV(messagePointer); break;
		case Y2RCommands::PingProcess: pingProcess(messagePointer); break;
		case Y2RCommands::SetAlpha: setAlpha(messagePointer); break;
		case Y2RCommands::SetBlockAlignment: setBlockAlignment(messagePointer); break;
		case Y2RCommands::SetInputFormat: setInputFormat(messagePointer); break;
		case Y2RCommands::SetInputLineWidth: setInputLineWidth(messagePointer); break;
		case Y2RCommands::SetInputLines: setInputLines(messagePointer); break;
		case Y2RCommands::SetOutputFormat: setOutputFormat(messagePointer); break;
		case Y2RCommands::SetPackageParameter: setPackageParameter(messagePointer); break;
		case Y2RCommands::SetReceiving: setReceiving(messagePointer); break;
		case Y2RCommands::SetRotation: setRotation(messagePointer); break;
		case Y2RCommands::SetSendingY: setSendingY(messagePointer); break;
		case Y2RCommands::SetSendingU: setSendingU(messagePointer); break;
		case Y2RCommands::SetSendingV: setSendingV(messagePointer); break;
		case Y2RCommands::SetSendingYUV: setSendingYUV(messagePointer); break;
		case Y2RCommands::SetSpacialDithering: setSpacialDithering(messagePointer); break;
		case Y2RCommands::SetStandardCoeff: setStandardCoeff(messagePointer); break;
		case Y2RCommands::SetTemporalDithering: setTemporalDithering(messagePointer); break;
		case Y2RCommands::SetTransferEndInterrupt: setTransferEndInterrupt(messagePointer); break;
		case Y2RCommands::StartConversion: [[likely]] startConversion(messagePointer); break;
		case Y2RCommands::StopConversion: stopConversion(messagePointer); break;

		// Intentionally break ordering a bit for less-used Y2R functions
		case Y2RCommands::SetCoefficientParams: setCoefficientParams(messagePointer); break;
		case Y2RCommands::GetCoefficientParams: getCoefficientParams(messagePointer); break;
		default: Helpers::panic("Y2R service requested. Command: %08X\n", command);
	}
}

void Y2RService::pingProcess(u32 messagePointer) {
	log("Y2R::PingProcess\n");
	mem.write32(messagePointer, IPC::responseHeader(0x2A, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, 0);  // Connected number
}

void Y2RService::driverInitialize(u32 messagePointer) {
	log("Y2R::DriverInitialize\n");
	mem.write32(messagePointer, IPC::responseHeader(0x2B, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);

	conversionCoefficients.fill(0);
}

void Y2RService::driverFinalize(u32 messagePointer) {
	log("Y2R::DriverInitialize\n");
	mem.write32(messagePointer, IPC::responseHeader(0x2C, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void Y2RService::getTransferEndEvent(u32 messagePointer) {
	log("Y2R::GetTransferEndEvent\n");
	if (!transferEndEvent.has_value()) {
		transferEndEvent = kernel.makeEvent(ResetType::OneShot);
	}

	mem.write32(messagePointer, IPC::responseHeader(0xF, 1, 2));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 12, transferEndEvent.value());
}

void Y2RService::setTransferEndInterrupt(u32 messagePointer) {
	const bool enable = mem.read32(messagePointer + 4) != 0;
	log("Y2R::SetTransferEndInterrupt (enabled: %s)\n", enable ? "yes" : "no");

	mem.write32(messagePointer, IPC::responseHeader(0xD, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
	transferEndInterruptEnabled = enable;
}

// We don't need to actually do anything for this.
// Cause it assumes that
// a) Y2R conversion works
// b) It isn't instant
void Y2RService::stopConversion(u32 messagePointer) {
	log("Y2R::StopConversion\n");

	if (isBusy) {
		isBusy = false;
		kernel.getScheduler().removeEvent(Scheduler::EventType::SignalY2R);
	}

	mem.write32(messagePointer, IPC::responseHeader(0x27, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

// See above. Our Y2R conversion is instant because there's really no point trying to delay it
// This is a modern enough console for us to screw timings
void Y2RService::isBusyConversion(u32 messagePointer) {
	log("Y2R::IsBusyConversion\n");

	mem.write32(messagePointer, IPC::responseHeader(0x28, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, static_cast<u32>(isBusy ? BusyStatus::Busy : BusyStatus::NotBusy));
}

void Y2RService::setBlockAlignment(u32 messagePointer) {
	const u32 newAlignment = mem.read32(messagePointer + 4);
	log("Y2R::SetBlockAlignment (format = %d)\n", newAlignment);

	if (newAlignment > 1) {
		Helpers::warn("Warning: Invalid block alignment for Y2R conversion\n");
	} else {
		alignment = static_cast<BlockAlignment>(newAlignment);
	}

	mem.write32(messagePointer, IPC::responseHeader(0x7, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void Y2RService::getBlockAlignment(u32 messagePointer) {
	log("Y2R::GetBlockAlignment\n");

	mem.write32(messagePointer, IPC::responseHeader(0x8, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, static_cast<u32>(alignment));
}

void Y2RService::setInputFormat(u32 messagePointer) {
	const u32 format = mem.read32(messagePointer + 4);
	log("Y2R::SetInputFormat (format = %d)\n", format);

	if (format > 4) {
		Helpers::warn("Warning: Invalid input format for Y2R conversion\n");
	} else {
		inputFmt = static_cast<InputFormat>(format);
	}

	mem.write32(messagePointer, IPC::responseHeader(0x1, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void Y2RService::getInputFormat(u32 messagePointer) {
	log("Y2R::GetInputFormat\n");

	mem.write32(messagePointer, IPC::responseHeader(0x2, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, static_cast<u32>(inputFmt));
}

void Y2RService::setOutputFormat(u32 messagePointer) {
	const u32 format = mem.read32(messagePointer + 4);
	log("Y2R::SetOutputFormat (format = %d)\n", format);

	if (format > 3) {
		Helpers::warn("Warning: Invalid output format for Y2R conversion\n");
	} else {
		outputFmt = static_cast<OutputFormat>(format);
	}

	mem.write32(messagePointer, IPC::responseHeader(0x3, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void Y2RService::getOutputFormat(u32 messagePointer) {
	log("Y2R::GetOutputFormat\n");

	mem.write32(messagePointer, IPC::responseHeader(0x4, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, static_cast<u32>(outputFmt));
}

void Y2RService::setPackageParameter(u32 messagePointer) {
	// Package parameter is 3 words
	const u32 word1 = mem.read32(messagePointer + 4);
	const u32 word2 = mem.read32(messagePointer + 8);
	const u32 word3 = mem.read32(messagePointer + 12);
	Helpers::warn("Y2R::SetPackageParameter\n");

	mem.write32(messagePointer, IPC::responseHeader(0x29, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void Y2RService::setRotation(u32 messagePointer) {
	const u32 rot = mem.read32(messagePointer + 4);
	log("Y2R::SetRotation (format = %d)\n", rot);

	if (rot > 3) {
		Helpers::warn("Warning: Invalid rotation for Y2R conversion\n");
	} else {
		rotation = static_cast<Rotation>(rot);
	}

	mem.write32(messagePointer, IPC::responseHeader(0x5, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void Y2RService::setAlpha(u32 messagePointer) {
	alpha = static_cast<u8>(mem.read16(messagePointer + 4));
	log("Y2R::SetAlpha (value = %02X)\n", alpha);

	mem.write32(messagePointer, IPC::responseHeader(0x22, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void Y2RService::setSpacialDithering(u32 messagePointer) {
	const bool enable = mem.read32(messagePointer + 4) != 0;
	log("Y2R::SetSpacialDithering (enable = %d)\n", enable);

	spacialDithering = enable;
	mem.write32(messagePointer, IPC::responseHeader(0x9, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void Y2RService::setTemporalDithering(u32 messagePointer) {
	const bool enable = mem.read32(messagePointer + 4) != 0;
	log("Y2R::SetTemporalDithering (enable = %d)\n", enable);

	temporalDithering = enable;
	mem.write32(messagePointer, IPC::responseHeader(0xB, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void Y2RService::setInputLineWidth(u32 messagePointer) {
	const u16 width = mem.read16(messagePointer + 4);
	log("Y2R::SetInputLineWidth (width = %d)\n", width);

	mem.write32(messagePointer, IPC::responseHeader(0x1A, 1, 0));
	// Width must be > 0, <= 1024 and must be aligned to 8 pixels
	if (width == 0 || width > 1024 || (width & 7) != 0) {
		Helpers::panic("Y2R: Invalid input line width");
	} else {
		inputLineWidth = width;
		mem.write32(messagePointer + 4, Result::Success);
	}
}

void Y2RService::getInputLineWidth(u32 messagePointer) {
	log("Y2R::GetInputLineWidth\n");

	mem.write32(messagePointer, IPC::responseHeader(0x1B, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, inputLineWidth);
}

void Y2RService::setInputLines(u32 messagePointer) {
	const u16 lines = mem.read16(messagePointer + 4);
	log("Y2R::SetInputLines (lines = %d)\n", lines);
	mem.write32(messagePointer, IPC::responseHeader(0x1C, 1, 0));

	// Width must be > 0, <= 1024 and must be aligned to 8 pixels
	if (lines == 0 || lines > 1024) {
		Helpers::panic("Y2R: Invalid input line count");
	} else {
		// According to Citra, the Y2R module seems to accidentally skip setting the line # if it's 1024
		if (lines != 1024) {
			inputLines = lines;
		}
		mem.write32(messagePointer + 4, Result::Success);
	}
}

void Y2RService::getInputLines(u32 messagePointer) {
	log("Y2R::GetInputLines\n");

	mem.write32(messagePointer, IPC::responseHeader(0x1D, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, inputLines);
}

void Y2RService::setStandardCoeff(u32 messagePointer) {
	const u32 coeff = mem.read32(messagePointer + 4);
	log("Y2R::SetStandardCoeff (coefficient = %d)\n", coeff);
	mem.write32(messagePointer, IPC::responseHeader(0x20, 1, 0));

	if (coeff > 3) {  // Invalid coefficient, should have an error code
		Helpers::panic("Y2R: Invalid standard coefficient (coefficient = %d)\n", coeff);
	} else {
		conversionCoefficients = standardCoefficients[coeff];
		mem.write32(messagePointer + 4, Result::Success);
	}
}

void Y2RService::getStandardCoefficientParams(u32 messagePointer) {
	const u32 coefficientIndex = mem.read32(messagePointer + 4);
	log("Y2R::GetStandardCoefficientParams (coefficient = %d)\n", coefficientIndex);

	if (coefficientIndex > 3) {  // Invalid coefficient, should have an error code
		Helpers::panic("Y2R: Invalid standard coefficient (coefficient = %d)\n", coefficientIndex);
	} else {
		mem.write32(messagePointer, IPC::responseHeader(0x21, 5, 0));
		mem.write32(messagePointer + 4, Result::Success);
		const auto& coeff = standardCoefficients[coefficientIndex];

		// Write standard coefficient parameters to output buffer
		for (int i = 0; i < 8; i++) {
			const u32 pointer = messagePointer + 8 + i * sizeof(u16);  // Pointer to write parameter to
			mem.write16(pointer, coeff[i]);
		}
	}
}

void Y2RService::setCoefficientParams(u32 messagePointer) {
	log("Y2R::SetCoefficientParams\n");
	auto& coeff = conversionCoefficients;

	// Write coefficient parameters to output buffer
	for (int i = 0; i < 8; i++) {
		const u32 pointer = messagePointer + 4 + i * sizeof(u16);  // Pointer to write parameter to
		coeff[i] = mem.read16(pointer);
	}

	mem.write32(messagePointer, IPC::responseHeader(0x1E, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void Y2RService::getCoefficientParams(u32 messagePointer) {
	log("Y2R::GetCoefficientParams\n");
	mem.write32(messagePointer, IPC::responseHeader(0x1F, 5, 0));
	mem.write32(messagePointer + 4, Result::Success);
	const auto& coeff = conversionCoefficients;

	// Write coefficient parameters to output buffer
	for (int i = 0; i < 8; i++) {
		const u32 pointer = messagePointer + 8 + i * sizeof(u16);  // Pointer to write parameter to
		mem.write16(pointer, coeff[i]);
	}
}

void Y2RService::setSendingY(u32 messagePointer) {
	log("Y2R::SetSendingY\n");
	srcY = {mem.read32(messagePointer + 4), mem.read32(messagePointer + 8), mem.read32(messagePointer + 12), mem.read32(messagePointer + 16)};

	mem.write32(messagePointer, IPC::responseHeader(0x10, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void Y2RService::setSendingU(u32 messagePointer) {
	log("Y2R::SetSendingU\n");
	srcU = {mem.read32(messagePointer + 4), mem.read32(messagePointer + 8), mem.read32(messagePointer + 12), mem.read32(messagePointer + 16)};

	mem.write32(messagePointer, IPC::responseHeader(0x11, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void Y2RService::setSendingV(u32 messagePointer) {
	log("Y2R::SetSendingV\n");
	srcV = {mem.read32(messagePointer + 4), mem.read32(messagePointer + 8), mem.read32(messagePointer + 12), mem.read32(messagePointer + 16)};

	mem.write32(messagePointer, IPC::responseHeader(0x12, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void Y2RService::setSendingYUV(u32 messagePointer) {
	log("Y2R::SetSendingYUV\n");
	srcYUYV = {mem.read32(messagePointer + 4), mem.read32(messagePointer + 8), mem.read32(messagePointer + 12), mem.read32(messagePointer + 16)};

	mem.write32(messagePointer, IPC::responseHeader(0x13, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void Y2RService::setReceiving(u32 messagePointer) {
	log("Y2R::SetReceiving\n");
	dst = {mem.read32(messagePointer + 4), mem.read32(messagePointer + 8), mem.read32(messagePointer + 12), mem.read32(messagePointer + 16)};

	mem.write32(messagePointer, IPC::responseHeader(0x18, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void Y2RService::startConversion(u32 messagePointer) {
	log("Y2R::StartConversion\n");

	convert();

	mem.write32(messagePointer, IPC::responseHeader(0x26, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);

	// Schedule Y2R conversion end event.
	// The tick value is tweaked based on the minimum delay needed to get FIFA 15 to not hang due to a race condition on its title screen
	static constexpr u64 delayTicks = 1'350'000;
	isBusy = true;

	// Remove any potential pending Y2R event and schedule a new one
	Scheduler& scheduler = kernel.getScheduler();
	scheduler.rescheduleEvent(Scheduler::EventType::SignalY2R, scheduler.currentTimestamp + delayTicks);
}

void Y2RService::isFinishedSendingYUV(u32 messagePointer) {
	log("Y2R::IsFinishedSendingYUV");
	constexpr bool finished = true;  // For now, Y2R transfers are instant

	mem.write32(messagePointer, IPC::responseHeader(0x14, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, finished ? 1 : 0);
}

void Y2RService::isFinishedSendingY(u32 messagePointer) {
	log("Y2R::IsFinishedSendingY");
	constexpr bool finished = true;

	mem.write32(messagePointer, IPC::responseHeader(0x15, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, finished ? 1 : 0);
}

void Y2RService::isFinishedSendingU(u32 messagePointer) {
	log("Y2R::IsFinishedSendingU");
	constexpr bool finished = true;

	mem.write32(messagePointer, IPC::responseHeader(0x16, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, finished ? 1 : 0);
}

void Y2RService::isFinishedSendingV(u32 messagePointer) {
	log("Y2R::IsFinishedSendingV");
	constexpr bool finished = true;

	mem.write32(messagePointer, IPC::responseHeader(0x17, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, finished ? 1 : 0);
}

void Y2RService::isFinishedReceiving(u32 messagePointer) {
	log("Y2R::IsFinishedSendingReceiving");
	constexpr bool finished = true;  // For now, receiving components is also instant

	mem.write32(messagePointer, IPC::responseHeader(0x19, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, finished ? 1 : 0);
}

void Y2RService::signalConversionDone() {
	if (isBusy) {
		isBusy = false;

		// Signal the transfer end event if it's been created. TODO: Is this affected by SetTransferEndInterrupt?
		if (transferEndEvent.has_value()) {
			kernel.signalEvent(transferEndEvent.value());
		}
	}
}

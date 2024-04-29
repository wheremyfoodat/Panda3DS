#pragma once
#include <capstone/capstone.h>

#include <span>
#include <string>

#include "helpers.hpp"

namespace Common {
	class CapstoneDisassembler {
		csh handle;                       // Handle to our disassembler object
		cs_insn* instructions = nullptr;  // Pointer to instruction object
		bool initialized = false;

	  public:
		bool isInitialized() { return initialized; }
		void init(cs_arch arch, cs_mode mode) { initialized = (cs_open(arch, mode, &handle) == CS_ERR_OK); }

		CapstoneDisassembler() {}
		CapstoneDisassembler(cs_arch arch, cs_mode mode) { init(arch, mode); }

		// Returns the number of instructions successfully disassembled
		// pc: program counter of the instruction to disassemble
		// bytes: Byte representation of instruction
		// buffer: text buffer to output the disassembly too
		usize disassemble(std::string& buffer, u32 pc, std::span<u8> bytes, u64 offset = 0) {
			if (!initialized) {
				buffer = "Capstone was not properly initialized";
				return 0;
			}

			usize count = cs_disasm(handle, bytes.data(), bytes.size(), pc, offset, &instructions);
			if (count == 0) {
				// Error in disassembly, quit early and return empty string
				buffer = "Error disassembling instructions with Capstone";
				return 0;
			}

			buffer = "";
			for (usize i = 0; i < count; i++) {
				buffer += std::string(instructions[i].mnemonic) + " " + std::string(instructions[i].op_str);

				if (i < count - 1) {
					// Append newlines between instructions, sans the final instruction
					buffer += "\n";
				}
			}

			cs_free(instructions, count);
			return count;
		}
	};
}  // namespace Common

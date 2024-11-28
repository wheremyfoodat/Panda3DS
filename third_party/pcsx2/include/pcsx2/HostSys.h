// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include <atomic>
#include <map>
#include <memory>
#include <string>

#include "helpers.hpp"

class PageProtectionMode {
  protected:
	bool m_read = false;
	bool m_write = false;
	bool m_exec = false;

  public:
	constexpr PageProtectionMode() = default;

	constexpr PageProtectionMode& Read(bool allow = true) {
		m_read = allow;
		return *this;
	}

	constexpr PageProtectionMode& Write(bool allow = true) {
		m_write = allow;
		return *this;
	}

	constexpr PageProtectionMode& Execute(bool allow = true) {
		m_exec = allow;
		return *this;
	}

	constexpr PageProtectionMode& All(bool allow = true) {
		m_read = m_write = m_exec = allow;
		return *this;
	}

	constexpr bool CanRead() const { return m_read; }
	constexpr bool CanWrite() const { return m_write; }
	constexpr bool CanExecute() const { return m_exec && m_read; }
	constexpr bool IsNone() const { return !m_read && !m_write; }
};

static PageProtectionMode PageAccess_None() { return PageProtectionMode(); }
static PageProtectionMode PageAccess_ReadOnly() { return PageProtectionMode().Read(); }
static PageProtectionMode PageAccess_WriteOnly() { return PageProtectionMode().Write(); }
static PageProtectionMode PageAccess_ReadWrite() { return PageAccess_ReadOnly().Write(); }
static PageProtectionMode PageAccess_ExecOnly() { return PageAccess_ReadOnly().Execute(); }
static PageProtectionMode PageAccess_Any() { return PageProtectionMode().All(); }

// --------------------------------------------------------------------------------------
//  HostSys
// --------------------------------------------------------------------------------------
namespace HostSys {
	// Maps a block of memory for use as a recompiled code buffer.
	// Returns NULL on allocation failure.
	extern void* Mmap(void* base, size_t size, const PageProtectionMode& mode);

	// Unmaps a block allocated by SysMmap
	extern void Munmap(void* base, size_t size);

	extern void MemProtect(void* baseaddr, size_t size, const PageProtectionMode& mode);

	extern std::string GetFileMappingName(const char* prefix);
	extern void* CreateSharedMemory(const char* name, size_t size);
	extern void DestroySharedMemory(void* ptr);
	extern void* MapSharedMemory(void* handle, size_t offset, void* baseaddr, size_t size, const PageProtectionMode& mode);
	extern void UnmapSharedMemory(void* baseaddr, size_t size);

	/// Returns the size of pages for the current host.
	size_t GetRuntimePageSize();
}  // namespace HostSys

class SharedMemoryMappingArea {
  public:
	static std::unique_ptr<SharedMemoryMappingArea> Create(size_t size);

	~SharedMemoryMappingArea();

	size_t GetSize() const { return m_size; }
	size_t GetNumPages() const { return m_num_pages; }

	u8* BasePointer() const { return m_base_ptr; }
	u8* OffsetPointer(size_t offset) const { return m_base_ptr + offset; }
	u8* PagePointer(size_t page) const { return m_base_ptr + __pagesize * page; }

	u8* Map(void* file_handle, size_t file_offset, void* map_base, size_t map_size, const PageProtectionMode& mode);
	bool Unmap(void* map_base, size_t map_size);

  private:
	SharedMemoryMappingArea(u8* base_ptr, size_t size, size_t num_pages);

	u8* m_base_ptr;
	size_t m_size;
	size_t m_num_pages;
	size_t m_num_mappings = 0;

#ifdef _WIN32
	using PlaceholderMap = std::map<size_t, size_t>;

	PlaceholderMap::iterator FindPlaceholder(size_t page);

	PlaceholderMap m_placeholder_ranges;
#endif
};
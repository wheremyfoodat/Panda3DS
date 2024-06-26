#include "fcram.hpp"
#include "memory.hpp"

void KFcram::Region::reset(u32 start, size_t size) {
	this->start = start;
	pages = size >> 12;
	freePages = pages;

	Block initialBlock(pages, 0);
	blocks.clear();
	blocks.push_back(initialBlock);
}

void KFcram::Region::alloc(std::list<FcramBlock>& out, s32 allocPages, bool linear) {
	for (auto it = blocks.begin(); it != blocks.end(); it++) {
		if (it->used) continue;

		// On linear allocations, only a single contiguous block may be used
		if (it->pages < allocPages && linear) continue;

		// If the current block is bigger than the allocation, split it
		if (it->pages > allocPages) {
			Block newBlock(it->pages - allocPages, it->pageOffs + allocPages);
			it->pages = allocPages;
			blocks.insert(it, newBlock);
		}

		// Mark the block as allocated and add it to the output list
		it->used = true;
		allocPages -= it->pages;
		freePages -= it->pages;

		u32 paddr = start + (it->pageOffs << 12);
		FcramBlock outBlock(paddr, it->pages);
		out.push_back(outBlock);

		if (allocPages < 1) return;
	}

	// Official kernel panics here
	Helpers::panic("Failed to allocate FCRAM, not enough guest memory");
}

u32 KFcram::Region::getUsedCount() {
	return pages - freePages;
}

u32 KFcram::Region::getFreeCount() {
	return freePages;
}

KFcram::KFcram(Memory& mem) : mem(mem) {}

void KFcram::reset(size_t ramSize, size_t appSize, size_t sysSize, size_t baseSize) {
	fcram = mem.getFCRAM();
	refs = std::unique_ptr<u32>(new u32[ramSize >> 12]);
	memset(refs.get(), 0, (ramSize >> 12) * sizeof(u32));

	appRegion.reset(0, appSize);
	sysRegion.reset(appSize, sysSize);
	baseRegion.reset(appSize + sysSize, baseSize);
}

void KFcram::alloc(FcramBlockList& out, s32 pages, FcramRegion region, bool linear) {
	switch (region) {
		case FcramRegion::App:
			appRegion.alloc(out, pages, linear);
			break;
		case FcramRegion::Sys:
			sysRegion.alloc(out, pages, linear);
			break;
		case FcramRegion::Base:
			baseRegion.alloc(out, pages, linear);
			break;
		default: Helpers::panic("Invalid FCRAM region chosen for allocation!"); break;
	}

	incRef(out);
}

void KFcram::incRef(FcramBlockList& list) {
	for (auto it = list.begin(); it != list.end(); it++) {
		for (int i = 0; i < it->pages; i++) {
			u32 index = (it->paddr >> 12) + i;
			refs.get()[index]++;
		}
	}
}

void KFcram::decRef(FcramBlockList& list) {
	for (auto it = list.begin(); it != list.end(); it++) {
		for (int i = 0; i < it->pages; i++) {
			u32 index = (it->paddr >> 12) + i;
			refs.get()[index]--;
			if (!refs.get()[index]) Helpers::panic("TODO: Freeing FCRAM");
		}
	}
}

u32 KFcram::getUsedCount(FcramRegion region) {
	switch (region) {
		case FcramRegion::App: return appRegion.getUsedCount();
		case FcramRegion::Sys: return sysRegion.getUsedCount();
		case FcramRegion::Base: return baseRegion.getUsedCount();
		default: Helpers::panic("Invalid FCRAM region in getUsedCount!");
	}
}
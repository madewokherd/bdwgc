#include "private/gc_priv.h"

void GC_foreach_heap_section(void* user_data, GC_heap_section_proc callback)
{
	GC_ASSERT(I_HOLD_LOCK());

	if (callback == NULL)
		return;

	// GC memory is organized in heap sections, which are split in heap blocks.
	// Each block has header (can get via HDR(ptr)) and it's size is aligned to HBLKSIZE
	// Block headers are kept separately from memory their points to, and quickly address
	// headers GC maintains 2-level cache structure which uses address as a hash key.
	for (unsigned i = 0; i < GC_n_heap_sects; i++)
	{
		ptr_t sectionStart = GC_heap_sects[i].hs_start;
		ptr_t sectionEnd = sectionStart + GC_heap_sects[i].hs_bytes;

		// Merge in contiguous sections.
		// A heap block might start in one heap section and extend 
		// into the next one. 
		while (i + 1 < GC_n_heap_sects && GC_heap_sects[i + 1].hs_start == sectionEnd)
		{
			++i;
			sectionEnd = GC_heap_sects[i].hs_start + GC_heap_sects[i].hs_bytes;
		}

		while (sectionStart < sectionEnd)
		{
			// This does lookup into 2 level tree data structure,
			// which uses address as hash key to find block header.
			hdr* hhdr = HDR(sectionStart);

			ptr_t nextSectionStart = NULL;
			if (IS_FORWARDING_ADDR_OR_NIL(hhdr))
			{
				// This pointer has no header registered in headers cache
				// We skip one HBLKSIZE and attempt to get header for it
				nextSectionStart = sectionStart + HBLKSIZE;
				callback(user_data, sectionStart, nextSectionStart, GC_HEAP_SECTION_TYPE_FREE);
			}
			else if (HBLK_IS_FREE(hhdr))
			{
				// We have a header, and the block is marked as free
				// Note: for "free" blocks "hb_sz" = the size in bytes of the whole block.
				nextSectionStart = sectionStart + hhdr->hb_sz;
				callback(user_data, sectionStart, nextSectionStart, GC_HEAP_SECTION_TYPE_FREE);
			}
			else
			{
				// This heap block is used, report it
				// Note: for used blocks "hb_sz" = size in bytes, of objects in the block.
				ptr_t usedSectionEnd = sectionStart + hhdr->hb_sz;
				nextSectionStart = sectionStart + HBLKSIZE * OBJ_SZ_TO_BLOCKS(hhdr->hb_sz);
				callback(user_data, sectionStart, usedSectionEnd, GC_HEAP_SECTION_TYPE_USED);
				if (nextSectionStart > usedSectionEnd)
					callback(user_data, usedSectionEnd, nextSectionStart, GC_HEAP_SECTION_TYPE_PADDING);
			}

			sectionStart = nextSectionStart;
		}
	}
}

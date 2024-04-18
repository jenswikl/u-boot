/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright (c) 2024 Linaro Limited
 */

#ifndef __MEMTAG_H
#define __MEMTAG_H

#if CONFIG_IS_ENABLED(ARMV8_MTE)

#define __MEMTAG_TAG_SHIFT	56
#define __MEMTAG_TAG_WIDTH	4
#define __MEMTAG_TAG_MASK	(BIT(__MEMTAG_TAG_WIDTH) - 1)
#define MEMTAG_GRANULE_SIZE	16
#define __MEMTAG_GRANULE_MASK	(MEMTAG_GRANULE_SIZE - 1)

/*
 * memtag_set_tags() - Tag a memory range
 * @addr:	Start of memory range
 * @size:	Size of memory range
 * @tag:	Tag to use
 *
 * The memory range is updated with the supplied tag. An eventual tag
 * already present in the upper bits of the address in @addr is ignored.
 *
 * @addr and @size must be aligned/multiple of MEMTAG_GRANULE_SIZE.
 *
 * Returns an address with the new tag inserted to be used to access this
 * memory area.
 */
void *memtag_set_tags(void *addr, size_t size, uint8_t tag);

/*
 * memtag_set_random_tags() - Tag a memory range with a random tag
 * @addr:	Start of memory range
 * @size:	Size of memory range
 *
 * The memory range is updated with a randomly generated tag. An eventual
 * tag already present in the upper bits of the address in @addr is
 * ignored.
 *
 * @addr and @size must be aligned/multiple of MEMTAG_GRANULE_SIZE.
 *
 * Returns an address with the new tag inserted to be used to access this
 * memory area.
 */
void *memtag_set_random_tags(void *addr, size_t size);

/*
 * memtag_read_tag() - Read the tag stored with an address
 * @addr:	Address with a tag
 *
 * Returns the stored tag.
 */
uint8_t memtag_read_tag(const void *addr);

#else

#define __MEMTAG_TAG_SHIFT	0
#define __MEMTAG_TAG_WIDTH	0
#define __MEMTAG_TAG_MASK	0
#define MEMTAG_GRANULE_SIZE	1
#define __MEMTAG_GRANULE_MASK	0

static inline void *memtag_set_tags(void *addr, size_t size __unused,
				    uint8_t tag __unused)
{
	return addr;
}

static inline void *memtag_set_random_tags(void *addr, size_t size __unused)
{
	return addr;
}

static inline uint8_t memtag_read_tag(const void *addr __unused)
{
	return 0;
}
#endif

/*
 * memtag_strip_tag_vaddr() - Removes an eventual tag from an address
 * @addr:	Address to strip
 *
 * Returns a unsigned long with an eventual tag removed.
 */
static inline unsigned long memtag_strip_tag_vaddr(const void *addr)
{
	unsigned long va = (unsigned long)addr;

	va &= ~((unsigned long)__MEMTAG_TAG_MASK << __MEMTAG_TAG_SHIFT);

	return va;
}

/*
 * memtag_insert_tag_vaddr() - Inserts a tag into an address
 * @addr:	Address to transform
 * @tag:	Tag to insert
 *
 * Returns the address with the new tag inserted.
 */
static inline unsigned long memtag_insert_tag_vaddr(unsigned long addr,
						    uint8_t tag)
{
	unsigned long va = memtag_strip_tag_vaddr((void *)addr);
	unsigned long t = tag & __MEMTAG_TAG_MASK;

	va |= t << __MEMTAG_TAG_SHIFT;

	return va;
}

/*
 * memtag_insert_tag() - Inserts a tag into an address
 * @addr:	Address to transform
 * @tag:	Tag to insert
 *
 * Returns the address with the new tag inserted.
 */
static inline void *memtag_insert_tag(void *addr, uint8_t tag)
{
	return (void *)memtag_insert_tag_vaddr((unsigned long)addr, tag);
}

/*
 * memtag_get_tag() - Extract a tag from an address
 * @addr:	Address with an eventual tag
 *
 * Returns the extracted tag.
 */
static inline uint8_t memtag_get_tag(const void *addr)
{
	uint64_t va = (unsigned long)addr;

	return (va >> __MEMTAG_TAG_SHIFT) & __MEMTAG_TAG_MASK;
}

static inline void memtag_assert_tag(const void *addr __maybe_unused)
{
	assert(memtag_get_tag(addr) == memtag_read_tag(addr));
}

/*
 * memtag_strip_tag_const() - Removes an eventual tag from an address
 * @addr:	Address to strip
 *
 * Returns the address without an eventual tag.
 */
static inline const void *memtag_strip_tag_const(const void *addr)
{
	return (const void *)memtag_strip_tag_vaddr(addr);
}

/*
 * memtag_strip_tag() - Removes an eventual tag from an address
 * @addr:	Address to strip
 *
 * Returns the address without an eventual tag.
 */
static inline void *memtag_strip_tag(void *addr)
{
	return (void *)memtag_strip_tag_vaddr(addr);
}

#endif /*__MEMTAG_H*/

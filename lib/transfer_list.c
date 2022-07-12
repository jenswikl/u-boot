// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2022, Linaro Limited
 */

#include <log.h>
#include <inttypes.h>
#include <transfer_list.h>
#include <string.h>

#define ADD_OVERFLOW(a, b, res) __builtin_add_overflow((a), (b), (res))

#define SUB_OVERFLOW(a, b, res) __builtin_sub_overflow((a), (b), (res))

#define MUL_OVERFLOW(a, b, res) __builtin_mul_overflow((a), (b), (res))

#define ROUNDUP_OVERFLOW(v, size, res) (__extension__({ \
	typeof(*(res)) __roundup_tmp = 0; \
	typeof(v) __roundup_mask = (typeof(v))(size) - 1; \
	\
	ADD_OVERFLOW((v), __roundup_mask, &__roundup_tmp) ? 1 : \
		(void)(*(res) = __roundup_tmp & ~__roundup_mask), 0; \
}))

struct transfer_list *transfer_list_init(void *p, size_t max_length)
{
	uintptr_t mask = (1 << (TRANSFER_LIST_GRANULE / 8)) - 1;
	struct transfer_list *tl = p;

	if (((uintptr_t)p & mask) || (max_length & mask) ||
	    max_length < sizeof(*tl))
		return NULL;

	memset(tl, 0, max_length);
	tl->signature = TRANSFER_LIST_SIGNATURE;
	tl->version = TRANSFER_LIST_VERSION;
	tl->length = sizeof(*tl);
	tl->max_length = max_length;
	transfer_list_update_checksum(tl);
	return tl;
}

static bool check_header(struct transfer_list *tl)
{
	if (tl->signature != TRANSFER_LIST_SIGNATURE) {
		log_err("Bad transfer list signature %#"PRIx32"\n",
			tl->signature);
		return false;
	}
	if (tl->version != TRANSFER_LIST_VERSION) {
		log_err("Unsupported transfer list version %#"PRIx32"\n",
			tl->version);
		return false;
	}
	if (!tl->max_length) {
		log_err("Bad transfer list max length %#"PRIx32"\n",
			tl->max_length);
		return false;;
	}
	if (tl->length > tl->max_length) {
		log_err("Bad transfer list length %#"PRIx32"\n", tl->length);
		return false;;
	}
	return true;
}

struct transfer_list *transfer_list_check_header(struct transfer_list *tl)
{
	if (!tl || !check_header(tl))
		return NULL;
	if (!transfer_list_verify_checksum(tl)) {
		log_err("Bad transfer list checksum %#x\n", tl->checksum);
		return NULL;
	}
	return tl;
}

struct transfer_entry *transfer_list_next(struct transfer_list *tl,
					  struct transfer_entry *last)
{
	struct transfer_entry *te = NULL;
	uintptr_t tl_ev = (uintptr_t)tl + tl->length;
	uintptr_t va = 0;
	uintptr_t ev = 0;
	size_t sz = 0;

	if (last) {
		va = (uintptr_t)last;
		if (ADD_OVERFLOW(last->hdr_size, last->data_size, &sz) ||
		    ADD_OVERFLOW(va, sz, &va) ||
		    ROUNDUP_OVERFLOW(va, TRANSFER_LIST_GRANULE, &va))
			return NULL;
	} else {
		va = (uintptr_t)(tl + 1);
	}
	te = (struct transfer_entry *)va;

	if (va + sizeof(*te) > tl_ev || te->hdr_size < sizeof(*te) ||
	    ADD_OVERFLOW(va, te->hdr_size, &ev) ||
	    ADD_OVERFLOW(ev, te->data_size, &ev) ||
	    ev > tl_ev)
		return NULL;

	return te;
}

static uint8_t calc_checksum(struct transfer_list *tl)
{
	uint8_t *b = (uint8_t *)tl;
	uint8_t cs = 0;
	size_t n = 0;

	for (n = 0; n < tl->length; n++)
		cs += b[n];

	return cs;
}

void transfer_list_update_checksum(struct transfer_list *tl)
{
	uint8_t cs = calc_checksum(tl);

	cs -= tl->checksum;
	cs = 256 - cs;
	tl->checksum = cs;
	assert((cs = calc_checksum(tl)) == 0 &&
		transfer_list_verify_checksum(tl));
}

bool transfer_list_verify_checksum(struct transfer_list *tl)
{
	return !calc_checksum(tl);
}

bool transfer_list_set_data_size(struct transfer_list *tl,
				 struct transfer_entry *te,
				 uint32_t new_data_size)
{
	uintptr_t tl_max_ev = (uintptr_t)tl + tl->max_length;
	uintptr_t tl_old_ev = (uintptr_t)tl + tl->length;
	uintptr_t new_ev = (uintptr_t)te;
	uintptr_t old_ev = (uintptr_t)te;

	if (ADD_OVERFLOW(old_ev, te->hdr_size, &old_ev) ||
	    ADD_OVERFLOW(old_ev, te->data_size, &old_ev) ||
	    ROUNDUP_OVERFLOW(old_ev, TRANSFER_LIST_GRANULE, &old_ev) ||
	    ADD_OVERFLOW(new_ev, te->hdr_size, &new_ev) ||
	    ADD_OVERFLOW(new_ev, new_data_size, &new_ev) ||
	    ROUNDUP_OVERFLOW(new_ev, TRANSFER_LIST_GRANULE, &new_ev))
		return false;

	if (new_ev > tl_max_ev)
		return false;

	te->data_size = new_data_size;

	if (new_ev > old_ev)
		tl->length += new_ev - old_ev;
	else
		tl->length -= old_ev - new_ev;

	if (new_ev != old_ev)
		memmove((void *)new_ev, (void *)old_ev, tl_old_ev - old_ev);

	return true;
}

void transfer_list_grow_to_max_data_size(struct transfer_list *tl,
					 struct transfer_entry *te)
{
	uint32_t sz = tl->max_length - tl->length +
		      round_up(te->data_size, TRANSFER_LIST_GRANULE);

	transfer_list_set_data_size(tl, te, sz);
	assert(te->data_size == sz);
}

void transfer_list_rem(struct transfer_list *tl, struct transfer_entry *te)
{
	uintptr_t tl_ev = (uintptr_t)tl + tl->length;
	uintptr_t ev = (uintptr_t)te;

	if (ADD_OVERFLOW(ev, te->hdr_size, &ev) ||
	    ADD_OVERFLOW(ev, te->data_size, &ev) ||
	    ROUNDUP_OVERFLOW(ev, TRANSFER_LIST_GRANULE, &ev) || ev > tl_ev)
		assert(0);

	memmove(te, (void *)ev, tl_ev - ev);
	tl->length -= ev - (uintptr_t)te;
}

struct transfer_entry *transfer_list_add(struct transfer_list *tl,
					 uint32_t tag_id, uint32_t data_size)
{
	uintptr_t max_tl_ev = (uintptr_t)tl + tl->max_length;
	uintptr_t tl_ev = (uintptr_t)tl + tl->length;
	struct transfer_entry *te = NULL;
	uintptr_t ev = tl_ev;

	if (ADD_OVERFLOW(ev, sizeof(*te), &ev) ||
	    ADD_OVERFLOW(ev, data_size, &ev) ||
	    ROUNDUP_OVERFLOW(ev, TRANSFER_LIST_GRANULE, &ev) || ev > max_tl_ev)
		return NULL;

	te = (struct transfer_entry *)tl_ev;
	memset(te, 0, ev - tl_ev);
	te->tag_id = tag_id;
	te->hdr_size = sizeof(*te);
	te->data_size = data_size;

	tl->length += ev - tl_ev;
	return te;
}

struct transfer_entry *transfer_list_find(struct transfer_list *tl,
					  uint32_t tag_id)
{
	struct transfer_entry *te = NULL;

	do
		te = transfer_list_next(tl, te);
	while (te && te->tag_id != tag_id);

	return te;
}

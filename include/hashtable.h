#ifndef HASHTABLE_H
#define HASHTABLE_H

#include <stddef.h>
#include <stdint.h>

struct HNode
{
	HNode *next = NULL;
	uint64_t hcode = 0;
};

struct HTable
{
	HNode **table = NULL; // array of HNode*
	size_t mask = 0;
	size_t size = 0;
};

struct HMap
{
	HTable ht_primary;
	HTable ht_secondary;
	size_t resizing_pos = 0;
};

HNode *hm_lookup(HMap *hmap, HNode *key, bool (*eq)(HNode *, HNode *));
void hm_insert(HMap *hmap, HNode *node);
HNode *hm_pop(HMap *hmap, HNode *key, bool (*eq)(HNode *, HNode *));
size_t hm_size(HMap *hmap);
void hm_destroy(HMap *hmap);

#endif
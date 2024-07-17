#include <assert.h>
#include <stdlib.h>

#include "../include/hashtable.h"
#include "../include/constants.h"

/**
 * @brief initialize a hashtable with n slots
 *
 *
 * @param *htable: pointer to Htable
 * @param n: no of slots of the hashtable
 */
static void h_init(HTable *htable, size_t n)
{
    assert(n > 0 && ((n - 1) & n) == 0);
    htable->table = (HNode **)calloc(sizeof(HNode *), n);
    htable->mask = n - 1;
    htable->size = 0;
}

/**
 * @brief inserts a HNode into the Htable
 *
 * finds the position of the slot and inserts the
 * new node at the start of the slot
 *
 * @param *htable: pointer to Htable
 * @param *node: pointer to HNode to be inserted
 */
static void h_insert(HTable *htable, HNode *node)
{
    size_t pos = node->hcode & htable->mask;
    HNode *first_node = htable->table[pos];
    node->next = first_node;
    htable->table[pos] = node;
    htable->size++;
}

/**
 * @brief lookup key in the hashtable
 *
 * finds the slot belonging to the key
 * checks in the chain if the key is present
 *
 * @param *htable: pointer to Htable
 * @param *key: key to be searched
 * @param (*eq)(HNode*, HNode*): equality callback for type HNode
 *
 * @return pointer to the address of the parent which
 * contains the node
 */
static HNode **h_lookup(HTable *htable, HNode *key, bool (*eq)(HNode *, HNode *))
{
    if (!htable->table)
    {
        return NULL;
    }

    size_t pos = key->hcode & htable->mask;
    HNode **from = &htable->table[pos]; // incoming pointer to the result
    for (HNode *cur; (cur = *from) != NULL; from = &cur->next)
    {
        if (cur->hcode == key->hcode && eq(cur, key))
        {
            return from;
        }
    }

    return NULL;
}

/**
 * @brief removes the HNode from table
 *
 * the from pointer is the imcoming pointer to the node which has
 * to be deleted i.e. from->next is the node to be deleted
 *
 * @param *htable: pointer to Htable
 * @param **from: the incoming node to the node which has to be removed
 *
 * @return the detached node
 */
static HNode *h_detach(HTable *htable, HNode **from)
{
    HNode *node = *from;
    *from = node->next;
    htable->size--;
    return node;
}

static void hm_help_resizing(HMap *hmap)
{
    size_t nwork = 0;
    while (nwork < k_resizing_work && hmap->ht_secondary.size > 0)
    {
        // scan for nodes from ht2 and move them to ht1
        HNode **from = &hmap->ht_secondary.table[hmap->resizing_pos];
        if (!*from)
        {
            hmap->resizing_pos++;
            continue;
        }

        h_insert(&hmap->ht_primary, h_detach(&hmap->ht_secondary, from));
        nwork++;
    }

    if (hmap->ht_secondary.size == 0 && hmap->ht_secondary.table)
    {
        // done
        free(hmap->ht_secondary.table);
        hmap->ht_secondary = HTable{};
    }
}

static void hm_start_resizing(HMap *hmap)
{
    assert(hmap->ht_secondary.table == NULL);
    // create a bigger hashtable and swap them
    hmap->ht_secondary = hmap->ht_primary;
    h_init(&hmap->ht_primary, (hmap->ht_primary.mask + 1) * 2);
    hmap->resizing_pos = 0;
}

/**
 * @brief insert the node into the hashmap
 *
 * the new table is the main table which is used
 * we insert the node in the new table
 *
 * if old table is not in use(meaning no resizing has
 * happened or previous resizing is complete),
 * we check the load-factor i.e. no of elements to no of buckets
 * resizing process is started if load-factor is more than threshold
 *
 * for resizing, the old table now holds the values of the new table
 * and new table is reinitialized with a bigger size
 *
 * @param *hmap: pointer to HMap
 * @param *node: pointer to HNode to be inserted
 *
 */
void hm_insert(HMap *hmap, HNode *node)
{
    if (!hmap->ht_primary.table)
    {
        h_init(&hmap->ht_primary, 4);
    }
    h_insert(&hmap->ht_primary, node);

    if (!hmap->ht_secondary.table)
    {
        size_t load_factor = hmap->ht_primary.size / (hmap->ht_primary.mask + 1);
        if (load_factor >= k_max_load_factor)
        {
            hm_start_resizing(hmap);
        }
    }
    hm_help_resizing(hmap);
}

HNode *hm_lookup(HMap *hmap, HNode *key, bool (*eq)(HNode *, HNode *))
{
    hm_help_resizing(hmap);
    HNode **from = h_lookup(&hmap->ht_primary, key, eq);
    from = from ? from : h_lookup(&hmap->ht_secondary, key, eq);
    return from ? *from : NULL;
}

HNode *hm_pop(HMap *hmap, HNode *key, bool (*eq)(HNode *, HNode *))
{
    hm_help_resizing(hmap);
    if (HNode **from = h_lookup(&hmap->ht_primary, key, eq))
    {
        return h_detach(&hmap->ht_primary, from);
    }
    if (HNode **from = h_lookup(&hmap->ht_secondary, key, eq))
    {
        return h_detach(&hmap->ht_secondary, from);
    }
    return NULL;
}

size_t hm_size(HMap *hmap)
{
    return hmap->ht_primary.size + hmap->ht_secondary.size;
}

void hm_destroy(HMap *hmap)
{
    free(hmap->ht_primary.table);
    free(hmap->ht_secondary.table);
    *hmap = HMap{};
}
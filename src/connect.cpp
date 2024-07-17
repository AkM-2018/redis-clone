#include <vector>
#include <string.h>
#include <cassert>
#include <sstream>
#include "../include/connect.h"
#include "../include/utils/print_utils.h"
#include "../include/utils/string_utils.h"
#include "../include/enums/res_enum.h"
#include "../include/entry.h"

static struct
{
    HMap db;
} g_data;

#define container_of(ptr, type, member) ({                  \
    const typeof( ((type *)0)->member ) *__mptr = (ptr);    \
    (type *)( (char *)__mptr - offsetof(type, member) ); })

static bool entry_eq(HNode *lhs, HNode *rhs)
{
    struct Entry *le = container_of(lhs, struct Entry, node);
    struct Entry *re = container_of(rhs, struct Entry, node);
    return le->key == re->key;
}

void Connect::do_set(std::vector<std::string> cmd)
{
    Entry key;
    key.key.swap(cmd[1]);
    key.node.hcode = str_hash((uint8_t *)key.key.data(), key.key.size());

    HNode *node = hm_lookup(&g_data.db, &key.node, &entry_eq);
    if (node)
    {
        container_of(node, Entry, node)->val.swap(cmd[2]);
    }
    else
    {
        Entry *ent = new Entry();
        ent->key.swap(key.key);
        ent->node.hcode = key.node.hcode;
        ent->val.swap(cmd[2]);
        hm_insert(&g_data.db, &ent->node);
    }

    this->rescode = RES_OK;
}

void Connect::do_get(std::vector<std::string> cmd)
{
    Entry key;
    key.key = cmd[1];
    key.node.hcode = str_hash((uint8_t *)key.key.data(), key.key.size());

    HNode *node = hm_lookup(&g_data.db, &key.node, &entry_eq);
    if (!node)
    {
        this->rescode = RES_NX;
        return;
    }

    const std::string &val = container_of(node, Entry, node)->val;
    assert(val.size() <= k_max_msg);
    this->wbuf = val.data();
    this->wbuf_size = val.size();
    this->rescode = RES_OK;
}

void Connect::do_del(std::vector<std::string> cmd)
{
    Entry key;
    key.key.swap(cmd[1]);
    key.node.hcode = str_hash((uint8_t *)key.key.data(), key.key.size());

    HNode *node = hm_pop(&g_data.db, &key.node, &entry_eq);
    if (node)
    {
        delete container_of(node, Entry, node);
    }

    this->rescode = RES_OK;
}

void Connect::do_request(std::string data)
{
    std::vector<std::string> cmd;
    std::string tmp;

    std::stringstream ss(data);

    while (getline(ss, tmp, ' '))
    {
        cmd.push_back(tmp);
    }

    for (auto ci : cmd)
        print(ci);

    if (cmd.size() == 2 && cmd_is(cmd[0], "get"))
    {
        do_get(cmd);
    }
    else if (cmd.size() == 3 && cmd_is(cmd[0], "set"))
    {
        do_set(cmd);
    }
    else if (cmd.size() == 2 && cmd_is(cmd[0], "del"))
    {
        do_del(cmd);
    }
    else
    {
        // cmd is not recognized
        this->rescode = RES_ERR;
        const char *msg = "Unknown cmd";
        this->wbuf = msg;
        this->wbuf_size = strlen(msg);
    }
}
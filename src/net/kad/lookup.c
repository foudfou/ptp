/* Copyright (c) 2020 Foudil Brétel.  All rights reserved. */
#include "log.h"
#include "utils/safer.h"
#include "net/kad/lookup.h"

void kad_lookup_init(struct kad_lookup *lookup)
{
    node_heap_init(&lookup->next, 32);
    node_heap_init(&lookup->past, 32);
    memset(lookup->par, 0, KAD_ALPHA_CONST);
    lookup->par_len = KAD_ALPHA_CONST;
}

void kad_lookup_terminate(struct kad_lookup *lookup)
{
    kad_lookup_reset(lookup);
    free_safer(lookup->next.items);
    free_safer(lookup->past.items);
}

void kad_lookup_reset(struct kad_lookup *lookup)
{
    lookup->round = 0;
    lookup->par_len = KAD_ALPHA_CONST;
    memset(lookup->par, 0, KAD_ALPHA_CONST);

    while (lookup->next.len > 0) {
        // log_error("___lookup reset: next.len=%ld", lookup->next.len);
        struct kad_node_lookup *nl = node_heap_pop(&lookup->next);
        // log_error("___lookup reset: next nl=%p", nl);
        if (nl) free_safer(nl);
    }

    while (lookup->past.len > 0) {
        struct kad_node_lookup *nl = node_heap_pop(&lookup->past);
        // log_error("___lookup reset: past nl=%p", nl);
        if (nl) free_safer(nl);
    }
}

bool kad_lookup_par_is_empty(struct kad_lookup *lookup)
{
    // Consider the whole par[], not its dynamic portion lookup->par_len.
    size_t par_len = KAD_K_CONST;
    size_t i = 0;
    while (i < par_len && lookup->par[i] == NULL)
        i++;
    return i == par_len;
}

/** \param node pointing to existing request in reqs_out */
bool kad_lookup_par_add(struct kad_lookup *lookup, struct kad_rpc_query *query)
{
    size_t i = 0;
    while (i < lookup->par_len && lookup->par[i] != NULL)
        i++;
    if (i == lookup->par_len)
        return false;
    lookup->par[i] = query;
    return true;
}

bool kad_lookup_par_remove(struct kad_lookup *lookup, const struct kad_rpc_query *query)
{
    // Consider the whole par[], not its dynamic portion lookup->par_len.
    for (size_t i = 0; i < KAD_K_CONST; ++i)
        if (query == lookup->par[i]) {
            lookup->par[i] = NULL;
            return true;
        }
    return false;
}

struct kad_node_lookup *
kad_lookup_new_from(const struct kad_node_info *info, const kad_guid target)
{
    struct kad_node_lookup *nl = malloc(sizeof(struct kad_node_lookup));
    if (!nl) {
        log_perror(LOG_ERR, "Failed malloc: %s.", errno);
        return NULL;
    }
    nl->target = target;
    nl->id = info->id;
    nl->addr = info->addr;
    return nl;
}

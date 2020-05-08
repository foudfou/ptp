/* Copyright (c) 2020 Foudil Brétel.  All rights reserved. */
#ifndef KAD_LOOKUP_H
#define KAD_LOOKUP_H

/**
 * State for the kad node lookup process.
 *
 * This state is accessed by 2 unsynced events: kad_lookup_recv() and
 * kad_lookup_progress().
 */

/*
  While node lookup is the most important procedure in kademlia, the paper
  gives a confusing description. This has led to varying interpretation (see
  references). Hereafter is our own interpretation.

  Q: What is node lookup ?

  A: « to locate the k closest nodes to some given node ID. »

  Q: How does the paper describe the process ?

  • The most important is to locate the k closest nodes to some given node ID.

  • Kademlia employs a recursive algorithm for node lookups. The lookup
    initiator starts by picking a nodes from its closest non-empty k-bucket.

  • The initiator then sends parallel, asynchronous FIND_NODE to the α nodes it
    has chosen.

  • α is a system-wide concurrency parameter, such as 3.

  • the initiator resends the FIND_NODE to nodes it has learned about from
    previous RPCs.

  • If a round of FIND_NODES fails to return a node any closer than the closest
    already seen, the initiator resends the FIND_NODE to all of the k closest
    nodes it has not already queried.

  • The lookup could terminate when the initiator has queried and gotten
    responses from the k closest nodesit has seen.

  Q: How does node lookup actually work ?

  A: Our high-level interpretation of the process:

  - recursive/iterative process

  - initiator picks alpha-const=3 closest nodes from his routes, adds them to
  (by-distance-)sorted list of unknown nodes, aka `lookup` list, and sends
  FIND_NODE in parallel [need timeout]

  - when a response arrives, add responding node to routes and lookup list.

  - nodes are removed from lookup list on iteration, when picking the next ones
  to issue FIND_NODE to.

  [By definition routes hold known nodes — not only contacted ones: « When a
  Kademlia node receives any message (request or reply) from another node, it
  updates the appropriate k-bucket for the sender’s node ID. », « When u learns
  of a new contact, it attempts to insert the contact in the appropriate
  k-bucket. » So we should just add learned nodes to routes with last_seen=0,
  when not already here, and systematically add them to the lookup list. In
  other words FIND_NODE does insert new nodes into routes.]

  - after round finishes, if returned nodes closer than lookup list, pick
  alpha, otherwise pick k. Iterate by sending them FIND_NODE in parallel. « If
  a round of find nodes fails to return a node any closer than the closest
  already seen, the initiator resends the find node to all of the k closest
  nodes it has not already queried. »

  - paper says we can begin a new round before all alpha nodes of last round
  answered. Basically that says: we can have alpha requests in flight; a round
  finishes for each received response; we just pick the first node from the
  lookup list for the next request.

  - lookup finishes when k responses received [why not until looked-up id
  found ? or max iteration reached ?].

  Q: How do we implement node lookups ?

  A: Design aspects of our implementation:

  - recursive lookup timer, state kept in kctx, with added `lookup` heap,
  `in_flight` request list and `contacted` heap. Timer doesn't need to be
  signaled, for ex. when receiving a response. [In practice the process is
  split into 2 async routines: 1. kad_lookup() which is scheduled periodically
  in a recursive manner and does the iteration work, 2. kad_lookup_response()
  which updates the lookup state accoordingly.]

  - pick alpha closest nodes from routes. Send them FIND_NODE. This effectively
  registers the requests to the request list and in_flight list.

  - when response received: corresponding request removed from request list
  [already done] and in_flight list; answering node inserted into routes or
  updated [already done]; (only new) learned nodes inserted to routes with
  last_seen null; nodes added to lookup list; lookup round incremented. Now if
  first of lookup heap is not closer than first of contacted heap, pick k on
  next iteration instead of alpha.

  - pick alpha (or next when parallel) closest nodes from lookup list. Add them
  to contacted list. Send them FIND_NODE. This effectively: removes them from
  lookup list; register requests to the request and in_flight lists. Iterate:
  goto previous.

  + lookup timer checks how many requests are in-flight via list of in-flight
  queries. When a request times out, remove it from in_flight and request
  lists.

  - we will limit running lookup processes to 1, simply by checking lookup
  round. Other lookup processes, like triggered by refresh, must be delayed.

  - when lookup round >= k: stop timer; reset lookup round; reset lookup list.

  Notes and references:

  - « Alpha and Parallelism.  Kademlia uses a value of 3 for alpha, the degree
  of parallelism used. It appears that (see stutz06) this value is optimal.
  There are at least three approaches to managing parallelism. The first is to
  launch alpha probes and wait until all have succeeded or timed out before
  iterating. This is termed strict parallelism. The second is to limit the
  number of probes in flight to alpha; whenever a probe returns a new one is
  launched. We might call this bounded parallelism. A third is to iterate after
  what seems to be a reasonable delay (duration unspecified), so that the
  number of probes in flight is some low multiple of alpha. This is loose
  parallelism and the approach used by Kademlia. »
  (http://xlattice.sourceforge.net/components/protocol/kademlia/specs.html#lookup)

  - https://github.com/libp2p/go-libp2p-kad-dht/issues/290

  - https://github.com/ntoll/drogulus/blob/master/drogulus/dht/lookup.py

  - https://pub.tik.ee.ethz.ch/students/2006-So/SA-2006-19.pdf
*/
#include "net/kad/routes.h"
#include "utils/heap.h"

struct kad_node_lookup {
    kad_guid                target;
    kad_guid                id;
    struct sockaddr_storage addr;
};

/**
 * Compares the distance of 2 nodes to a given target.
 *
 * Returns a negative number if a > b, 0 if a == b, a positive int if
 * a < b. Intended for min-heap.
 */
static inline
int node_heap_cmp(const struct kad_node_lookup *a,
                  const struct kad_node_lookup *b)
{
    if (memcmp(&a->target, &b->target, KAD_GUID_SPACE_IN_BYTES) != 0)
        return INT_MIN; // convention

    size_t i = 0;
    unsigned char xa, xb;
    while (i < KAD_GUID_SPACE_IN_BYTES) {
        // avoid kad_guid_xor()
        xa = a->id.bytes[i] ^ a->target.bytes[i];
        xb = b->id.bytes[i] ^ b->target.bytes[i];
        if (xa != xb)
            break;
        i++;
    }
    return i == KAD_GUID_SPACE_IN_BYTES ? 0 : xb - xa;
}

HEAP_GENERATE(node_heap, struct kad_node_lookup *)

struct kad_lookup {
    int                   round;
    struct kad_rpc_query *par[KAD_K_CONST]; // aka in-flight
    size_t                par_len;
    struct node_heap      next;
    struct node_heap      past;
};

void kad_lookup_init(struct kad_lookup *lookup);
void kad_lookup_terminate(struct kad_lookup *lookup);
void kad_lookup_reset(struct kad_lookup *lookup);
bool kad_lookup_par_is_empty(struct kad_lookup *lookup);
bool kad_lookup_par_add(struct kad_lookup *lookup, struct kad_rpc_query *query);
bool kad_lookup_par_remove(struct kad_lookup *lookup, const struct kad_rpc_query *query);
struct kad_node_lookup *kad_lookup_new_from(const struct kad_node_info *info, const kad_guid target);


#endif /* KAD_LOOKUP_H */

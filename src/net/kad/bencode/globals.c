/* Copyright (c) 2019 Foudil Brétel.  All rights reserved. */
#include "parser.h"

/**
 * Global storage for bencode parser - isolated to avoid ODR violations
 *
 * INITIALIZE WITH benc_repr_reset() on every use!
 *
 * Static storage prevents risking blowing up the stack (1280+ objects,
 * benc_lit 272B benc_node 2328B).
 *
 * TODO optimize storage. 1. explore (m)allocated storage: no shared state, not
 * so many objects in practice, threads get their own; 2. reduce size of
 * benc_lit and benc_node.
 */
struct benc_literal repr_literals[BENC_ROUTES_LITERAL_MAX];
struct benc_repr repr;

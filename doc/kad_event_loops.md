# Kademlia Event Loop Integration Patterns

[Claude-generated on 2025-08-23]

This document examines how different Kademlia/DHT implementations handle event loop integration and provides recommendations for making KAD libraries usable across different event-driven architectures.

## Problem Statement

Network libraries face a fundamental design decision: should they impose their own event loop or integrate with existing ones? This is particularly relevant for Kademlia DHT implementations that need to handle:

- Periodic maintenance (bucket refresh, node pings)
- Asynchronous network I/O (UDP messages)
- Timer-based operations (request timeouts, retries)

## Survey of Existing Implementations

### 1. Callback-Based Integration (KadC)

**Library**: [KadC](https://kadc.sourceforge.net/) - C library for Kademlia (originally for Overnet)

**Approach**: Provides callback interfaces for asynchronous operations
```c
// Example from KadC
KadC_find2(..., callback_function, user_data);
```

**Integration Pattern**:
- User registers callback functions
- Library calls them when events occur
- Works with any event loop that can handle callbacks

**Pros**:
- Easy integration with any event system
- Clear separation of concerns
- Minimal coupling

**Cons**:
- Can lead to callback hell for complex operations
- Error handling can become complex
- State management across callbacks

### 2. Periodic Polling (jech/dht)

**Library**: [jech/dht](https://github.com/jech/dht) - BitTorrent DHT library in C

**Approach**: Caller must periodically invoke library functions
```c
// Caller's responsibility
dht_periodic();  // Call periodically
```

**Integration Pattern**:
- Event loop calls `dht_periodic()` at regular intervals
- Library returns recommended sleep time in `tosleep` parameter
- Works with both event-driven and threaded code

**Pros**:
- Simple integration model
- Predictable behavior
- Works with any scheduling mechanism

**Cons**:
- Requires active polling even when idle
- Timing precision depends on caller
- Potential for missed events if polling is irregular

### 3. Imposed Event Loop (libtorrent)

**Library**: [libtorrent](https://github.com/arvidn/libtorrent) - Full BitTorrent implementation

**Approach**: Uses `io_context` from boost::asio
```cpp
// libtorrent approach
io_context ctx;
// All async operations use ctx
```

**Integration Pattern**:
- Library provides its own async I/O framework
- Users work within the provided event system
- Sophisticated async operations and disk I/O

**Pros**:
- High performance and well-tested
- Handles complex async patterns elegantly
- Rich ecosystem around boost::asio

**Cons**:
- Forces architectural decisions on users
- Heavy dependency on boost
- Harder to integrate with existing event loops

### 4. Event Emitter Pattern (libp2p)

**Library**: [libp2p](https://libp2p.io/) - Modular network stack

**Approach**: Emits events using `noun:verb` pattern
```javascript
// Example from JavaScript implementation
libp2p.addEventListener('peer:discovery', (event) => {
  // Handle discovered peer
});
```

**Integration Pattern**:
- Library emits structured events
- Users subscribe to relevant events
- Clean separation between library and application logic

**Pros**:
- Clean separation of concerns
- Flexible and extensible
- Works well with modern async patterns

**Cons**:
- More complex initial setup
- Event ordering can be tricky
- Potential for event handler memory leaks

## Recommended Integration Patterns for C Libraries

Based on the analysis, here are recommended approaches for making C Kademlia libraries maximally reusable:

### Option 1: Plugin Architecture
```c
struct kad_callbacks {
    void (*on_message_received)(const char *buf, size_t len, struct sockaddr *from);
    void (*on_timeout)(void *timer_id);
    void (*on_node_discovered)(struct kad_node_info *node);
    void (*on_error)(int error_code, const char *message);
};

// User provides integration functions
int kad_init(struct kad_callbacks *callbacks);
```

### Option 2: File Descriptor Integration
```c
// Return file descriptor that becomes readable when KAD needs attention
int kad_get_poll_fd(void);

// Process events when fd is readable
void kad_process_events(void);

// For epoll/kqueue integration
struct pollfd fds[] = {{kad_get_poll_fd(), POLLIN, 0}};
poll(fds, 1, timeout);
if (fds[0].revents & POLLIN) {
    kad_process_events();
}
```

### Option 3: Separate Concerns
```c
// Network I/O integration
int kad_get_socket_fd(void);           // UDP socket for DHT messages
void kad_handle_network_event(void);   // Process incoming messages

// Timer integration
int kad_get_next_timeout_ms(void);     // When next timeout occurs
void kad_handle_timeout(void);         // Process timeout events

// Example integration with epoll
struct epoll_event events[MAX_EVENTS];
int timeout = kad_get_next_timeout_ms();
int nfds = epoll_wait(epfd, events, MAX_EVENTS, timeout);

if (nfds == 0) {
    kad_handle_timeout();  // Timeout occurred
}
```

## Design Principles

### 1. Don't Impose Your Event Loop
- Let users keep their existing event infrastructure
- Provide clear integration points instead of forcing adoption

### 2. Provide Multiple Integration Options
- Support both callback and polling patterns
- Allow file descriptor-based integration for advanced users

### 3. Clear Separation of Concerns
- Network I/O, timers, and business logic should be separable
- Make it easy to test and mock individual components

### 4. Document Integration Patterns
- Provide clear examples for common event loops
- Show integration with epoll, kqueue, libuv, etc.

### 5. Handle Resource Management
- Clear ownership semantics for buffers and structures
- Provide cleanup functions for proper teardown

## Example Integration Templates

### Integration with epoll (Linux)
```c
void integrate_with_epoll(int epfd) {
    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = kad_get_socket_fd();
    epoll_ctl(epfd, EPOLL_CTL_ADD, kad_get_socket_fd(), &ev);

    // In event loop
    while (running) {
        int timeout = kad_get_next_timeout_ms();
        int nfds = epoll_wait(epfd, events, MAX_EVENTS, timeout);

        for (int i = 0; i < nfds; i++) {
            if (events[i].data.fd == kad_get_socket_fd()) {
                kad_handle_network_event();
            }
        }

        if (nfds == 0) {  // Timeout
            kad_handle_timeout();
        }
    }
}
```

### Integration with libuv
```c
void on_uv_timeout(uv_timer_t* handle) {
    kad_handle_timeout();
    uv_timer_start(handle, on_uv_timeout, kad_get_next_timeout_ms(), 0);
}

void on_uv_recv(uv_udp_t* handle, ssize_t nread, const uv_buf_t* buf,
                const struct sockaddr* addr, unsigned flags) {
    if (nread > 0) {
        kad_process_message(buf->base, nread, addr);
    }
}
```

## Conclusion

The most successful C libraries provide **integration flexibility** rather than **architectural prescription**. By offering multiple integration patterns and clear documentation, a Kademlia library can be adopted across different architectures and use cases.

The key is to design the API around the **essential operations** (network I/O, timers, callbacks) while leaving the **coordination mechanism** (event loop) to the user.

## References

- [KadC Library](https://kadc.sourceforge.net/)
- [jech/dht GitHub Repository](https://github.com/jech/dht)
- [libtorrent Documentation](https://libtorrent.org/)
- [libp2p Specifications](https://github.com/libp2p/specs)
- [Standard BitTorrent DHT Implementation](https://github.com/arvidn/libtorrent/blob/e2b12e72d89d3037a4d927bef70d663b1fbb2530/src/kademlia/node.cpp#L759)

# Go Channels in C — Go-Style Channel Primitive with pthreads

*CMPSC 473 — Operating Systems · The Pennsylvania State University · 2023*

**[Live Showcase →](https://halkhoori2000.github.io/Go-Channels-in-C/)**

A Go-inspired channel primitive implemented from scratch in C using POSIX threads — supporting buffered and unbuffered channels, blocking and non-blocking send/receive, graceful close/destroy, and a multi-channel `select` that blocks until any listed channel becomes ready.

---

## Overview

This project implements the full channel API used in Go's concurrency model, but in C with pthreads. A `channel_t` wraps a circular buffer, a mutex, two condition variables (one for senders, one for receivers), and two linked lists of semaphores used by `channel_select` to wake waiting `select` callers when any channel becomes ready.

Both buffered channels (capacity > 0) and unbuffered channels (capacity = 0) are supported. Unbuffered channels block the sender until a receiver is waiting, matching Go's synchronisation semantics.

## How It Works

**Buffered channels**: `channel_send` locks the mutex, waits on the `receive` condition variable while the circular buffer is full, writes to the buffer, then signals the `send` condition variable to wake any waiting receivers. `channel_receive` mirrors this pattern.

**Non-blocking variants**: `channel_non_blocking_send` and `channel_non_blocking_receive` return `CHANNEL_FULL` / `CHANNEL_EMPTY` immediately instead of blocking — used internally by `channel_select`.

**Close/destroy**: `channel_close` sets `open = false` and broadcasts on both condition variables so all blocked threads wake and return `CLOSED_ERROR`. `channel_destroy` frees all memory; it returns `DESTROY_ERROR` if called on an open channel.

**Select**: `channel_select` registers a per-call semaphore into each candidate channel's sender or receiver semaphore list, then loops attempting non-blocking operations on each channel. If none succeeds, it blocks on `sem_wait`. When any channel receives data or a sender arrives, it posts all registered semaphores, waking the blocked `select`. The winning operation completes, the semaphore is unregistered from all channels, and `selected_index` is set.

## Use Cases

- Learning Go-style CSP (Communicating Sequential Processes) concurrency without leaving C
- Building multi-producer / multi-consumer pipelines where producers and consumers run as pthreads
- Implementing fan-in / fan-out patterns using `channel_select` to multiplex across several channels
- Systems programming coursework on synchronisation primitives, condition variables, and semaphores

## Challenges

- **Unbuffered channel semantics**: A capacity-0 buffer meant the sender must block until a receiver consumes — achieving true rendezvous synchronisation required careful condition variable ordering so neither side spins.
- **Select without busy-waiting**: Registering a per-call semaphore into each channel's notification list, then using `sem_wait`, lets `channel_select` sleep until any channel posts — avoiding a polling loop while still supporting multiple channels in one call.
- **Semaphore unregistration on exit**: After select completes (success or error), the semaphore must be removed from every registered channel's list before returning — failing to do so leaves dangling pointers that corrupt future sends/receives on those channels.
- **Broadcast on close**: `channel_close` must wake *all* blocked senders and receivers with `pthread_cond_broadcast`, not `pthread_cond_signal`, and also post all select semaphores so that `channel_select` callers also unblock.
- **Thread-safe destroy**: `channel_destroy` must confirm the channel is closed before freeing memory; a race between destroy and a lingering blocked thread would corrupt the mutex being destroyed.

## Tech Stack

| Component | Detail |
|---|---|
| Language | C (C99) |
| Threading | POSIX pthreads |
| Synchronisation | `pthread_mutex_t`, `pthread_cond_t`, `sem_t` |
| Buffer | Circular buffer (FIFO) |
| Select notification | Per-call semaphore + linked-list registration |

## Project Structure

```
src/
├── channel.c / channel.h     # channel implementation (send, receive, select, close, destroy)
├── buffer.c / buffer.h       # circular FIFO buffer
├── linked_list.c / .h        # doubly-linked list (semaphore registration)
├── stress.c / stress_send_recv.c  # stress tests
├── test.c                    # unit test suite
├── Makefile
└── topology.txt / *.txt      # graph topology files for stress tests
```

## Build & Run

```bash
cd src
make
./test            # run unit tests
./stress          # run stress test
```

Requires GCC and pthreads. Tested on Linux x86-64.

# Cache Controller FSM Simulator

A simple C++ simulation of a finite-state machine (FSM) based cache controller for a direct-mapped cache. This project was developed for the **Computer Organization and Design** course to demonstrate how a cache controller handles read/write requests, cache hits, cache misses, block allocation, and write-back.

## Project Overview

This simulator models a simple system with three main parts:

- a **CPU request queue**
- a **direct-mapped cache controller**
- a **main memory** with fixed latency

The cache controller is implemented as an FSM with four states:

- `IDLE`
- `COMPARE_TAG`
- `WRITE_BACK`
- `ALLOCATE`

The simulator processes one CPU request at a time and prints a cycle-by-cycle trace showing the current state, request type, hit/miss result, action taken, and next state.

## Cache Configuration

The simulator uses the following cache settings:

- **Cache type:** Direct-mapped
- **Number of cache lines:** 1024
- **Block size:** 16 bytes
- **Words per block:** 4 words
- **Address size:** 32-bit
- **Offset bits:** 4
- **Index bits:** 10
- **Write policy:** Write-back
- **Miss policy:** Write-allocate
- **Memory latency:** 3 cycles

## FSM Behavior

### IDLE
Waits for the next CPU request.

### COMPARE_TAG
Checks the selected cache line using the index field of the address.  
If the valid bit and tag match, it is a hit.  
If a miss occurs, the controller checks whether the current line is dirty.

### WRITE_BACK
Used when a miss occurs and the current cache line contains modified data.  
The old block is written back to memory before replacement.

### ALLOCATE
Fetches the required block from memory and loads it into the cache.  
After allocation, the original request is retried.

## Features

- Simulates a simple FSM-based cache controller
- Supports both **read** and **write** requests
- Shows **cache hit** and **cache miss** behavior
- Demonstrates **dirty eviction** and **write-back**
- Tracks statistics such as:
  - total reads
  - total writes
  - total hits
  - total misses
  - total write-backs
  - total allocations
  - hit rate
  - total cycles

## File Structure

```text
.
├── simulator.cpp
└── README.md
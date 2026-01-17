# FPGA Inference Engine Firmware - Proof of Concept

## Overview

This is a **proof-of-concept** firmware for managing FPGA-accelerated LLM inference. It demonstrates the architecture with simulated hardware registers.

**NOT production-ready** - focuses on clean, pivotable code structure.

## Architecture

```
┌─────────────────────────────────────────────────┐
│              User Interface                     │
│  (stdin/stdout - commands & responses)          │
└─────────────────┬───────────────────────────────┘
                  │
┌─────────────────▼───────────────────────────────┐
│         Inference Engine Thread                 │
│                                                  │
│  • Task Queue (generate requests)               │
│  • Command Queue (stop/reset/shutdown)          │
│  • Engine State Machine (idle/generating)       │
└─────────────────┬───────────────────────────────┘
                  │
┌─────────────────▼───────────────────────────────┐
│         Accelerator Interface                   │
│                                                  │
│  • Register writes (configuration, start)       │
│  • Register reads (status, tokens)              │
│  • DMA buffer management (simulated)            │
└─────────────────┬───────────────────────────────┘
                  │
┌─────────────────▼───────────────────────────────┐
│         Hardware Registers (simulated)          │
│                                                  │
│  Control:  AP_CTRL, START, RESET                │
│  Config:   Input/Output/KV addresses            │
│  Task:     Prompt length, Task ID               │
│  Status:   Done, Idle, Output tokens            │
└─────────────────────────────────────────────────┘
```

## Key Components

### 1. Register Mapping (`xaccelerator_hw.h`)
- Auto-generated style (similar to Vivado HLS output)
- Defines register offsets and bit masks
- **NOT complete** - proof of concept only
- In real system: generated from HLS or IP-XACT

### 2. Queue Template (`queue.hpp`)
- Fixed-size circular buffer
- Used for both Tasks and Commands
- Thread-safe operations would be added for production

### 3. Types (`types.hpp`)
- Task: user requests (generate text)
- Command: control signals (stop/reset/shutdown)
- EngineState: tracks current operation
- AcceleratorStatus: mirrors hardware state

### 4. Accelerator Interface (`accelerator.hpp`)
- Abstracts register read/write operations
- Configuration: set once at startup (addresses, strides)
- Task control: per-inference settings
- Status polling: check for completion
- **Simulated** - uses printf instead of actual register I/O

### 5. Inference Engine (`inference_engine.cpp`)
- Main control loop
- Processes tasks from queue
- Handles interruption via commands
- Manages accelerator lifecycle

## Register Structure (Simplified)

```
0x00: AP_CTRL     - Start/Done/Idle/Reset bits
0x04: GIE         - Global Interrupt Enable
0x08: IER         - Interrupt Enable Register  
0x0C: ISR         - Interrupt Status Register

--- Configuration (set once) ---
0x10-0x14: Input buffer address (64-bit)
0x18-0x1C: Output buffer address (64-bit)
0x20-0x24: KV cache address (64-bit)
0x28: Stride
0x2C: Max tokens

--- Per-Task (set each inference) ---
0x30: Prompt length
0x34: Task ID
0x38: Task type

--- Status (read-only) ---
0x3C: Status register
0x40: Output token
0x44: Output valid flag
0x48: Error code
```

## Build & Run

```bash
# Compile
make

# Run
./inference_engine

# Or both
make run
```

## Usage

```
> Hello world                  # Generate response
> /stop                        # Stop current generation
> /reset                       # Clear KV cache
> /quit                        # Shutdown
```

## What's NOT Implemented

- [ ] Actual memory-mapped I/O (uses printf simulation)
- [ ] Real DMA transfers (buffers are in-process)
- [ ] Interrupt handling (uses polling)
- [ ] Complete register set (minimal proof-of-concept)
- [ ] Thread safety / mutexes (single-threaded access assumed)
- [ ] Real tokenizer (uses dummy char-to-int)
- [ ] Error recovery mechanisms
- [ ] Performance optimization
- [ ] Device tree parsing for base addresses

## Production TODOs

1. **Memory Mapping:**
   ```cpp
   // Replace printf with:
   int fd = open("/dev/mem", O_RDWR | O_SYNC);
   void* ptr = mmap(NULL, size, PROT_READ | PROT_WRITE, 
                    MAP_SHARED, fd, base_addr);
   volatile uint32_t* reg = (uint32_t*)ptr;
   *reg = value;  // Actual write
   ```

2. **DMA Setup:**
   - Use `/dev/udmabuf` or similar
   - Allocate contiguous memory regions
   - Setup scatter-gather if needed

3. **Interrupt Handling:**
   ```cpp
   // Add UIO or polling thread
   int uio_fd = open("/dev/uio0", O_RDWR);
   uint32_t info = 1;
   read(uio_fd, &info, sizeof(info));  // Wait for interrupt
   // Update status from ISR
   ```

4. **Thread Safety:**
   ```cpp
   std::mutex queue_mutex;
   std::lock_guard<std::mutex> lock(queue_mutex);
   taskQueue.push(task);
   ```

5. **Real Tokenizer:**
   - Integrate sentencepiece or tiktoken
   - Pre-load vocabulary

6. **Error Handling:**
   - Timeout detection
   - Hardware error recovery
   - Watchdog timers

## Architecture Notes

### Why This Design?

1. **Queued Interface** - Decouples user input from hardware latency
2. **Command Priority** - Allows interrupting long generations
3. **Stateful Accelerator** - KV cache persists across requests
4. **Clean Separation** - Register layer is easily swappable

### Key Design Decisions

- **No Register Wrappers Yet** - Direct access for easy debugging
- **Polling vs Interrupts** - Polling shown, interrupts would be added
- **Simple Queue** - No priority, works for PoC
- **Explicit State** - Engine state machine is clear and debuggable

### Pivoting to Production

To make this production-ready:
1. Add actual memory-mapped I/O
2. Implement interrupt handlers
3. Add thread safety primitives
4. Integrate real tokenizer
5. Add error handling and recovery
6. Performance profiling and optimization
7. Add logging framework
8. Device tree integration

## Testing

```bash
# Build
make clean && make

# Test basic flow
echo "test prompt" | ./inference_engine

# Test commands
./inference_engine
> Hello
> /stop
> /reset
> Another test
> /quit
```

## File Structure

```
fpga_firmware/
├── xaccelerator_hw.h      # Register definitions (HLS-style)
├── types.hpp              # Core types and enums
├── queue.hpp              # Queue template
├── accelerator.hpp        # Hardware interface
├── inference_engine.cpp   # Main implementation
├── Makefile              # Build system
└── README.md             # This file
```

## Notes for Thesis

This code demonstrates:
- ✅ Queued task management
- ✅ Command/control separation  
- ✅ State machine design
- ✅ Hardware register interface pattern
- ✅ Clean, maintainable architecture
- ✅ Easy to extend/modify

**Remember:** This is intentionally simplified for clarity. Production code would add safety, performance, and robustness features.

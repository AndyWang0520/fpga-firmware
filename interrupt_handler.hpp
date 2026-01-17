// interrupt_handler.hpp

#ifndef INTERRUPT_HANDLER_HPP
#define INTERRUPT_HANDLER_HPP

#include <cstdint>
#include <cstdio>
#include <fcntl.h>
#include <unistd.h>
#include <thread>
#include <atomic>
#include <functional>
#include <poll.h>
#include "xaccelerator_hw.h"

// Interrupt types
enum class InterruptType {
    NONE = 0,
    AP_DONE = 1,
    AP_READY = 2,
    TOKEN_READY = 4,
    ERROR = 8
};

using InterruptCallback = std::function<void(InterruptType)>;

class InterruptHandler {
private:
    int uio_fd;
    bool enabled;
    bool running;
    std::thread irq_thread;
    
    volatile uint32_t* reg_base;
    
    // Callbacks
    InterruptCallback on_done_callback;
    InterruptCallback on_ready_callback;
    InterruptCallback on_token_callback;
    InterruptCallback on_error_callback;
    
    // Statistics
    std::atomic<uint64_t> total_interrupts;
    std::atomic<uint64_t> done_count;
    std::atomic<uint64_t> ready_count;
    std::atomic<uint64_t> token_count;
    std::atomic<uint64_t> error_count;
    
    void irqServiceThread() {
        printf("[IRQ] Interrupt service thread started\n");
        
        struct pollfd fds;
        fds.fd = uio_fd;
        fds.events = POLLIN;
        
        while (running) {

            int ret = poll(&fds, 1, 1000); 
            
            if (ret < 0) {
                perror("[IRQ] Poll error");
                break;
            }
            
            if (ret == 0) {
                continue;
            }
            
            if (fds.revents & POLLIN) {
                uint32_t irq_count;
                ssize_t nb = read(uio_fd, &irq_count, sizeof(irq_count));
                
                if (nb == sizeof(irq_count)) {
                    total_interrupts++;
                    
                    uint32_t isr = readISR();
                
                    handleInterrupt(isr);
                    
                    clearISR(isr);
                    
                } else {
                    printf("[IRQ] Read error: %zd\n", nb);
                }
            }
        }
        
        printf("[IRQ] Interrupt service thread stopped\n");
    }
    

    uint32_t readISR() {
        #ifdef REAL_HARDWARE
        if (reg_base) {
            return reg_base[XACCELERATOR_CTRL_ADDR_ISR / 4];
        }
        #endif
        
        // Simulation: fake interrupt status
        static int sim_counter = 0;
        if (sim_counter++ % 10 == 0) {
            return 0x01;  // ap_done
        }
        return 0;
    }
    
    void clearISR(uint32_t mask) {
        #ifdef REAL_HARDWARE
        if (reg_base) {
            // Write-1-to-clear
            reg_base[XACCELERATOR_CTRL_ADDR_ISR / 4] = mask;
        }
        #else
        printf("[IRQ] Clearing ISR: 0x%08X\n", mask);
        #endif
    }
    
    void handleInterrupt(uint32_t isr) {
        // Check for ap_done (bit 0)
        if (isr & 0x01) {
            done_count++;
            if (on_done_callback) {
                on_done_callback(InterruptType::AP_DONE);
            }
        }
        
        // Check for ap_ready (bit 1)
        if (isr & 0x02) {
            ready_count++;
            if (on_ready_callback) {
                on_ready_callback(InterruptType::AP_READY);
            }
        }
        
        // Custom: token ready (bit 2, if implemented)
        if (isr & 0x04) {
            token_count++;
            if (on_token_callback) {
                on_token_callback(InterruptType::TOKEN_READY);
            }
        }
        
        // Custom: error (bit 3, if implemented)
        if (isr & 0x08) {
            error_count++;
            if (on_error_callback) {
                on_error_callback(InterruptType::ERROR);
            }
        }
    }

public:
    InterruptHandler() : uio_fd(-1), enabled(false), running(false),
                         reg_base(nullptr),
                         total_interrupts(0), done_count(0),
                         ready_count(0), token_count(0), error_count(0) {}
    
    ~InterruptHandler() {
        stop();
    }
    
    bool init(const char* uio_device, volatile uint32_t* registers = nullptr) {
        printf("[IRQ] Initializing interrupt handler...\n");
        
        reg_base = registers;
        
        #ifdef REAL_HARDWARE
 
        uio_fd = open(uio_device, O_RDWR);
        if (uio_fd < 0) {
            perror("[IRQ] Failed to open UIO device");
            printf("[IRQ] Make sure device exists: %s\n", uio_device);
            printf("[IRQ] Check: ls -l /dev/uio*\n");
            return false;
        }
        
        printf("[IRQ] Opened UIO device: %s (fd=%d)\n", uio_device, uio_fd);
        
        if (reg_base) {

            reg_base[XACCELERATOR_CTRL_ADDR_GIE / 4] = 0x01;
            reg_base[XACCELERATOR_CTRL_ADDR_IER / 4] = 0x03;
            
            printf("[IRQ] Hardware interrupts enabled\n");
        }
        
        #else
        printf("[IRQ] Running in simulation mode (no real UIO)\n");
        uio_fd = 0;  // Fake fd for simulation
        #endif
        
        enabled = true;
        return true;
    }
    
    // Start interrupt handling thread
    bool start() {
        if (!enabled) {
            printf("[IRQ] Not initialized\n");
            return false;
        }
        
        if (running) {
            printf("[IRQ] Already running\n");
            return true;
        }
        
        printf("[IRQ] Starting interrupt service thread...\n");
        running = true;
        irq_thread = std::thread(&InterruptHandler::irqServiceThread, this);
        
        return true;
    }
    
    // Stop interrupt handling
    void stop() {
        if (running) {
            printf("[IRQ] Stopping interrupt service thread...\n");
            running = false;
            
            if (irq_thread.joinable()) {
                irq_thread.join();
            }
        }
        
        #ifdef REAL_HARDWARE
        if (uio_fd >= 0) {
            // Disable interrupts
            if (reg_base) {
                reg_base[XACCELERATOR_CTRL_ADDR_GIE / 4] = 0x00;
                reg_base[XACCELERATOR_CTRL_ADDR_IER / 4] = 0x00;
            }
            
            close(uio_fd);
            uio_fd = -1;
        }
        #endif
        
        enabled = false;
    }
    
    void onDone(InterruptCallback cb) { on_done_callback = cb; }
    void onReady(InterruptCallback cb) { on_ready_callback = cb; }
    void onToken(InterruptCallback cb) { on_token_callback = cb; }
    void onError(InterruptCallback cb) { on_error_callback = cb; }
    
    uint64_t getTotalInterrupts() const { return total_interrupts; }
    uint64_t getDoneCount() const { return done_count; }
    uint64_t getReadyCount() const { return ready_count; }
    uint64_t getTokenCount() const { return token_count; }
    uint64_t getErrorCount() const { return error_count; }

    void printStats() {
        printf("\n[IRQ] Interrupt Statistics:\n");
        printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
        printf("Total interrupts:  %lu\n", total_interrupts.load());
        printf("  AP_DONE:         %lu\n", done_count.load());
        printf("  AP_READY:        %lu\n", ready_count.load());
        printf("  TOKEN_READY:     %lu\n", token_count.load());
        printf("  ERROR:           %lu\n", error_count.load());
        printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n\n");
    }
    
    // Manual interrupt enable 
    void enableInterrupt(uint32_t mask) {
        #ifdef REAL_HARDWARE
        if (reg_base) {
            uint32_t ier = reg_base[XACCELERATOR_CTRL_ADDR_IER / 4];
            reg_base[XACCELERATOR_CTRL_ADDR_IER / 4] = ier | mask;
        }
        #endif
    }
    
    // Manual interrupt disable
    void disableInterrupt(uint32_t mask) {
        #ifdef REAL_HARDWARE
        if (reg_base) {
            uint32_t ier = reg_base[XACCELERATOR_CTRL_ADDR_IER / 4];
            reg_base[XACCELERATOR_CTRL_ADDR_IER / 4] = ier & ~mask;
        }
        #endif
    }
    
    bool isRunning() const { return running; }
};

#endif // INTERRUPT_HANDLER_HPP

// accelerator.hpp

#ifndef ACCELERATOR_HPP
#define ACCELERATOR_HPP

#include "xaccelerator_hw.h"
#include "config_struct.hpp"
#include "types.hpp"
#include <cstdint>
#include <vector>
#include <cstdio>
#include <unistd.h>
#include <cstring>

class Accelerator {
private:
    uint32_t base_addr;
    
    ConfigIn config;
    StatusOut status;
    uint32_t config_words[38];
    uint32_t status_words[4];
    
    std::vector<uint32_t> input_buffer;
    std::vector<uint32_t> output_buffer;
    std::vector<uint32_t> kv_cache;
    
    void writeReg(uint32_t offset, uint32_t value) {
        // Simulated write
        printf("[HW_WRITE] 0x%04X = 0x%08X\n", offset, value);
        usleep(100); // Simulate I/O delay
    }
    
    uint32_t readReg(uint32_t offset) {
        // Simulated read
        uint32_t value = 0;
        
        // Simulate different register responses
        if (offset == XACCELERATOR_CTRL_ADDR_AP_CTRL) {
            value = XACCELERATOR_AP_CTRL_IDLE | XACCELERATOR_AP_CTRL_DONE;
        } else if (offset >= XACCELERATOR_STATUS_OUT_BASE && 
                   offset < XACCELERATOR_STATUS_OUT_BASE + 16) {
            // Return status words
            int word_idx = (offset - XACCELERATOR_STATUS_OUT_BASE) / 4;
            if (word_idx < 4) {
                value = status_words[word_idx];
            }
        } else if (offset == XACCELERATOR_STATUS_OUT_CTRL) {
            value = XACCELERATOR_STATUS_OUT_AP_VLD; // Always valid for simulation
        }
        
        printf("[HW_READ] 0x%04X = 0x%08X\n", offset, value);
        usleep(100);
        return value;
    }

public:
    Accelerator() : base_addr(XACCELERATOR_BASEADDR) {
        input_buffer.resize(4096);
        output_buffer.resize(1024);
        kv_cache.resize(65536);
        memset(config_words, 0, sizeof(config_words));
        memset(status_words, 0, sizeof(status_words));
    }
    
  
    void configure(uint64_t input_addr, uint64_t output_addr, 
                   uint64_t kv_cache_addr, uint32_t stride, 
                   uint32_t max_tokens) {
        printf("[ACCEL] Configuring accelerator with 1216-bit config_in...\n");
        

        config.input_buffer_addr = input_addr;
        config.output_buffer_addr = output_addr;
        config.kv_cache_addr = kv_cache_addr;
        config.stride = stride;
        config.max_tokens = max_tokens;

        config.pack(config_words);
        
 
        for (int i = 0; i < XACCELERATOR_CONFIG_IN_WORDS; i++) {
            uint32_t offset = XACCELERATOR_CONFIG_IN_OFFSET(i);
            writeReg(offset, config_words[i]);
        }
        
        printf("[ACCEL] Configuration complete (wrote %d words)\n", 
               XACCELERATOR_CONFIG_IN_WORDS);
    }
    
    void setTaskConfig(int task_id, uint32_t prompt_len) {
        printf("[ACCEL] Setting task config - ID: %d, PromptLen: %u\n", 
               task_id, prompt_len);
        

        config.task_id = task_id;
        config.prompt_length = prompt_len;
        config.task_type = 0; 
        

        config.pack(config_words);
        
        // Only write the changed words (optimization)
        // todo write the relevant section
        writeReg(XACCELERATOR_CONFIG_IN_OFFSET(14), config_words[14]); // prompt_length
        writeReg(XACCELERATOR_CONFIG_IN_OFFSET(15), config_words[15]); // task_id
        writeReg(XACCELERATOR_CONFIG_IN_OFFSET(16), config_words[16]); // task_type
    }
    
    void startInference(int task_id, const std::vector<uint32_t>& tokens) {
        printf("[ACCEL] Starting inference - Task ID: %d, Tokens: %zu\n", 
               task_id, tokens.size());
        setTaskConfig(task_id, tokens.size());
        
        for (size_t i = 0; i < tokens.size() && i < input_buffer.size(); i++) {
            input_buffer[i] = tokens[i];
        }
        
        printf("[ACCEL] Writing AP_START...\n");
        writeReg(XACCELERATOR_CTRL_ADDR_AP_CTRL, XACCELERATOR_AP_CTRL_START);
        
        status.tokens_generated = 0;
        status.flags = 0x01; // valid
        status.pack_to_words();
    }
    
    void readStatus() {
        uint32_t ctrl = readReg(XACCELERATOR_STATUS_OUT_CTRL);
        if (!(ctrl & XACCELERATOR_STATUS_OUT_AP_VLD)) {
            return; 
        }
        
        for (int i = 0; i < XACCELERATOR_STATUS_OUT_WORDS; i++) {
            uint32_t offset = XACCELERATOR_STATUS_OUT_OFFSET(i);
            status_words[i] = readReg(offset);
        }
        
        status.unpack(status_words);
    }
    
    bool getNextToken(uint32_t& token) {
        readStatus();
        
        if (status.isValid() && !status.isDone()) {
            token = status.current_token;
            
            static int token_counter = 0;
            if (token_counter++ > 10) {
                token = EOS_TOKEN;
                status.flags |= 0x02; 
                token_counter = 0;
            } else {
                token = 100 + token_counter; 
                status.tokens_generated++;
            }
            
            status.current_token = token;
            status.pack_to_words();
            
            return true;
        }
        
        return false;
    }
    
    bool isDone() {
        uint32_t ctrl = readReg(XACCELERATOR_CTRL_ADDR_AP_CTRL);
        return (ctrl & XACCELERATOR_AP_CTRL_DONE) != 0;
    }
    
    bool isIdle() {
        uint32_t ctrl = readReg(XACCELERATOR_CTRL_ADDR_AP_CTRL);
        return (ctrl & XACCELERATOR_AP_CTRL_IDLE) != 0;
    }
    

    void reset() {
        printf("[ACCEL] Resetting accelerator...\n");
        
        writeReg(XACCELERATOR_IRQ_CLEAR_IN, 0xFFFFFFFF);
        writeReg(XACCELERATOR_CTRL_ADDR_AP_CTRL, 0x00);
        
        for (size_t i = 0; i < kv_cache.size(); i++) {
            kv_cache[i] = 0;
        }
        
        printf("[ACCEL] Reset complete, KV cache cleared\n");
    }

    StatusOut getStatus() {
        readStatus();
        return status;
    }
};

#endif // ACCELERATOR_HPP

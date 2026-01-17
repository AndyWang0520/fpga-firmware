#ifndef CONFIG_STRUCT_HPP
#define CONFIG_STRUCT_HPP

#include <cstdint>
#include <cstring>

// ConfigIn: 1216 bits total = 38 x 32-bit words
// todo logical structure here based on what HLS expects
struct ConfigIn {
    // These are proof-of-concept fields
    
    // Memory addresses (3 x 64-bit = 192 bits)
    uint64_t input_buffer_addr;     // bits 0-63
    uint64_t output_buffer_addr;    // bits 64-127
    uint64_t kv_cache_addr;         // bits 128-191
    
    // Configuration parameters (multiple 32-bit fields)
    uint32_t stride;                // bits 192-223
    uint32_t max_tokens;            // bits 224-255
    uint32_t batch_size;            // bits 256-287
    uint32_t sequence_length;       // bits 288-319
    
    // Model configuration
    uint32_t num_layers;            // bits 320-351
    uint32_t hidden_size;           // bits 352-383
    uint32_t num_heads;             // bits 384-415
    uint32_t vocab_size;            // bits 416-447
    
    // Task-specific (per-inference)
    uint32_t prompt_length;         // bits 448-479
    uint32_t task_id;               // bits 480-511
    uint32_t task_type;             // bits 512-543
    uint32_t flags;                 // bits 544-575
    
    // Reserved/padding to reach 1216 bits
    uint32_t reserved[20];          // bits 576-1215 (640 bits)
    
    ConfigIn() {
        memset(this, 0, sizeof(ConfigIn));
    }
    
    void pack(uint32_t words[38]) const {
        memcpy(words, this, 38 * sizeof(uint32_t));
    }
    
    void unpack(const uint32_t words[38]) {
        memcpy(this, words, 38 * sizeof(uint32_t));
    }
    
    static void setAddress(uint32_t words[38], int start_word, uint64_t addr) {
        words[start_word] = (uint32_t)(addr & 0xFFFFFFFF);
        words[start_word + 1] = (uint32_t)(addr >> 32);
    }
    
    static uint64_t getAddress(const uint32_t words[38], int start_word) {
        uint64_t low = words[start_word];
        uint64_t high = words[start_word + 1];
        return (high << 32) | low;
    }
};

// StatusOut: 128 bits = 4 x 32-bit words
struct StatusOut {
    //  todo adjust based on actual HLS design
    uint32_t current_token;         // bits 0-31
    uint32_t tokens_generated;      // bits 32-63
    uint32_t error_code;            // bits 64-95
    uint32_t flags;                 // bits 96-127
    
    bool isValid() const { return (flags & 0x01) != 0; }
    bool isDone() const { return (flags & 0x02) != 0; }
    bool hasError() const { return (flags & 0x04) != 0; }
    
    StatusOut() : current_token(0), tokens_generated(0), 
                  error_code(0), flags(0) {}
    
    void unpack(const uint32_t words[4]) {
        current_token = words[0];
        tokens_generated = words[1];
        error_code = words[2];
        flags = words[3];
    }
    
    // Pack to words (for simulation)
    void pack_to_words() {
        // Helper for simulation
    }
};

#endif // CONFIG_STRUCT_HPP
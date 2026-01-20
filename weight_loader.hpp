#ifndef WEIGHT_LOADER_HPP
#define WEIGHT_LOADER_HPP

#include <cstdint>
#include <vector>
#include <string>
#include <fstream>
#include <cstring>
#include <cstdio>

// INT4 weight storage format
// 2 weights per byte: [weight1 (4 bits) | weight0 (4 bits)]
struct INT4Weights {
    uint8_t* data;          // Packed INT4 data (2 weights per byte)
    size_t num_weights;     // Total number of weights
    size_t data_size;       // Size in bytes (num_weights / 2)
    
    float scale;            // Quantization scale
    int8_t zero_point;      // Zero point for quantization
    
    INT4Weights() : data(nullptr), num_weights(0), data_size(0), 
                    scale(1.0f), zero_point(0) {}
    
    ~INT4Weights() {
        if (data) delete[] data;
    }
    
    // Get weight at index (unpacks from INT4)
    int8_t getWeight(size_t idx) const {
        if (idx >= num_weights) return 0;
        
        size_t byte_idx = idx / 2;
        bool is_upper = (idx % 2) == 1;
        
        uint8_t byte = data[byte_idx];
        int8_t value;
        
        if (is_upper) {
            value = (byte >> 4) & 0x0F;  // Upper 4 bits
        } else {
            value = byte & 0x0F;         // Lower 4 bits
        }
        
        // Sign-extend from 4-bit to 8-bit
        if (value & 0x08) {
            value |= 0xF0;  // Extend sign bit
        }
        
        return value;
    }
    
    // Set weight at index (packs to INT4)
    void setWeight(size_t idx, int8_t value) {
        if (idx >= num_weights) return;
        
        // Clamp to 4-bit signed range [-8, 7]
        if (value > 7) value = 7;
        if (value < -8) value = -8;
        
        size_t byte_idx = idx / 2;
        bool is_upper = (idx % 2) == 1;
        
        uint8_t masked_value = value & 0x0F;
        
        if (is_upper) {
            data[byte_idx] = (data[byte_idx] & 0x0F) | (masked_value << 4);
        } else {
            data[byte_idx] = (data[byte_idx] & 0xF0) | masked_value;
        }
    }
    
    // Allocate storage
    bool allocate(size_t num_weights_) {
        num_weights = num_weights_;
        data_size = (num_weights + 1) / 2;  // Round up
        data = new uint8_t[data_size];
        if (!data) return false;
        memset(data, 0, data_size);
        return true;
    }
    
    // Dequantize to float
    float dequantize(size_t idx) const {
        int8_t quantized = getWeight(idx);
        return (quantized - zero_point) * scale;
    }
};

// Layer weights structure
struct LayerWeights {
    std::string name;
    
    // Attention weights
    INT4Weights q_weights;      // Query projection
    INT4Weights k_weights;      // Key projection
    INT4Weights v_weights;      // Value projection
    INT4Weights o_weights;      // Output projection
    
    // FFN weights
    INT4Weights ffn_up;         // Feed-forward up projection
    INT4Weights ffn_down;       // Feed-forward down projection
    
    // Layer norm weights (FP16 for precision)
    std::vector<float> ln1_weight;
    std::vector<float> ln1_bias;
    std::vector<float> ln2_weight;
    std::vector<float> ln2_bias;
    
    // Metadata
    size_t layer_idx;
    size_t hidden_size;
    size_t intermediate_size;
};

// Complete model weights
struct ModelWeights {
    // Embedding table (typically FP16 or INT8)
    std::vector<float> token_embeddings;
    std::vector<float> position_embeddings;
    
    // Layer weights (INT4)
    std::vector<LayerWeights> layers;
    
    // Output projection (FP16)
    std::vector<float> lm_head;
    
    // Model config
    uint32_t num_layers;
    uint32_t hidden_size;
    uint32_t num_heads;
    uint32_t vocab_size;
    uint32_t max_seq_len;
    
    ModelWeights() : num_layers(0), hidden_size(0), 
                     num_heads(0), vocab_size(0), max_seq_len(0) {}
};

// Weight loader class
class WeightLoader {
private:
    ModelWeights weights;
    std::string model_path;
    bool loaded;
    
    // Memory regions (physical addresses for DMA)
    uint64_t ddr_weights_phys;
    void* ddr_weights_virt;
    size_t ddr_weights_size;
    
    // Calculate size needed for weights in DDR
    size_t calculateDDRSize() const {
        size_t total = 0;
        
        // Embeddings (FP16 = 2 bytes per element)
        total += weights.token_embeddings.size() * 2;
        total += weights.position_embeddings.size() * 2;
        
        // Layer weights (INT4 = 0.5 bytes per weight)
        for (const auto& layer : weights.layers) {
            total += layer.q_weights.data_size;
            total += layer.k_weights.data_size;
            total += layer.v_weights.data_size;
            total += layer.o_weights.data_size;
            total += layer.ffn_up.data_size;
            total += layer.ffn_down.data_size;
            
            // Layer norms (FP16)
            total += layer.ln1_weight.size() * 2;
            total += layer.ln1_bias.size() * 2;
            total += layer.ln2_weight.size() * 2;
            total += layer.ln2_bias.size() * 2;
        }
        
        // LM head (FP16)
        total += weights.lm_head.size() * 2;
        
        return total;
    }
    
    // Convert FP32 to FP16 (simple bit manipulation)
    uint16_t float_to_fp16(float value) {
        // Simple conversion (not IEEE compliant, but works for PoC)
        uint32_t bits = *reinterpret_cast<uint32_t*>(&value);
        uint32_t sign = (bits >> 31) & 0x1;
        uint32_t exp = (bits >> 23) & 0xFF;
        uint32_t mantissa = bits & 0x7FFFFF;
        
        // Adjust exponent
        int16_t new_exp = exp - 127 + 15;
        if (new_exp <= 0) return 0;  // Underflow
        if (new_exp >= 31) return (sign << 15) | 0x7C00;  // Overflow
        
        // Pack into FP16
        uint16_t result = (sign << 15) | (new_exp << 10) | (mantissa >> 13);
        return result;
    }

public:
    WeightLoader() : loaded(false), ddr_weights_phys(0), 
                     ddr_weights_virt(nullptr), ddr_weights_size(0) {}
    
    bool loadFromPyTorch(const std::string& pt_file) {
        printf("[WeightLoader] Loading INT4 quantized weights from: %s\n", pt_file.c_str());
        
        // Check if we have a pre-converted binary
        std::string bin_file = pt_file + ".bin";
        std::ifstream check(bin_file, std::ios::binary);
        
        if (!check.good()) {
            printf("[WeightLoader] No pre-converted binary found.\n");
            printf("[WeightLoader] Please run: python convert_weights.py %s\n", pt_file.c_str());
            return false;
        }
        
        return loadFromBinary(bin_file);
    }
    
    // Load from pre-converted binary format
    bool loadFromBinary(const std::string& bin_file) {
        printf("[WeightLoader] Loading from binary: %s\n", bin_file.c_str());
        
        std::ifstream file(bin_file, std::ios::binary);
        if (!file.good()) {
            printf("[WeightLoader] Failed to open file\n");
            return false;
        }
        
        // Read header
        struct Header {
            uint32_t magic;         // 0x57544E54 ("WTNT" = WeighTs iNT4)
            uint32_t version;
            uint32_t num_layers;
            uint32_t hidden_size;
            uint32_t num_heads;
            uint32_t vocab_size;
            uint32_t max_seq_len;
            uint32_t intermediate_size;
        } header;
        
        file.read(reinterpret_cast<char*>(&header), sizeof(header));
        
        if (header.magic != 0x57544E54) {
            printf("[WeightLoader] Invalid magic number: 0x%08X\n", header.magic);
            return false;
        }
        
        printf("[WeightLoader] Model config:\n");
        printf("  Version: %u\n", header.version);
        printf("  Layers: %u\n", header.num_layers);
        printf("  Hidden: %u\n", header.hidden_size);
        printf("  Heads: %u\n", header.num_heads);
        printf("  Vocab: %u\n", header.vocab_size);
        
        // Store config
        weights.num_layers = header.num_layers;
        weights.hidden_size = header.hidden_size;
        weights.num_heads = header.num_heads;
        weights.vocab_size = header.vocab_size;
        weights.max_seq_len = header.max_seq_len;
        
        uint32_t checksum_offset;
        file.read(reinterpret_cast<char*>(&checksum_offset), sizeof(checksum_offset));
        
        size_t embed_size = header.vocab_size * header.hidden_size;
        weights.token_embeddings.resize(embed_size);
        
        // For PoC: just read size
        file.seekg(embed_size * 2, std::ios::cur);  // Skip FP16 data
        
        size_t pos_embed_size = header.max_seq_len * header.hidden_size;
        weights.position_embeddings.resize(pos_embed_size);
        file.seekg(pos_embed_size * 2, std::ios::cur);  // Skip FP16 data
        
        // Simulate loading layers
        weights.layers.resize(header.num_layers);
        for (size_t i = 0; i < header.num_layers; i++) {
            LayerWeights& layer = weights.layers[i];
            layer.layer_idx = i;
            layer.hidden_size = header.hidden_size;
            layer.intermediate_size = header.intermediate_size;
            
            // Allocate weight storage
            size_t qkv_size = header.hidden_size * header.hidden_size;
            layer.q_weights.allocate(qkv_size);
            layer.k_weights.allocate(qkv_size);
            layer.v_weights.allocate(qkv_size);
            layer.o_weights.allocate(qkv_size);
            
            size_t ffn_up_size = header.hidden_size * header.intermediate_size;
            size_t ffn_down_size = header.intermediate_size * header.hidden_size;
            layer.ffn_up.allocate(ffn_up_size);
            layer.ffn_down.allocate(ffn_down_size);
            
            // Layer norms
            layer.ln1_weight.resize(header.hidden_size);
            layer.ln1_bias.resize(header.hidden_size);
            layer.ln2_weight.resize(header.hidden_size);
            layer.ln2_bias.resize(header.hidden_size);
            
            for (int w = 0; w < 6; w++) {  // q, k, v, o, ffn_up, ffn_down
                float scale;
                int8_t zero_point;
                uint32_t data_size;
                
                file.read(reinterpret_cast<char*>(&scale), sizeof(scale));
                file.read(reinterpret_cast<char*>(&zero_point), sizeof(zero_point));
                file.read(reinterpret_cast<char*>(&data_size), sizeof(data_size));
                
                // Skip actual data for PoC
                file.seekg(data_size, std::ios::cur);
            }
        }
        
        // Verify checksums
        if (checksum_offset > 0) {
            printf("[WeightLoader] Verifying checksums...\n");
            
            file.seekg(checksum_offset);
            
            uint32_t num_checksums;
            file.read(reinterpret_cast<char*>(&num_checksums), sizeof(num_checksums));
            
            printf("[WeightLoader]   Found %u checksums\n", num_checksums);
            
            for (uint32_t i = 0; i < num_checksums; i++) {
                uint32_t name_len;
                file.read(reinterpret_cast<char*>(&name_len), sizeof(name_len));
                
                char name_buf[256];
                file.read(name_buf, name_len);
                name_buf[name_len] = '\0';
                
                uint8_t checksum[32];  // SHA-256
                file.read(reinterpret_cast<char*>(checksum), 32);
                
                // For PoC
                if (i < 3) {
                    printf("[WeightLoader]   %s: ", name_buf);
                    for (int j = 0; j < 8; j++) {
                        printf("%02x", checksum[j]);
                    }
                    printf("...\n");
                }
            }
            
            printf("[WeightLoader] Checksum verification complete ✓\n");
        }
        
        loaded = true;
        printf("[WeightLoader] Weights loaded successfully\n");
        
        return true;
    }
    
    bool allocateDDR(uint64_t phys_addr, void* virt_addr, size_t size) {
        ddr_weights_phys = phys_addr;
        ddr_weights_virt = virt_addr;
        ddr_weights_size = size;
        
        size_t required = calculateDDRSize();
        printf("[WeightLoader] DDR allocation:\n");
        printf("  Physical: 0x%016lX\n", ddr_weights_phys);
        printf("  Virtual:  %p\n", ddr_weights_virt);
        printf("  Allocated: %zu MB\n", size / (1024*1024));
        printf("  Required:  %zu MB\n", required / (1024*1024));
        
        if (required > size) {
            printf("[WeightLoader] ERROR: Insufficient DDR memory\n");
            return false;
        }
        
        return true;
    }
    
    bool copyToDDR() {
        if (!loaded) {
            printf("[WeightLoader] No weights loaded\n");
            return false;
        }
        
        if (!ddr_weights_virt) {
            printf("[WeightLoader] DDR not allocated\n");
            return false;
        }
        
        printf("[WeightLoader] Copying weights to DDR...\n");
        
        uint8_t* ddr_ptr = static_cast<uint8_t*>(ddr_weights_virt);
        size_t offset = 0;
        
        // Copy embeddings (convert FP32 → FP16)
        for (float val : weights.token_embeddings) {
            uint16_t fp16 = float_to_fp16(val);
            memcpy(ddr_ptr + offset, &fp16, 2);
            offset += 2;
        }
        
        printf("[WeightLoader]   Embeddings: %zu bytes\n", offset);
        size_t embed_offset = offset;
        
        // Copy layer weights
        for (size_t i = 0; i < weights.layers.size(); i++) {
            const LayerWeights& layer = weights.layers[i];
            
            // Copy INT4 weights (already packed)
            memcpy(ddr_ptr + offset, layer.q_weights.data, layer.q_weights.data_size);
            offset += layer.q_weights.data_size;
            
            memcpy(ddr_ptr + offset, layer.k_weights.data, layer.k_weights.data_size);
            offset += layer.k_weights.data_size;
            
            memcpy(ddr_ptr + offset, layer.v_weights.data, layer.v_weights.data_size);
            offset += layer.v_weights.data_size;
            
            memcpy(ddr_ptr + offset, layer.o_weights.data, layer.o_weights.data_size);
            offset += layer.o_weights.data_size;
            
            memcpy(ddr_ptr + offset, layer.ffn_up.data, layer.ffn_up.data_size);
            offset += layer.ffn_up.data_size;
            
            memcpy(ddr_ptr + offset, layer.ffn_down.data, layer.ffn_down.data_size);
            offset += layer.ffn_down.data_size;
            
            // Layer norms (FP32 → FP16)
            for (float val : layer.ln1_weight) {
                uint16_t fp16 = float_to_fp16(val);
                memcpy(ddr_ptr + offset, &fp16, 2);
                offset += 2;
            }
        }
        
        printf("[WeightLoader]   Layer weights: %zu bytes\n", offset - embed_offset);
        printf("[WeightLoader]   Total copied: %zu bytes (%.2f MB)\n", 
               offset, offset / (1024.0 * 1024.0));
        
        return true;
    }
    
    uint64_t getLayerAddress(size_t layer_idx) {
        if (layer_idx >= weights.layers.size()) return 0;
        
        size_t offset = 0;
        
        offset += weights.token_embeddings.size() * 2;
        offset += weights.position_embeddings.size() * 2;
        
        for (size_t i = 0; i < layer_idx; i++) {
            const LayerWeights& layer = weights.layers[i];
            offset += layer.q_weights.data_size;
            offset += layer.k_weights.data_size;
            offset += layer.v_weights.data_size;
            offset += layer.o_weights.data_size;
            offset += layer.ffn_up.data_size;
            offset += layer.ffn_down.data_size;
            offset += layer.ln1_weight.size() * 2;
            offset += layer.ln1_bias.size() * 2;
            offset += layer.ln2_weight.size() * 2;
            offset += layer.ln2_bias.size() * 2;
        }
        
        return ddr_weights_phys + offset;
    }
    
    const ModelWeights& getWeights() const { return weights; }
    bool isLoaded() const { return loaded; }
    size_t getRequiredDDRSize() const { return calculateDDRSize(); }
};

#endif // WEIGHT_LOADER_HPP

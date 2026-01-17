// inference_engine.cpp
// Main inference engine implementation with weight loading

#include "types.hpp"
#include "queue.hpp"
#include "accelerator.hpp"
#include "weight_loader.hpp"
#include "memory_manager.hpp"
#include <iostream>
#include <thread>
#include <atomic>
#include <chrono>
#include <vector>

//TODO global queue vs mutex 
Queue<Task, 100> taskQueue;
Queue<Command, 10> commandQueue;

std::vector<uint32_t> tokenize(const std::string& text) {
    std::vector<uint32_t> tokens;
    for (char c : text) {
        tokens.push_back((uint32_t)c);
    }
    return tokens;
}

std::string detokenize(uint32_t token) {
    if (token < 128) {
        return std::string(1, (char)token);
    }
    return "[T" + std::to_string(token) + "]";
}

void sendOutputToUI(const std::string& text) {
    std::cout << text << std::flush;
}

void clearKvCache(Accelerator& accel) {
    accel.reset();
}

void handleTopLevelCommand(const Command& cmd, EngineState& state, Accelerator& accel) {
    switch (cmd.type) {
        case CommandType::SHUTDOWN:
            state.status = EngineStatus::SHUTTING_DOWN;
            break;
            
        case CommandType::RESET:
            clearKvCache(accel);
            sendOutputToUI("\n[Memory cleared]\n");
            break;
            
        case CommandType::STOP_CURRENT:
            // Nothing to stop when idle
            break;
    }
}

void runGeneration(const Task& task, EngineState& state, Accelerator& accel) {
    state.cancelCurrent = false;
    state.resetRequested = false;
    
    std::vector<uint32_t> promptTokens = tokenize(task.prompt);
    
    sendOutputToUI("\n[Generating] ");
    
    accel.startInference(task.id, promptTokens);
    
    int token_count = 0;
    const int MAX_TOKENS = 50; // todo determine limit
    
    while (token_count < MAX_TOKENS) {

        Command cmd;
        if (commandQueue.tryPop(cmd)) {
            switch (cmd.type) {
                case CommandType::SHUTDOWN:
                    state.cancelCurrent = true;
                    state.status = EngineStatus::SHUTTING_DOWN;
                    sendOutputToUI("\n[Aborted: shutdown requested]\n");
                    return;
                    
                case CommandType::RESET:
                    state.cancelCurrent = true;
                    state.resetRequested = true;
                    break;
                    
                case CommandType::STOP_CURRENT:
                    state.cancelCurrent = true;
                    break;
            }
        }
        
        if (state.cancelCurrent) {
            sendOutputToUI("\n[Aborted]\n");
            
            if (state.resetRequested) {
                clearKvCache(accel);
                sendOutputToUI("[Memory cleared]\n");
                state.resetRequested = false;
            }
            return;
        }
        
        uint32_t nextToken;
        if (accel.getNextToken(nextToken)) {
            if (nextToken == EOS_TOKEN) {
                sendOutputToUI("\n[EOS]\n");
                return;
            }
            
            sendOutputToUI(detokenize(nextToken));
            token_count++;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(50)); //todo tune processing time
    }
    
    sendOutputToUI("\n[Max tokens reached]\n");
}

void inferenceEngineThread() {
    EngineState state;
    Accelerator accel;
    
    //todo determine correct mem address
    uint64_t input_addr = 0x10000000;
    uint64_t output_addr = 0x20000000;
    uint64_t kv_cache_addr = 0x30000000;
    accel.configure(input_addr, output_addr, kv_cache_addr, 128, 2048);
    
    std::cout << "[Engine] Inference engine started\n";
    
    while (state.status != EngineStatus::SHUTTING_DOWN) {
        if (state.status == EngineStatus::IDLE) {

            Command cmd;
            if (commandQueue.tryPop(cmd)) {
                handleTopLevelCommand(cmd, state, accel);
                continue;
            }
            
            Task task;
            if (taskQueue.tryPop(task)) {
                state.currentTaskId = task.id;
                state.status = EngineStatus::GENERATING;
                
                runGeneration(task, state, accel);
                
                if (state.status == EngineStatus::GENERATING) {
                    state.status = EngineStatus::IDLE;
                    state.currentTaskId = -1;
                }
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        } else if (state.status == EngineStatus::GENERATING) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
    
    clearKvCache(accel);
    std::cout << "[Engine] Shutdown complete\n";
}

bool pushTask(const Task& task) {
    if (taskQueue.full()) {
        std::cout << "[Warning] Task queue full, dropping request\n";
        return false;
    }
    return taskQueue.push(task);
}

int main() {
    std::cout << "=================================================\n";
    std::cout << "FPGA Inference Engine - with Weight Loading\n";
    std::cout << "=================================================\n";
    std::cout << "Commands:\n";
    std::cout << "  /quit   - Shutdown engine\n";
    std::cout << "  /stop   - Stop current generation\n";
    std::cout << "  /reset  - Clear KV cache\n";
    std::cout << "  <text>  - Generate response\n";
    std::cout << "=================================================\n\n";
    
    std::cout << "Phase 1: Memory Initialization\n";
    std::cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n";
    
    MemoryManager memory;
    if (!memory.init()) {
        std::cerr << "Failed to initialize memory manager\n";
        return 1;
    }
    
    size_t weight_size = 1024 * 1024 * 1024;  // 1GB for weights
    size_t kv_cache_size = 512 * 1024 * 1024;  // 512MB for KV cache
    size_t input_size = 16 * 1024;   // 16KB input buffer
    size_t output_size = 16 * 1024;  // 16KB output buffer
    
    if (!memory.allocateWeights(weight_size)) {
        std::cerr << "Failed to allocate weight memory\n";
        return 1;
    }
    
    if (!memory.allocateKVCache(kv_cache_size)) {
        std::cerr << "Failed to allocate KV cache\n";
        return 1;
    }
    
    if (!memory.allocateIOBuffers(input_size, output_size)) {
        std::cerr << "Failed to allocate I/O buffers\n";
        return 1;
    }
    
    memory.printMemoryMap();
    
    std::cout << "Phase 2: Weight Loading\n";
    std::cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n";
    
    WeightLoader weightLoader;
    
    const char* model_file = "model.pt.bin";
    
    if (!weightLoader.loadFromBinary(model_file)) {
        std::cout << "\nNo model weights found. To load weights:\n";
        std::cout << "  1. Get INT4 quantized PyTorch model (model.pt)\n";
        std::cout << "  2. Run: python convert_weights.py model.pt model.pt.bin\n";
        std::cout << "  3. Place model.pt.bin in current directory\n";
        std::cout << "\nContinuing without weights (simulation mode)...\n\n";
    } else {
        if (!weightLoader.allocateDDR(
                memory.getWeightsPhysAddr(),
                memory.getWeightsVirtAddr(),
                memory.getWeightsSize())) {
            std::cerr << "Failed to allocate DDR for weights\n";
            return 1;
        }
        
        if (!weightLoader.copyToDDR()) {
            std::cerr << "Failed to copy weights to DDR\n";
            return 1;
        }
        
        std::cout << "Weights loaded successfully!\n\n";
    }
    
    std::cout << "Phase 3: Accelerator Configuration\n";
    std::cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n";
    
    std::thread engineThread(inferenceEngineThread);
    
    int nextTaskId = 1;
    std::string userInput;
    
    std::cout << "\nSystem ready for inference!\n";
    
    while (true) {
        std::cout << "\n> ";
        std::getline(std::cin, userInput);
        
        if (userInput.empty()) {
            continue;
        }
        
        if (userInput == "/quit") {
            commandQueue.push(Command(CommandType::SHUTDOWN));
            break;
        } else if (userInput == "/stop") {
            commandQueue.push(Command(CommandType::STOP_CURRENT));
        } else if (userInput == "/reset") {
            commandQueue.push(Command(CommandType::RESET));
        } else {
            Task task(nextTaskId++, TaskType::GENERATE, userInput);
            pushTask(task);
        }
    }
    
    engineThread.join();
    
    memory.cleanup();
    
    std::cout << "\n[Main] Application shutdown\n";
    return 0;
}

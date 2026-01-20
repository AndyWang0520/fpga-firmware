// types.hpp

#ifndef TYPES_HPP
#define TYPES_HPP

#include <cstdint>
#include <string>

enum class TaskType {
    GENERATE
};

struct Task {
    int id;
    TaskType type;
    std::string prompt;
    
    Task() : id(0), type(TaskType::GENERATE), prompt("") {}
    Task(int _id, TaskType _type, const std::string& _prompt) 
        : id(_id), type(_type), prompt(_prompt) {}
};

enum class CommandType {
    STOP_CURRENT,   
    RESET,         
    SHUTDOWN        
};

struct Command {
    CommandType type;
    
    Command() : type(CommandType::STOP_CURRENT) {}
    explicit Command(CommandType _type) : type(_type) {}
};

enum class EngineStatus {
    IDLE,
    GENERATING,
    SHUTTING_DOWN
};

struct EngineState {
    EngineStatus status;
    int currentTaskId;      // -1 means null
    bool cancelCurrent;
    bool resetRequested;
    
    EngineState() : status(EngineStatus::IDLE), 
                    currentTaskId(-1),
                    cancelCurrent(false),
                    resetRequested(false) {}
};

const uint32_t EOS_TOKEN = 0xFFFFFFFF;

#endif // TYPES_HPP

// queue.hpp

#ifndef QUEUE_HPP
#define QUEUE_HPP

#include <cstddef>
#include <cstring>

template<typename T, size_t MAX_SIZE = 100>
class Queue {
private:
    T buffer[MAX_SIZE];
    size_t head;
    size_t tail;
    size_t count;

public:
    Queue() : head(0), tail(0), count(0) {}

    bool push(const T& item) {
        if (count >= MAX_SIZE) {
            return false; // Queue full
        }
        buffer[tail] = item;
        tail = (tail + 1) % MAX_SIZE;
        count++;
        return true;
    }

    bool pop(T& item) {
        if (count == 0) {
            return false; // Queue empty
        }
        item = buffer[head];
        head = (head + 1) % MAX_SIZE;
        count--;
        return true;
    }

    bool tryPop(T& item) {
        return pop(item);
    }

    size_t size() const {
        return count;
    }

    bool empty() const {
        return count == 0;
    }

    bool full() const {
        return count >= MAX_SIZE;
    }
};

#endif // QUEUE_HPP

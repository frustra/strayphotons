#include "BufferedSocket.hh"

#include <Common.hh>

namespace sp {
namespace network {
    BufferedSocketInput::BufferedSocketInput(kissnet::tcp_socket &socket) : socket(socket) {
        socket.set_non_blocking(true);

        // Allocate some buffers for the pool
        pool.resize(DEFAULT_BUFFER_POOL_SIZE);
        buffer.emplace_back(allocateBlock());
    }

    bool BufferedSocketInput::Next(const void** data, int* size) {
        Assert(data != nullptr && size != nullptr, "BufferedSocketInput::Next data and size cannot be null");

        consume();
        if (readOffset < writeOffset) {
            size_t blockIndex = readOffset / BUFFER_POOL_BLOCK_SIZE;
            size_t blockOffset = readOffset - (blockIndex * BUFFER_POOL_BLOCK_SIZE);
            size_t maxSize = BUFFER_POOL_BLOCK_SIZE - blockOffset;
            Assert(blockIndex < buffer.size(), "BufferedSocketInput::Next reading past end of buffer");

            if (*size > maxSize) {
                *size = maxSize;
            }
            *data = pool[buffer[blockIndex]].second.data() + blockOffset;
            return true;
        } else {
            return !endOfStream;
        }
    }

    void BufferedSocketInput::BackUp(int count) {
        Assert(count < readOffset, "BufferedSocketInput Can't backup past previous operation");

        readOffset -= count;
    }

    bool BufferedSocketInput::Skip(int count) {
        Assert(count >= 0, "BufferedSocketInput can't Skip backwards");

        consume();
        if (endOfStream && readOffset + count > writeOffset) {
            readOffset = writeOffset;
            return false;
        } else {
            readOffset += count;
            return true;
        }
    }

    int64_t BufferedSocketInput::ByteCount() const {
        return bytesConsumed + readOffset;
    }

    void BufferedSocketInput::consume() {
        // Return blocks in the buffer before readOffset to the pool.
        while (readOffset >= BUFFER_POOL_BLOCK_SIZE) {
            freeBlock(buffer.front());
            buffer.pop_front();
            readOffset -= BUFFER_POOL_BLOCK_SIZE;
            bytesConsumed += BUFFER_POOL_BLOCK_SIZE;
        }

        // Make space for another socket read into the buffer.
        size_t blockIndex = writeOffset / BUFFER_POOL_BLOCK_SIZE;
        while (blockIndex >= buffer.size()) {
            buffer.emplace_back(allocateBlock());
        }
    
        // Perform a non-blocking read on the network socket.
        size_t blockOffset = writeOffset - (blockIndex * BUFFER_POOL_BLOCK_SIZE);
        auto [size, valid] = socket.recv(pool[buffer[blockIndex]].second, blockOffset);
        if (valid) {
            if (valid.value == kissnet::socket_status::cleanly_disconnected) {
                endOfStream = true;
            }
            writeOffset += size;
        } else {
            // Socket error
            endOfStream = true;
        }
    }

    size_t BufferedSocketInput::allocateBlock() {
        for (size_t i = 0; i < pool.size(); i++) {
            if (!pool[i].first) {
                pool[i].first = true;
                return i;
            }
        }
        pool.emplace_back();
        return pool.size() - 1;
    }

    void BufferedSocketInput::freeBlock(size_t index) {
        if (index < pool.size()) {
            pool[index].first = false;
        }
    }
} // namespace network
} // namespace sp

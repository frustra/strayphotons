#include "BufferedSocket.hh"

#include <Common.hh>

namespace sp {
    namespace network {
        BufferedStream::BufferedStream() {
            // Allocate some buffers for the pool
            pool.resize(DEFAULT_BUFFER_POOL_SIZE);
            buffer.emplace_back(allocateBlock());
        }

        bool BufferedStream::NextInput(std::byte **data, int *size) {
            Assert(data != nullptr && size != nullptr, "BufferedStream::NextInput data and size cannot be null");
            Assert(!endOfStream, "BufferedStream::NextInput writing to closed stream");

            advanceBuffer();

            // Make space for another socket read into the buffer.
            size_t blockIndex = inputOffset / BUFFER_POOL_BLOCK_SIZE;
            while (blockIndex >= buffer.size()) {
                buffer.emplace_back(allocateBlock());
            }

            size_t blockOffset = inputOffset - (blockIndex * BUFFER_POOL_BLOCK_SIZE);
            size_t maxSize = BUFFER_POOL_BLOCK_SIZE - blockOffset;
            Assert(blockIndex < buffer.size(), "BufferedStream::NextInput writing past end of buffer");

            *size = maxSize;
            *data = pool[buffer[blockIndex]].second.data() + blockOffset;
            inputOffset += *size;
            return true;
        }

        void BufferedStream::BackUpInput(int count) {
            Assert(inputOffset - count >= outputOffset, "BufferedStream Can't backup past already read output");

            inputOffset -= count;
        }

        void BufferedStream::CloseInput() {
            endOfStream = true;
        }

        int64_t BufferedStream::ByteCountInput() const {
            return bytesConsumed + inputOffset;
        }

        bool BufferedStream::NextOutput(const std::byte **data, int *size) {
            Assert(data != nullptr && size != nullptr, "BufferedStream::NextOutput data and size cannot be null");

            advanceBuffer();
            if (outputOffset < inputOffset) {
                size_t blockIndex = outputOffset / BUFFER_POOL_BLOCK_SIZE;
                size_t blockOffset = outputOffset - (blockIndex * BUFFER_POOL_BLOCK_SIZE);
                size_t maxSize = BUFFER_POOL_BLOCK_SIZE - blockOffset;
                Assert(blockIndex < buffer.size(), "BufferedStream::NextOutput reading past end of buffer");

                *size = inputOffset - outputOffset;
                if ((int)maxSize < *size) *size = maxSize;
                *data = pool[buffer[blockIndex]].second.data() + blockOffset;
                outputOffset += *size;
                return true;
            } else {
                *size = 0;
                return !endOfStream;
            }
        }

        void BufferedStream::BackUpOutput(int count) {
            Assert(count < outputOffset, "BufferedStream Can't backup past previous operation");

            outputOffset -= count;
        }

        bool BufferedStream::SkipOutput(int count) {
            Assert(count >= 0, "BufferedStream Can't skip backwards");

            advanceBuffer();
            if (endOfStream && outputOffset + count > inputOffset) {
                outputOffset = inputOffset;
                return false;
            } else {
                outputOffset += count;
                return true;
            }
        }

        int64_t BufferedStream::ByteCountOutput() const {
            return bytesConsumed + outputOffset;
        }

        void BufferedStream::advanceBuffer() {
            // Return blocks in the buffer before outputOffset to the pool.
            while (outputOffset >= (int64_t)BUFFER_POOL_BLOCK_SIZE) {
                freeBlock(buffer.front());
                buffer.pop_front();
                outputOffset -= BUFFER_POOL_BLOCK_SIZE;
                bytesConsumed += BUFFER_POOL_BLOCK_SIZE;
            }
        }

        size_t BufferedStream::allocateBlock() {
            for (size_t i = 0; i < pool.size(); i++) {
                if (!pool[i].first) {
                    pool[i].first = true;
                    return i;
                }
            }
            pool.emplace_back();
            return pool.size() - 1;
        }

        void BufferedStream::freeBlock(size_t index) {
            if (index < pool.size()) { pool[index].first = false; }
        }

        bool BufferedSocketInput::Next(const void **data, int *size) {
            // Perform a non-blocking read on the network socket.
            std::byte *buffer;
            int bufferSize;
            if (stream.NextInput(&buffer, &bufferSize) && bufferSize > 0) {
                auto [size, valid] = socket.recv(buffer, bufferSize, false);
                if (valid) {
                    if (valid.value == kissnet::socket_status::cleanly_disconnected) { stream.CloseInput(); }
                } else {
                    // Socket error
                    stream.CloseInput();
                }
            }

            return stream.NextOutput(reinterpret_cast<const std::byte **>(data), size);
        }

        bool BufferedSocketOutput::FlushBuffer() {
            // Perform a non-blocking write on the network socket.
            const std::byte *buffer;
            int bufferSize;
            if (stream.NextOutput(&buffer, &bufferSize)) {
                if (bufferSize <= 0) return true;

                auto [size, valid] = socket.send(buffer, bufferSize);
                if (valid) {
                    if (valid.value == kissnet::socket_status::cleanly_disconnected) { stream.CloseInput(); }
                    return true;
                } else {
                    // Socket error
                    stream.CloseInput();
                }
            }
            return false;
        }

        void BufferedSocketOutput::Close() {
            stream.CloseInput();
        }
    } // namespace network
} // namespace sp

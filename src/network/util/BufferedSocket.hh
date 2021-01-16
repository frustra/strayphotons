#pragma once

#include <deque>
#include <kissnet.hpp>
#include <google/protobuf/io/zero_copy_stream.h>

namespace sp {
namespace network {
    static constexpr size_t DEFAULT_BUFFER_POOL_SIZE = 3;
    static constexpr size_t BUFFER_POOL_BLOCK_SIZE = 4096;
    
    typedef kissnet::buffer<BUFFER_POOL_BLOCK_SIZE> PoolBlock;

    class BufferedSocketInput : public google::protobuf::io::ZeroCopyInputStream {
    public:
        BufferedSocketInput(kissnet::tcp_socket &socket);

        bool Next(const void** data, int* size) override;
        void BackUp(int count) override;
        bool Skip(int count) override;
        int64_t ByteCount() const override;
    private:
        void consume();
        size_t allocateBlock(); // Returns an index into the block pool
        void freeBlock(size_t index);

        kissnet::tcp_socket &socket;

        int64_t readOffset = 0;
        int64_t writeOffset = 0;
        int64_t bytesConsumed = 0;
        bool endOfStream = false;

        std::vector<std::pair<bool, PoolBlock>> pool; // Used flag, PoolBlock
        std::deque<size_t> buffer; // Index of Block in pool
    };
    
    class BufferedSocketOutput : public google::protobuf::io::ZeroCopyOutputStream {
    public:
        BufferedSocketOutput(kissnet::tcp_socket &socket);

        bool Next(void **data, int *size) override;
        void BackUp(int count) override;
        int64_t ByteCount() const override;

        bool WriteAliasedRaw(const void *data, int size) override {
            return false;
        }
        bool AllowsAliasing() const override {
            return false;
        }
    private:
        kissnet::tcp_socket &socket;
        
        int64_t writeOffset = 0;
        int64_t flushOffset = 0;
        int64_t bytesFlushed = 0;
        bool socketValid = false;

        std::vector<std::pair<bool, PoolBlock>> pool; // Used flag, PoolBlock
        std::deque<size_t> buffer; // Index of Block in pool
    };
} // namespace network
} // namespace sp

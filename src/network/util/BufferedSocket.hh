#pragma once

#include <deque>
#include <google/protobuf/io/zero_copy_stream.h>
#include <kissnet.hpp>

namespace sp {
    namespace network {
        class BufferedStream {
        public:
            BufferedStream();

            bool NextInput(std::byte **data, int *size);
            void BackUpInput(int count);
            void CloseInput();
            int64_t ByteCountInput() const;

            bool NextOutput(const std::byte **data, int *size);
            void BackUpOutput(int count);
            bool SkipOutput(int count);
            int64_t ByteCountOutput() const;

            static constexpr size_t DEFAULT_BUFFER_POOL_SIZE = 3;
            static constexpr size_t BUFFER_POOL_BLOCK_SIZE = 4096;

        private:
            typedef kissnet::buffer<BUFFER_POOL_BLOCK_SIZE> Block;

            // Free all blocks before the current output offset
            void advanceBuffer();
            // Returns an index into the block pool
            size_t allocateBlock();
            void freeBlock(size_t index);

            int64_t outputOffset = 0;
            int64_t inputOffset = 0;
            int64_t bytesConsumed = 0;
            bool endOfStream = false;

            std::vector<std::pair<bool, Block>> pool; // Used flag, PoolBlock
            std::deque<size_t> buffer; // Index of Block in pool
        };

        class BufferedSocketInput : public google::protobuf::io::ZeroCopyInputStream {
        public:
            BufferedSocketInput(kissnet::tcp_socket &socket) : socket(socket) {}

            bool Next(const void **data, int *size) override;

            void BackUp(int count) override {
                return stream.BackUpOutput(count);
            }

            bool Skip(int count) override {
                return stream.SkipOutput(count);
            }

            int64_t ByteCount() const override {
                return stream.ByteCountOutput();
            }

        private:
            kissnet::tcp_socket &socket;
            BufferedStream stream;
        };

        class BufferedSocketOutput : public google::protobuf::io::ZeroCopyOutputStream {
        public:
            BufferedSocketOutput(kissnet::tcp_socket &socket) : socket(socket) {
                socket.set_non_blocking(true);
            }

            bool Next(void **data, int *size) override {
                return stream.NextInput(reinterpret_cast<std::byte **>(data), size);
            }

            void BackUp(int count) override {
                return stream.BackUpInput(count);
            }

            int64_t ByteCount() const override {
                return stream.ByteCountInput();
            }

            bool WriteAliasedRaw(const void *data, int size) override {
                return false;
            }

            bool AllowsAliasing() const override {
                return false;
            }

            bool FlushBuffer();

        private:
            kissnet::tcp_socket &socket;
            BufferedStream stream;
        };
    } // namespace network
} // namespace sp

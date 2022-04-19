#pragma once

#include <atomic>
#include <memory>
#include <vector>

namespace sp {
    template<typename T>
    class LockFreeAudioSet {
        using IndexVectorPtr = std::shared_ptr<std::vector<size_t>>;

    public:
        LockFreeAudioSet(size_t maxSize) {
            validIndexes.store(make_shared<std::vector<size_t>>());
            vec.reserve(maxSize);
        }

        const T &Get(size_t index) const {
            return vec[index];
        }

        T &Get(size_t index) {
            return vec[index];
        }

        /**
         * Returns a list of indexes that other threads are permitted to read and write.
         * The items or members of the items at those indexes need to be internally thread safe if more than one thread
         * will access them.
         */
        IndexVectorPtr GetValidIndexes() const {
            return validIndexes.load();
        }

        /**
         * Owner thread only. Returns a new index that can be accessed by only the owner thread until the index is made
         * valid.
         */
        size_t AllocateItem() {
            size_t index;
            if (freeIndexes.empty()) {
                index = vec.size();
                vec.resize(vec.size() + 1);
            } else {
                index = freeIndexes.back();
                freeIndexes.pop_back();
            }
            return index;
        }

        /**
         * Owner thread only. Queues an index to be made visible to other threads on the next call to UpdateIndexes.
         */
        void MakeItemValid(size_t index) {
            nextValidIndexSet.insert(index);
        }

        /**
         * Owner thread only. Queues an index to be made invisible to other threads on the next call to UpdateIndexes.
         * The index will be available for reuse in a subsequent call to AllocateItem once no threads are using it.
         */
        void FreeItem(size_t index) {
            nextValidIndexSet.erase(index);

            if (currentFrameIndex == -1) {
                // no indexes have been made valid yet, so we can immediately free
                freeIndexes.push_back(index);
            } else {
                // mark index to be freed after the current validIndexes has no references
                frames[currentFrameIndex].indexesToFree.push_back(index);
            }
        }

        /**
         * Owner thread only. Commits all buffered changes. Frees any indexes that are no longer in use by any thread.
         */
        void UpdateIndexes() {
            IndexVectorPtr nextValidIndexes;
            for (size_t i = 0; i < frames.size(); i++) {
                auto &frame = frames[i];
                if (frame.validIndexes.use_count() > 1) continue;

                if (!frame.indexesToFree.empty()) {
                    freeIndexes.insert(freeIndexes.end(), frame.indexesToFree.begin(), frame.indexesToFree.end());
                    frame.indexesToFree.clear();
                }

                if (!nextValidIndexes) {
                    currentFrameIndex = (int)i;
                    nextValidIndexes = frame.validIndexes;
                    nextValidIndexes->clear();
                }
            }

            if (!nextValidIndexes) {
                currentFrameIndex = (int)frames.size();
                nextValidIndexes = make_shared<std::vector<size_t>>();
                frames.emplace_back(Frame{nextValidIndexes, {}});
            }

            nextValidIndexes->reserve(nextValidIndexSet.size());
            nextValidIndexes->insert(nextValidIndexes->begin(), nextValidIndexSet.begin(), nextValidIndexSet.end());

            validIndexes.store(nextValidIndexes);
        }

    private:
        std::vector<T> vec;
        std::atomic<IndexVectorPtr> validIndexes;
        int currentFrameIndex = -1;

        std::vector<size_t> freeIndexes;
        std::set<size_t> nextValidIndexSet;

        struct Frame {
            IndexVectorPtr validIndexes;
            std::vector<size_t> indexesToFree;
        };
        std::vector<Frame> frames;
    };
} // namespace sp

/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <atomic>
#include <memory>
#include <set>
#include <vector>

namespace sp {
    template<typename T, size_t MaxSize>
    class LockFreeAudioSet {
        using IndexVector = std::vector<size_t>;
        using IndexVectorPtr = std::shared_ptr<IndexVector>;

    public:
        LockFreeAudioSet() {
            UpdateIndexes();
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
#ifdef __cpp_lib_atomic_shared_ptr
            return validIndexes.load();
#else
            return std::atomic_load(&validIndexes);
#endif
        }

        /**
         * Owner thread only. Returns a new index that can be accessed by only the owner thread until the index is made
         * valid.
         */
        size_t AllocateItem() {
            size_t index;
            if (freeIndexes.empty()) {
                index = vecSize++;
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
            frames[currentFrameIndex].indexesToFree.push_back(index);
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
                nextValidIndexes = make_shared<IndexVector>();
                frames.emplace_back(Frame{nextValidIndexes, {}});
            }

            nextValidIndexes->reserve(nextValidIndexSet.size());
            nextValidIndexes->insert(nextValidIndexes->begin(), nextValidIndexSet.begin(), nextValidIndexSet.end());

#ifdef __cpp_lib_atomic_shared_ptr
            validIndexes.store(nextValidIndexes);
#else
            std::atomic_store(&validIndexes, nextValidIndexes);
#endif
        }

    private:
        std::array<T, MaxSize> vec;
        size_t vecSize = 0;

#ifdef __cpp_lib_atomic_shared_ptr
        std::atomic<IndexVectorPtr> validIndexes;
#else
        IndexVectorPtr validIndexes; // must access with std::atomic_*
#endif
        int currentFrameIndex = -1;

        IndexVector freeIndexes;
        std::set<size_t> nextValidIndexSet;

        struct Frame {
            IndexVectorPtr validIndexes;
            IndexVector indexesToFree;
        };
        std::vector<Frame> frames;
    };
} // namespace sp

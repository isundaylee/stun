#pragma once

#include <common/Logger.h>
#include <common/Util.h>

#include <stddef.h>

#include <vector>

namespace common {

template <int BlockSize, int BlockCount> class MemoryPool {
public:
  ~MemoryPool() {
    for (auto pool : pools_) {
      free(pool);
    }
  }

  void* allocate(size_t size) {
    assertTrue(size <= BlockSize,
               "Requested allocation larger than BlockSize.");

    if (freeList_ == nullptr) {
      LOG_V("MemoryPool") << "Allocating a new pool." << std::endl;

      Byte* newPool = static_cast<Byte*>(malloc(BlockSize * BlockCount));
      assertTrue(newPool != nullptr, "Out of memory in MemoryPool.");
      pools_.push_back(newPool);

      for (size_t i = 0; i < BlockCount; i++) {
        this->free(newPool);
        newPool += BlockSize;
      }
    }

    assertTrue(freeList_ != nullptr, "How can freeList_ still be nullptr?");

    void* allocated = freeList_;
    freeList_ = freeList_->next;
    return allocated;
  }

  void free(void* block) {
    auto freeBlock = static_cast<FreeList*>(block);
    freeBlock->next = freeList_;
    freeList_ = freeBlock;
  }

private:
  struct FreeList {
    FreeList* next;
  };

  FreeList* freeList_ = nullptr;
  std::vector<void*> pools_;
};
}

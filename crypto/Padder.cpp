#include "crypto/Padder.h"

namespace crypto {

static const Byte kPadderPaddingByte = 0xFB;

typedef size_t PadderSizeType;

Padder::Padder(size_t minSize) : minSize_(minSize) {}

/* virtual */ size_t Padder::encrypt(Byte* data, size_t size,
                                     size_t capacity) /* override */ {
  size_t actualSize = std::max(minSize_, size) + sizeof(PadderSizeType);
  assertTrue(actualSize <= capacity,
             "Padder doesn't have enough space for the size header.");
  if (size < minSize_) {
    memset(data + size, kPadderPaddingByte, minSize_ - size);
  }
  PadderSizeType* sizeFooter =
      (PadderSizeType*)(data + std::max(minSize_, size));
  *sizeFooter = size;
  return actualSize;
}

/* virtual */ size_t Padder::decrypt(Byte* data, size_t size,
                                     size_t capacity) /* override */ {
  assertTrue(size >= sizeof(PadderSizeType),
             "Padder encountered an impossibly short packet.");
  PadderSizeType* sizeFooter =
      (PadderSizeType*)(data + (size - sizeof(PadderSizeType)));
  return *sizeFooter;
}
}

#include "crypto/LZOCompressor.h"

#include <algorithm>

namespace crypto {

LZOCompressor::LZOCompressor() { workMem_.resize(LZO1X_1_MEM_COMPRESS); }

/* virtual */ size_t LZOCompressor::encrypt(Byte* data, size_t size,
                                            size_t capacity) /* override */ {
  if (capacity > buffer_.capacity()) {
    buffer_.resize(capacity);
  }

  lzo_uint compressedSize = capacity;
  auto ret = lzo1x_1_compress(data, size, buffer_.data(), &compressedSize,
                              workMem_.data());
  assertTrue(ret == LZO_E_OK, "LZOCompressor compression failed.");

  memcpy(data, buffer_.data(), compressedSize);

  return compressedSize;
}

/* virtual */ size_t LZOCompressor::decrypt(Byte* data, size_t size,
                                            size_t capacity) /* override */ {
  if (capacity > buffer_.capacity()) {
    buffer_.resize(capacity);
  }

  lzo_uint decompressedSize = capacity;
  auto ret = lzo1x_decompress(data, size, buffer_.data(), &decompressedSize,
                              workMem_.data());
  assertTrue(ret == LZO_E_OK, "LZOCompressor decompression failed.");

  memcpy(data, buffer_.data(), decompressedSize);

  return decompressedSize;
}
} // namespace crypto

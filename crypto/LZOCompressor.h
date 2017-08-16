#pragma once

#include <minilzo/minilzo.h>

#include <crypto/Encryptor.h>

#include <common/Util.h>

#include <unistd.h>

namespace crypto {

class LZOCompressor : public Encryptor {
public:
  LZOCompressor();

  virtual size_t encrypt(Byte* data, size_t size, size_t capacity) override;
  virtual size_t decrypt(Byte* data, size_t size, size_t capacity) override;

private:
  std::vector<Byte> workMem_;
  std::vector<Byte> buffer_;
};
}

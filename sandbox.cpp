#include <crypto/AESEncryptor.h>

#include <fcntl.h>
#include <unistd.h>

#include <iostream>

int main(int argc, char* argv[]) {
  Byte key[16] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
  crypto::AESEncryptor enc(crypto::AESKey(key, 16));

  std::string text = "Hello, world! This is a simply cryptography check ;)";
  Byte data[128];
  std::copy(text.begin(), text.end(), data);
  size_t size = text.length();

  size = enc.encrypt(&data[0], size, 128);
  size = enc.decrypt(&data[0], size, 128);

  for (size_t i=0; i<size; i++) {
    std::cout << "0x" << std::hex << (int) data[i] << " ";
  }
  std::cout << std::endl;

  return 0;
}

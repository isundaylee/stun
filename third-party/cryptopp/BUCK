cxx_library(
  name = 'cryptopp',
  header_namespace = 'cryptopp',
  exported_headers = subdir_glob([
    ('', '*.h'),
  ]),
  srcs = glob([
    '*.cpp',
  ],
  excludes = [
    'test.cpp',
  ]),
  visibility = [
    'PUBLIC',
  ],
)

cxx_binary(
  name = 'test',
  srcs = [
    'test.cpp',
  ],
  deps = [
    ':cryptopp',
  ],
)

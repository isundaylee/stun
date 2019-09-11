with allow_unsafe_import():
    import time

genrule(
    name = 'metadata-header',
    srcs = ['generate-metadata-header.sh'],
    bash = './generate-metadata-header.sh $OUT; echo ' + str(time.time()),
    out = 'Metadata.h',
)

cxx_binary(
    name = 'main',
    srcs = ['main.cpp'],
    headers = [':metadata-header'],
    deps = [
        '//stun:stun',
        '//flutter:flutter',
        '//third-party:cxxopts',
    ],
    visibility = ['PUBLIC'],
)

cxx_binary(
    name = 'sandbox',
    srcs = ['sandbox.cpp'],
    deps = [
    ],
)

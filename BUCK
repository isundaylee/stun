cxx_binary(
    name = 'main',
    srcs = ['main.cpp'],
    deps = [
        '//stun:stun',
        '//flutter:flutter',
        '//third-party/cxxopts:cxxopts',
    ],
    visibility = ['PUBLIC'],
)

cxx_binary(
    name = 'sandbox',
    srcs = ['sandbox.cpp'],
    deps = [
        '//event:event',
        '//networking:networking',
    ],
)

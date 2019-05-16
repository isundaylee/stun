genrule(
    name = 'metadata-header',
    srcs = ['generate-metadata-header.sh'],
    cmd = './generate-metadata-header.sh $OUT',
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
        '//event:event',
        '//networking:networking',
    ],
)

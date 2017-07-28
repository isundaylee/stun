cxx_binary(
    name = 'main',
    srcs = ['main.cpp'],
    deps = [
        '//stun:stun',
    ],
    linker_flags = ['-static-libstdc++']
)

cxx_binary(
    name = 'sandbox',
    srcs = ['sandbox.cpp'],
    deps = [
        '//event:event',
        '//common:common',
        '//stats:stats',
        '//crypto:crypto',
    ],
)

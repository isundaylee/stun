cxx_binary(
    name = 'main',
    headers = subdir_glob([
        ('include', '*.h')
    ]),
    srcs = glob([
        'src/**/*.cpp',
    ], excludes = ['src/sandbox.cpp']),
    deps = [
        '//event:event',
    ],
)

cxx_binary(
    name = 'sandbox',
    headers = subdir_glob([
        ('include', '*.h')
    ]),
    srcs = glob(['src/**/*.cpp'], excludes = ['src/main.cpp']),
    deps = [
        '//libev:ev',
        '//event:event',
    ]
)

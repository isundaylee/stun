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
        '//common:common',
        '//networking:networking',
    ],
)

cxx_binary(
    name = 'sandbox',
    headers = subdir_glob([
        ('include', '*.h')
    ]),
    srcs = glob(['src/**/*.cpp'], excludes = ['src/main.cpp']),
    deps = [
        '//event:event',
        '//common:common',
        '//networking:networking',
    ]
)

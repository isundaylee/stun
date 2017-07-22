cxx_binary(
    name = 'main',
    headers = subdir_glob([
        ('include', '**/*.h')
    ]),
    srcs = glob([
        'src/**/*.cpp',
    ]),
    deps = [
        '//libev:ev'
    ],
)

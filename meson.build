project('squashfs-mount', ['c'],
        default_options : ['c_std=c99', 'default_library=shared'],
        version: files('VERSION'),
        meson_version: '>=0.57')

rootless = get_option('rootless')

# Look for libmount
dep_libmount = dependency('mount')

version = meson.project_version()

exec = executable('squashfs-mount',
                  sources: ['squashfs-mount.c', 'utils.c'],
                  dependencies: [dep_libmount],
                  c_args: ['-DSQFS_MOUNT_VERSION="@0@"'.format(version)],
                  # equivalent to: `chown root:root $exec; chmod u+s $exec;` before installation.
                  install_mode: ['rwsr-xr-x', 'root', 'root'],
                  install: true)

if rootless
  default_library = get_option('default_library')
  deps = []
  if default_library == 'static'
    sqfs_ll = dependency('squashfuse_ll', static: true)
    deps += sqfs_ll
    deps += dependency(get_option('fuse_version'), method: 'pkg-config', static: true)
    deps += dependency('liblz4', static: true)
    deps += dependency('libzstd', static: true)
    deps += dependency('liblzma', static: true)
    deps += dependency('lzo2', static: true)
    deps += dependency('zlib', static: true)
  else
    sqfs_ll = dependency('squashfuse_ll', static: false)
    deps += dependency(get_option('fuse_version'), method: 'pkg-config', static: false)
    deps += sqfs_ll
  endif

  executable('squashfs-mount-rootless',
             sources: ['squashfs-mount-rootless.c', 'rootless.c', 'utils.c'],
             dependencies: deps,
             c_args: ['-DSQFS_MOUNT_VERSION="@0@"'.format(version)],
             install_mode: [],
             install: true)
endif

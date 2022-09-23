project('squashfs-mount', ['c'], default_options : ['c_std=c99'], version: files('VERSION'), meson_version: '>=0.57')
version = meson.project_version()

# Look for libmount, squashfuse_ll
dep_libmount = dependency('mount')
dep_sqfs = dependency('squashfuse_ll')

# get fuse_version from options
# TODO parse FUSE_USE_VERSION in config.h from squashfuse instead
dep_fuse = dependency(get_option('fuse_version'), method: 'pkg-config')

if(get_option('suid'))
  install_mode =  ['rwsr-xr-x', 'root', 'root']
else
  install_mode = []
endif

exec = executable('squashfs-mount',
              sources: ['squashfs-mount.c', 'rootless.c'],
              dependencies: [dep_libmount, dep_sqfs, dep_fuse],
              c_args: ['-DVERSION="@0@"'.format(version)],
              # equivalent to: `chown root:root $exec; chmod u+s $exec;` before installation.
              install_mode: install_mode,
              install: true)

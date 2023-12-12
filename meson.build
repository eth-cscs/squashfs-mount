project('squashfs-mount', ['c'], default_options : ['c_std=c99'], version: files('VERSION'), meson_version: '>=0.57')

# Look for libmount
dep_libmount = dependency('mount')
dep_sqfs = dependency('squashfuse_ll')
dep_fuse = dependency('fuse')

version = meson.project_version()

exec = executable('squashfs-mount',
                  sources: ['squashfs-mount.c', 'utils.c'],
                  dependencies: [dep_libmount],
                  c_args: ['-DVERSION="@0@"'.format(version)],
                  # equivalent to: `chown root:root $exec; chmod u+s $exec;` before installation.
                  install_mode: ['rwsr-xr-x', 'root', 'root'],
                  install: true)

executable('squashfs-mount-rootless',
           sources: ['squashfs-mount-rootless.c', 'rootless.c', 'utils.c'],
           dependencies: [dep_libmount, dep_sqfs, dep_fuse],
           c_args: ['-DVERSION="@0@"'.format(version)],
           install_mode: [],
           install: true)

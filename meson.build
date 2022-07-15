project('squashfs-mount', ['c'], default_options : ['c_std=c99'], version: files('VERSION'), meson_version: '>=0.57')

# Look for libmount, squashfuse_ll and fuse
dep_libmount = dependency('mount')
dep_sqfs = dependency('squashfuse_ll')
dep_fuse = dependency('fuse')

version = meson.project_version()

exec = executable('squashfs-mount',
              sources: ['squashfs-mount.c', 'unprivileged.c'],
              dependencies: [dep_libmount, dep_sqfs, dep_fuse],
              c_args: ['-DVERSION="@0@"'.format(version)],
              # equivalent to: `chown root:root $exec; chmod u+s $exec;` before installation.
              install_mode: ['rwsr-xr-x', 'root', 'root'],
              install: true)

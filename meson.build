project('squashfs-mount', ['c'], default_options : ['c_std=c99'], version: files('VERSION'), meson_version: '>=0.57')

rootless = get_option('rootless')

# Look for libmount
dep_libmount = dependency('mount')

version = meson.project_version()

exec = executable('squashfs-mount',
                  sources: ['squashfs-mount.c', 'utils.c'],
                  dependencies: [dep_libmount],
                  c_args: ['-DVERSION="@0@"'.format(version)],
                  # equivalent to: `chown root:root $exec; chmod u+s $exec;` before installation.
                  install_mode: ['rwsr-xr-x', 'root', 'root'],
                  install: true)
if rootless
  static_linking = get_option('static')
  dep_sqfs = dependency('squashfuse_ll', static: static_linking)
  dep_fuse = dependency('fuse')
  executable('squashfs-mount-rootless',
             sources: ['squashfs-mount-rootless.c', 'rootless.c', 'utils.c'],
             dependencies: [dep_libmount, dep_sqfs, dep_fuse],
             c_args: ['-DVERSION="@0@"'.format(version)],
             install_mode: [],
             install: true)
endif

project('squashfs-mount', ['c'], default_options : ['c_std=c99'], version: files('VERSION'), meson_version: '>=0.60')

# Look for libmount
dep_libmount = dependency('mount')

version = meson.project_version()

exec = executable('squashfs-mount',
              sources: ['squashfs-mount.c'],
              dependencies: [dep_libmount],
              c_args: [f'-DVERSION="@version@"'],
              # equivalent to: `chown root:root $exec; chmod u+s $exec;` before installation.
              install_mode: ['rwsr-xr-x', 'root', 'root'],
              install: true)

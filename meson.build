project('squashfs-mount', ['c'], version: '0.1-dev', meson_version: '>=0.60')

# Look for libmount
dep_libmount = dependency('mount', version : '>=2.23')

exec = executable('squashfs-mount',
              sources: ['squashfs-mount.c'],
              dependencies: [dep_libmount],
              # equivalent to: `chown root:root $exec; chmod +s $exec;` before installation.
              install_mode: ['rwsr-sr-x', 'root', 'root'],
              install: true)

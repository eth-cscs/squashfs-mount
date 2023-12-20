# Copyright 2013-2023 Lawrence Livermore National Security, LLC and other
# Spack Project Developers. See the top-level COPYRIGHT file for details.
#
# SPDX-License-Identifier: (Apache-2.0 OR MIT)

from spack.package import *


class SquashfsMount(MesonPackage):
    """Squashfsmount is a small setuid binary which mounts squashfs-images in a
    mount namespace and executes the given command as the normal user."""

    homepage = "https://github.com/eth-cscs/squashfs-mount"
    url = "https://github.com/eth-cscs/squashfs-mount/archive/refs/tags/v0.5.0.tar.gz"
    git = "https://github.com/eth-cscs/squashfs-mount.git"


    license("BSD-3-Clause")

    maintainers("simonpintarelli")

    version("master", branch="master")
    version("0.5.0", sha256="37f36717c7bba332a988dd8b5285de849e31aa2a468d75967bd86192300e9be8")

    variant("rootless", default=True, description="enable squashfuse version")

    depends_on("util-linux", type="link")

    with when("+rootless"):
        depends_on("libfuse")
        depends_on("squashfuse")

    conflicts("+rootless", when="@:0.6")


    def meson_args(self):
        args = []
        if "+rootless" in self.spec:
           args += ["-Drootless=true"]
           if "^libfuse@:2.9.99" in self.spec:
               args += ["-Dfuse_version=fuse"]
           else:
               args += ["-Dfuse_version=fuse3"]
        return args

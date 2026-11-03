##############################################################################
# Copyright (c) 2016-26, Lawrence Livermore National Security, LLC and Umpire
# project contributors. See the COPYRIGHT file for details.
#
# SPDX-License-Identifier: (MIT)
##############################################################################
from spack.package import *
from spack_repo.builtin.build_systems.cached_cmake import cmake_cache_path
from spack_repo.builtin.packages.umpire.package import Umpire as BuiltinUmpire


class Umpire(BuiltinUmpire):
    """Override of the builtin umpire package adding the spdlog dependency
    introduced on develop (Umpire uses spdlog for logging when
    UMPIRE_ENABLE_LOGGING is on, which is the default). Remove once the
    spack-packages commit pinned in radiuss-spack-configs carries these
    changes."""

    # Matches the src/tpl/umpire/spdlog submodule pin (v1.17.0). spdlog 1.17
    # requires fmt@12, consistent with the existing fmt@12.1.0 requirement.
    depends_on("spdlog@1.17.0:", when="@2026:")

    def initconfig_package_entries(self):
        entries = super().initconfig_package_entries()

        if self.spec.satisfies("^spdlog"):
            entries.append(cmake_cache_path("spdlog_DIR", self.spec["spdlog"].prefix))

        return entries

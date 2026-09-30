##############################################################################
# Copyright (c) 2016-26, Lawrence Livermore National Security, LLC and Umpire
# project contributors. See the COPYRIGHT file for details.
#
# SPDX-License-Identifier: (MIT)
##############################################################################
from spack.package import *
from spack_repo.builtin.packages.spdlog.package import Spdlog as BuiltinSpdlog


class Spdlog(BuiltinSpdlog):
    """Override of the builtin spdlog package that adds the v1.17.0 release,
    matching Umpire's src/tpl/umpire/spdlog submodule pin. Remove once the
    spack-packages commit pinned in radiuss-spack-configs includes 1.17.0."""

    version("1.17.0", sha256="d8862955c6d74e5846b3f580b1605d2428b11d97a410d86e2fb13e857cd3a744")

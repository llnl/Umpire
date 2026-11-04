#############################################################################################
# Copyright (c) Lawrence Livermore National Security LLC. See the COPYRIGHT file for details.
#
# SPDX-License-Identifier: (MIT)
#############################################################################################

ifeq ($(DEBUG),1)
	DebugArgs=--progress plain
else
	DebugArgs=
endif

targets = gcc clang umap_build asan cuda hip sycl intel 

$(targets):
	DOCKER_BUILDKIT=1 docker build --target $@ --no-cache $(DebugArgs) .

style:
	scripts/docker/apply-style.sh

help:
	@echo 'usage: make [variable] [target]'
	@echo ''
	@echo 'Build Umpire using Docker!'
	@echo ''
	@echo 'target:'
	@echo '    asan                           build with clang sanitizer'
	@echo '    gccN                           build with GCC N'
	@echo '    clangN                         build with clang N'
	@echo '    nvccN                          build with CUDA N'
	@echo '    hip                            build with HIP'
	@echo '    hip.debug                      build image for building HIP (attach with docker run -it <image> /bin/bash)'
	@echo '    umap_build                     build umpire with umap (build-only test is currently only option)'
	@echo ''
	@echo 'variable:'
	@echo '    DEBUG                          display all output if set to 1'
	@echo ''
	@echo 'For example'
	@echo ''
	@echo '    make DEBUG=1 nvcc11'
	@echo ''
	@echo 'builds with the nvcc Docker image for nvcc 11.1.1 and displays all output.'

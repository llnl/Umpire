//////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2016-26, Lawrence Livermore National Security, LLC and Umpire
// project contributors. See the COPYRIGHT file for details.
//
// SPDX-License-Identifier: (MIT)
//////////////////////////////////////////////////////////////////////////////
#include <stdio.h>

#include "umpire/interface/c_fortran/umpire.h"

#define SIZE 1024

int main()
{
  /* _sphinx_tag_tut_pool_create_start */
  umpire_resourcemanager rm;
  umpire_resourcemanager_get_instance(&rm);

  umpire_allocator allocator;
  umpire_resourcemanager_get_allocator_by_name(&rm, "HOST", &allocator);

  umpire_allocator pool;
  umpire_resourcemanager_make_allocator_quick_pool(&rm, "pool", allocator, 1024*512, 512, &pool);
  /* _sphinx_tag_tut_pool_create_end */

  /* _sphinx_tag_tut_allocate_start */
  double* data = (double*) umpire_allocator_allocate(&pool, SIZE*sizeof(double));

  printf("Allocated %lu bytes using the %s allocator...", 
      (SIZE*sizeof(double)), umpire_allocator_get_name(&pool));

  umpire_allocator_deallocate(&pool, data);
  /* _sphinx_tag_tut_allocate_end */

  printf("deallocated.\n");

  return 0;
}

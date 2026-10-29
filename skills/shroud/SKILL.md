---
name: shroud
description: Workflow guidance for working with Umpire's Fortran interface with shroud, including safe editing steps and common shroud pitfalls.
---

# Shroud Workflow

Use this skill when you need to edit the Fortran interface in Umpire. Keep the generated sources consistent. Umpire uses Shroud to generate the Fortran interface. You can learn more about Shroud at `https://shroud.readthedocs.io/en/latest/`.

## Mental Model (important)

- `src/umpire/interface` is the location of Umpire's Fortran code.
- `src/umpire/interface/umpire_shroud.yaml` is the main file that describes how Umpire uses Shroud.
  - The generated Fortran from `umpire_shroud` will overwrite any `src/umpire/interface/**.f` or `src/umpire/interface/**.cpp` files.

After the `umpire_shroud.yaml` file is processed, the resulting .f and .cpp files are the ones that get compiled.

## Safe Edit Procedure

1. Make necessary edits to `src/umpire/interface/umpire_shroud.yaml`
2. Rebuild the code making sure to enable Fortran in the cmake configuration with `ENABLE_FORTRAN=On`
3. Verify the results by building the code and running `make test`

## Common pitfalls 

- **Editing the wrong file**: if you edit `*.f` or `*.cpp` under `src/umpire/interface/`, you're editing the transformed source; changes will be overwritten by a github action which runs any time the umpire_shroud.yaml is edited to regenerate the fortran code.

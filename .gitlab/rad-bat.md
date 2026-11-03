# Maintaining host-configs with `rad-bat`

Every maintained Umpire build-and-test job defines both `SPEC` and
`RAD_BAT_HOST_CONFIG_NAME`. With `RAD_BAT_MODE` unset, `rad-bat` therefore
selects host-config mode. A missing file is an error and never causes an
implicit Spack invocation.

A job with only `SPEC` selects Spack mode. A job with only a host-config name
selects host-config mode. If both are present, host-config mode wins unless an
explicit `RAD_BAT_MODE` overrides the selection. Each job logs the set/unset
inputs and selected mode without printing their values.

To regenerate the complete maintained set, start a GitLab pipeline for the
mirrored GitHub branch with:

- `RAD_BAT_MODE=spack`
- optionally `RAD_BAT_PUSH_TO_BUILDCACHE=true` to publish dependencies

Every job creates an immutable, project-specific dependency installation and
publishes its generated host-config as a job artifact. Download all artifacts
into `host-configs/`, review them, and commit them through the normal GitHub
branch and pull-request workflow. The GitLab mirror is never modified by the
refresh pipeline.

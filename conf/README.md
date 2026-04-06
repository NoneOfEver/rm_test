# Build Variants

Put optional build-specific fragments here, for example:

- `debug.conf`
- `release.conf`
- `feature_x.conf`

Then apply them with `-DEXTRA_CONF_FILE=<file>` or your own build scripts.

Current project-specific overlays:

- `systemview.conf`: enables SEGGER SystemView tracing

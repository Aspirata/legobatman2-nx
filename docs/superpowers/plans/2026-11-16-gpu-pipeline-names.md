# Identify measured internal GPU pipelines

Run031 produced valid, available GPU timestamp results without another no-space
abort. Its later profile windows show 4.50-4.54 FPS; preceding windows show
2.39-3.59 FPS. The most costly individual printed samples are largely internal
compute/transfer pipelines identified only by transient handles. This is not
enough to choose a specific optimization, and sampled totals cannot be summed
or extrapolated.

Add bounded creation-time metadata mapping internal handles to resolve shaders,
host depth stores, EDRAM dumps, texture loaders, ownership-transfer shader/pass
keys, direct-color resolve and swap processing. Preserve timestamp algorithms,
rendering, fixed driver archives and runtime configuration. The metadata hook is
optional in ordinary builds and performs no GPU operation.

Save run031 source/NRO rollback, check host regressions and the full build, review
the bounded metadata diff, install silently with full readback, and repeat the
same cutscene. Decode names before proposing another rendering optimization.

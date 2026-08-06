# Loki Integration Guide

This guide describes how to integrate Loki as the master framework in dependent repositories such as FullStack and BlackSmith.

## Integration Model

- Loki remains the framework source of truth.
- Dependent repositories include Loki as a pinned submodule at `loki/`.
- Integration-specific application logic stays in the dependent repository.

## Baseline Steps

1. Add Loki as submodule:
   ```bash
   git submodule add https://github.com/Fomorianshifter/Loki.git loki
   ```
2. Initialize/update submodule:
   ```bash
   git submodule update --init --recursive
   ```
3. Wire build/test scripts to call Loki build targets from `loki/`.
4. Commit `.gitmodules` and `loki` gitlink pointer.

## Integration Validation Checklist

- Loki submodule initializes in clean clone
- dependent project builds successfully with pinned Loki commit
- dependent CI checks out submodules (`submodules: recursive`)
- Loki commit hash is documented in upgrade PR description

## Related Docs

- [SUBMODULE_GUIDE.md](SUBMODULE_GUIDE.md)
- [VERSION_COMPATIBILITY.md](VERSION_COMPATIBILITY.md)
- [BUILD.md](BUILD.md)

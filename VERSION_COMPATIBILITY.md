# Loki Version Compatibility Guide

This document defines how dependent projects should pin and validate Loki versions.

## Compatibility Policy

- Pin Loki by commit SHA or release tag in each dependent repository.
- Do not track floating branches in production integrations.
- Upgrade Loki only through explicit dependency bump commits/PRs.

## Recommended Versioning Flow

1. Identify target Loki commit/tag.
2. Update submodule pointer in dependent repo:
   ```bash
   cd loki
   git fetch origin
   git checkout <tag-or-commit>
   cd ..
   git add loki
   git commit -m "Bump Loki to <tag-or-commit>"
   ```
3. Run dependent project CI/build/test.
4. Merge only after validation succeeds.

## Compatibility Matrix Template

Use this template in release notes or project docs:

| Dependent Project Version | Loki Commit/Tag | Validation Status | Notes |
|---|---|---|---|
| vX.Y.Z | `<sha-or-tag>` | ✅ Passed | Baseline release |
| vX.Y+1.Z | `<sha-or-tag>` | ✅ Passed | Includes feature updates |

## Breaking Change Guidance

- Announce breaking API/hardware contract changes before upgrade rollout.
- Provide migration steps in Loki release notes.
- Update dependent repositories one at a time and verify runtime behavior.

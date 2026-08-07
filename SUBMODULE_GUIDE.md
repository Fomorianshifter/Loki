# Loki Submodule Guide

## Overview

Git submodules let one repository track another repository at a specific commit. Loki uses this model so Loki can remain the master framework while dependent projects (such as FullStack and BlackSmith) consume stable, pinned Loki versions.

Why this is useful:

- keeps Loki framework history independent
- gives dependent projects reproducible builds by pinning an exact Loki commit
- enables controlled upgrades instead of accidental framework drift

---

## For Loki Users

If you are cloning Loki directly, no submodule initialization is required for Loki itself:

```bash
git clone https://github.com/Fomorianshifter/Loki.git
cd Loki
```

Loki is the source framework. Submodule workflows are used by projects that depend on Loki.

Recommended mental model:

- **Loki repo:** framework authoring and release source
- **Dependent repo (FullStack/BlackSmith):** references Loki as a pinned dependency

---

## Adding Loki as Submodule (FullStack / BlackSmith)

### 1) Add Loki to a dependent project

From the dependent repository root:

```bash
git submodule add https://github.com/Fomorianshifter/Loki.git loki
git commit -m "Add Loki framework as submodule"
```

This creates:

- `.gitmodules` entry with Loki URL/path
- a tracked gitlink entry for `loki/` (pinned commit pointer)

### 2) Expected directory structure

```text
dependent-project/
├── .gitmodules
├── loki/                    # submodule checkout of Fomorianshifter/Loki
└── <project files>
```

### 3) Verify setup

```bash
git submodule status
cat .gitmodules
```

You should see `loki` listed and pointing to `https://github.com/Fomorianshifter/Loki.git`.

---

## Working with Submodules

### Clone a project that already uses Loki

```bash
git clone --recurse-submodules <dependent-project-url>
cd <dependent-project-dir>
```

If already cloned without recursion:

```bash
git submodule update --init --recursive
```

### Update Loki version in dependent project

```bash
cd loki
git fetch origin
git checkout <target-tag-or-commit>
cd ..
git add loki
git commit -m "Bump Loki submodule to <target-tag-or-commit>"
```

### Make Loki changes from inside dependent project

```bash
cd loki
git checkout -b feature/<change-name>
# edit + commit inside loki
git push origin feature/<change-name>
# open PR in Fomorianshifter/Loki and merge
```

After Loki PR merge:

```bash
cd loki
git fetch origin
git checkout <merged-loki-commit>
cd ..
git add loki
git commit -m "Update Loki pointer to include <change-name>"
```

### Commit submodule updates correctly

- commit framework code changes in Loki repository first
- then commit only the updated `loki` pointer in dependent repo
- include Loki commit/tag in the dependent repo commit message

---

## Troubleshooting

### Submodule out of sync

Symptoms:

- `git submodule status` shows `+` prefix
- working tree reports unexpected submodule changes

Fix:

```bash
git submodule sync --recursive
git submodule update --init --recursive
```

### Merge conflicts in submodule pointer

Symptom:

- merge conflict on `loki` path (gitlink conflict)

Fix:

1. Choose target Loki commit (usually newer validated commit).
2. Update submodule checkout:
   ```bash
   cd loki
   git checkout <resolved-commit>
   cd ..
   ```
3. Mark conflict resolved:
   ```bash
   git add loki
   git commit
   ```

### Detached HEAD in submodule

Submodules are commonly checked out in detached HEAD at pinned commits. This is normal for consumption.

If you need to develop in Loki:

```bash
cd loki
git checkout -b feature/<branch-name>
```

### Update all submodules

```bash
git submodule update --init --recursive
git submodule foreach --recursive git fetch origin
```

To move all to configured remote tracking branches (if configured):

```bash
git submodule update --remote --recursive
```

---

## CI/CD Considerations

In automated builds:

- always checkout with submodules enabled (`recursive: true`)
- ensure the CI job fetches full history if tags/commit resolution is required
- fail fast if submodule init/update fails
- run Loki build/tests from dependent project CI after submodule checkout

GitHub Actions example:

```yaml
- uses: actions/checkout@v4
  with:
    submodules: recursive
```

For pinned reproducibility, avoid auto-updating submodules in CI unless the workflow explicitly tests upgrade scenarios.

---

## Best Practices

- pin Loki to reviewed commits/tags, not floating branches
- treat Loki bump PRs as dependency upgrades with explicit validation notes
- validate both Loki and dependent-project tests before merging submodule bumps
- document compatibility decisions (supported Loki versions per project release)
- communicate framework-breaking changes before publishing new Loki versions

Practical recommendation:

1. Merge feature in Loki
2. Tag release (optional but recommended)
3. Update dependent repo `loki` pointer
4. Run dependent CI
5. Merge dependent update PR

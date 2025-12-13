---
agent: 'agent'
model: Claude Sonnet 4
tools: ['codebase', 'edit', 'runCommands', 'changes']
description: 'Prepare and release a drumlogue unit'
---

# Release Drumlogue Unit

You are preparing a unit for release. Follow the complete release workflow.

## Prerequisites

Ask for:
1. **Unit name** (e.g., clouds-revfx)
2. **Version number** (semantic: x.y.z or x.y.z-suffix like 1.0.0-pre, 2.1.0-beta)

## Release Workflow

### 1. Update Version

**Command:**
```bash
make version UNIT=<unit-name> VERSION=<x.y.z>
```

**What it does:**
- Converts semantic version to hex format: `0xMMNNPPU`
  - MM = major version (00-FF)
  - NN = minor version (00-FF)
  - PP = patch version (00-FF)
  - U = unsigned suffix
- Updates `.version` field in `drumlogue/<unit>/header.c`
- Adds comment with readable version

**Example:**
```bash
make version UNIT=clouds-revfx VERSION=1.2.3
# Sets: .version = 0x010203U,  // v1.2.3 (major<<16 | minor<<8 | patch)
```

**Pre-release versions:**
```bash
make version UNIT=clouds-revfx VERSION=1.0.0-pre
# Sets: .version = 0x010000U,  // v1.0.0 (1.0.0-pre)
```

### 2. Update Release Notes

**Create/update:** `drumlogue/<unit>/RELEASE_NOTES.md`

**Template:**
```markdown
# Release Notes - <Unit Name>

## v<X.Y.Z> - YYYY-MM-DD

### New Features
- Feature description

### Improvements
- Improvement description

### Bug Fixes
- Fix description

### Breaking Changes
- Change description (if any)

### Known Issues
- Issue description (if any)
```

**Example:**
```markdown
# Release Notes - Clouds RevFX

## v1.2.0 - 2024-03-15

### New Features
- Added pitch shifter mode
- Stereo width control

### Improvements
- Reduced CPU usage by 15%
- Smoother parameter transitions

### Bug Fixes
- Fixed reverb tail cutoff on preset change
- Corrected dry/wet mix at extreme values

### Known Issues
- None
```

### 3. Build Unit

**Command:**
```bash
make build UNIT=<unit-name>
```

**Or directly:**
```bash
./build.sh <unit-name>
```

**Verify build:**
- Check for warnings or errors
- Confirm `.drmlgunit` artifact exists
- Test on hardware if possible

### 4. Test Thoroughly

**Desktop tests:**
```bash
cd test/<unit-name>
make clean && make test
```

**Hardware tests:**
- Load `.drmlgunit` to drumlogue
- Test all parameters
- Verify presets
- Check MIDI/tempo sync (if applicable)
- Test edge cases

### 5. Commit Changes

**Review changes:**
```bash
git status
git diff
```

**Commit:**
```bash
git add drumlogue/<unit-name>/header.c
git add drumlogue/<unit-name>/RELEASE_NOTES.md
git commit -m "Release <unit-name> v<X.Y.Z>"
```

### 6. Create Git Tag

**Command:**
```bash
make tag UNIT=<unit-name> VERSION=<X.Y.Z>
```

**What it does:**
- Creates annotated tag: `<unit-name>/v<X.Y.Z>`
- Example: `clouds-revfx/v1.2.0`

**Verify tag:**
```bash
git tag | grep <unit-name>
```

### 7. Push Changes

**Push commits and tags:**
```bash
git push
git push --tags
```

**Or specific tag:**
```bash
git push origin <unit-name>/v<X.Y.Z>
```

### 8. Create GitHub Release (Optional)

**Via GitHub web UI:**
1. Go to repository → Releases → New Release
2. Select tag: `<unit-name>/v<X.Y.Z>`
3. Release title: `<Unit Name> v<X.Y.Z>`
4. Description: Copy from RELEASE_NOTES.md
5. Attach `.drmlgunit` file
6. Publish release

**Via gh CLI:**
```bash
gh release create <unit-name>/v<X.Y.Z> \
  drumlogue/<unit-name>/<unit-name>.drmlgunit \
  --title "<Unit Name> v<X.Y.Z>" \
  --notes-file drumlogue/<unit-name>/RELEASE_NOTES.md
```

## Full Release Command

**One-step preparation:**
```bash
make release UNIT=<unit-name> VERSION=<X.Y.Z>
```

**What it does:**
1. Updates version in header.c
2. Builds the unit
3. Shows next steps

**Then manually:**
1. Update RELEASE_NOTES.md
2. Test thoroughly
3. Commit changes
4. Create tag: `make tag UNIT=<unit-name> VERSION=<X.Y.Z>`
5. Push: `git push && git push --tags`

## Version Guidelines

**Semantic Versioning (x.y.z):**
- **Major (x):** Breaking changes, major new features
- **Minor (y):** New features, backward compatible
- **Patch (z):** Bug fixes, minor improvements

**Pre-release suffixes:**
- `-pre`: Pre-release/release candidate
- `-beta`: Beta version
- `-alpha`: Alpha version
- `-rc1`: Release candidate 1

**Examples:**
- `1.0.0` - Initial release
- `1.0.1` - Bug fix
- `1.1.0` - New feature
- `2.0.0` - Breaking change
- `1.2.0-pre` - Pre-release

## Checklist

- [ ] Version updated in header.c
- [ ] RELEASE_NOTES.md updated
- [ ] Unit builds without errors
- [ ] Desktop tests pass
- [ ] Hardware testing complete
- [ ] All parameters verified
- [ ] Documentation updated (README.md)
- [ ] Changes committed
- [ ] Git tag created
- [ ] Changes pushed to remote
- [ ] GitHub release created (optional)
- [ ] .drmlgunit uploaded to release

## Multi-Unit Releases

**For multiple units:**
```bash
# Unit 1
make release UNIT=clouds-revfx VERSION=1.2.0
# ... test, commit, tag ...

# Unit 2
make release UNIT=elementish-synth VERSION=2.0.0
# ... test, commit, tag ...

# Push all
git push && git push --tags
```

## Hotfix Workflow

**For urgent fixes:**
```bash
# Increment patch version
make version UNIT=<unit> VERSION=x.y.z+1
# Fix issue
# Test
make build UNIT=<unit>
# Release
git commit -am "Hotfix <unit> v<x.y.z+1>"
make tag UNIT=<unit> VERSION=<x.y.z+1>
git push && git push --tags
```

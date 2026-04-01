# Releasing oceandl

## Versioning policy

- Follow Semantic Versioning.
- Update `cpp/include/oceandl/version.hpp` for each release.
- Record user-visible changes in `CHANGELOG.md`.

## Release checklist

1. Ensure `main` is green in CI (Linux/macOS/Windows).
2. Run local smoke checks:
   - `oceandl --version`
   - `oceandl datasets`
   - `oceandl providers`
   - `oceandl download --help`
3. Update:
   - `CHANGELOG.md`
   - `README.md` (if user-facing behavior changed)
4. Create a version tag:
   - `git tag vX.Y.Z`
   - `git push origin vX.Y.Z`
5. Verify `Release` workflow uploaded all platform artifacts.
6. Publish release notes from changelog entries.

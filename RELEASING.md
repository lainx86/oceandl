# Releasing oceandl

## Public release positioning

Keep public messaging aligned with the current project state:

- releases are still alpha,
- the primary supported path is still source build on the CI-covered platforms,
- GitHub Release archives are convenience artifacts for evaluation and early adopters, not a claim that `oceandl` is already a stable binary-first product,
- the maintainer-owned release artifacts currently published by the workflow are Linux `x64` plus the formal source archive,
- broader package-manager distribution beyond the maintainer-owned AUR package is not published yet and should not be implied in release notes,
- the first maintainer-owned package-manager target is the Arch `makepkg` package spec in `packaging/arch/oceandl/`.

## Versioning policy

- Follow Semantic Versioning.
- Update `cpp/include/oceandl/version.hpp` for each release.
- Record user-visible changes in `CHANGELOG.md`.

## Release checklist

1. Ensure the Linux release path is green locally and in GitHub Actions before tagging.
2. Ensure the support matrix and bootstrap commands in `README.md` still match the current CI/toolchain reality.
3. Ensure the release notes and README language still describe the release as alpha/source-first unless that policy has intentionally changed.
4. Run local smoke checks from the build tree:
   - `oceandl --version`
   - `oceandl datasets`
   - `oceandl providers`
   - `oceandl download --help`
5. Update:
   - `CHANGELOG.md`
   - `README.md` (if user-facing behavior changed)
6. Create a version tag:
   - `git tag vX.Y.Z`
   - `git push origin vX.Y.Z`
7. Verify the `Release` workflow packages the Linux install tree:
   - `bin/` contains `oceandl`
   - non-system runtime libraries are bundled next to the executable on Linux
8. Verify the publish job also creates the formal source archive `oceandl-src-vX.Y.Z.tar.gz`.
9. Verify the workflow re-downloads the final Linux archive, extracts it, and smoke-tests the extracted binary.
10. Verify the publish job generates and uploads `SHA256SUMS`.
11. Verify each uploaded archive can be checked with the published SHA-256 values.
12. Update `packaging/arch/oceandl/PKGBUILD` with the new `pkgver` and the SHA-256 for `oceandl-src-vX.Y.Z.tar.gz`.
13. Regenerate `packaging/arch/oceandl/.SRCINFO`:
    - `cd packaging/arch/oceandl`
    - `makepkg --printsrcinfo > .SRCINFO`
14. Verify the package-manager install path locally before publishing the package spec externally:
    - `./scripts/create_source_release.sh X.Y.Z dist/oceandl-src-vX.Y.Z.tar.gz`
    - `./scripts/verify_arch_pkgbuild.sh dist/oceandl-src-vX.Y.Z.tar.gz packaging/arch/oceandl`
15. Publish release notes from changelog entries, keeping the alpha/source-first framing explicit.

## Artifact integrity

- Public releases publish a `SHA256SUMS` file for all archived assets.
- Public releases also publish the formal source archive `oceandl-src-vX.Y.Z.tar.gz` for package-manager consumers.
- Detached signatures are intentionally **not** published yet.
- Signature support is deferred until maintainer-managed signing keys, storage, and rotation policy are in place.
- When signing keys are ready, add detached signatures for `SHA256SUMS` or for each release asset as a follow-up.

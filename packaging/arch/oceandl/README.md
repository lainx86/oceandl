# Arch package spec for `oceandl`

This directory contains the maintainer-owned Arch `makepkg` package spec selected as the
first official package-manager target for `oceandl`.

Current scope:

- official first-stage target: Arch `makepkg` / AUR-compatible source package
- authoritative package files live in this repository
- external publication, such as an AUR repo, is a maintainer follow-up once tagged releases are flowing

The package builds from the formal source release asset:

- asset name: `oceandl-src-vX.Y.Z.tar.gz`
- asset URL: `https://github.com/lainx86/oceandl/releases/download/vX.Y.Z/oceandl-src-vX.Y.Z.tar.gz`
- checksum source: the matching line in `SHA256SUMS`

Until the first tagged release for a given version exists, the checked-in `PKGBUILD` keeps its
default checksum as `SKIP`. The maintainer is expected to replace it with the published SHA-256
immediately after the release workflow uploads the matching source asset.

## Update process

1. Release the tag and wait for the `Release` workflow to publish `oceandl-src-vX.Y.Z.tar.gz` and `SHA256SUMS`.
2. Copy the SHA-256 for `oceandl-src-vX.Y.Z.tar.gz` from `SHA256SUMS`.
3. Update `pkgver` and the pinned checksum in `PKGBUILD`.
4. Regenerate `.SRCINFO`:

```bash
cd packaging/arch/oceandl
makepkg --printsrcinfo > .SRCINFO
```

5. Verify the package spec against the staged source archive before publishing it externally:

```bash
./scripts/create_source_release.sh X.Y.Z dist/oceandl-src-vX.Y.Z.tar.gz
./scripts/verify_arch_pkgbuild.sh dist/oceandl-src-vX.Y.Z.tar.gz packaging/arch/oceandl
```

## Local verification override

For local validation before a public tag exists, `PKGBUILD` accepts:

- `OCEANDL_SOURCE_URL`
- `OCEANDL_SOURCE_SHA256`

The verification script above uses those overrides automatically so the install path can be
tested without downloading from GitHub.

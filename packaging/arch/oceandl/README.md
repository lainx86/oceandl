# Arch package spec for `oceandl`

This directory contains the maintainer-owned Arch `makepkg` package spec selected as the
first official package-manager target for `oceandl`.

Current scope:

- official first-stage target: Arch `makepkg` / AUR-compatible source package
- authoritative package files live in this repository
- the AUR package repo is now published at `aur.archlinux.org/packages/oceandl`

The package builds from the formal source release asset:

- asset name: `oceandl-src-vX.Y.Z.tar.gz`
- asset URL: `https://github.com/lainx86/oceandl/releases/download/vX.Y.Z/oceandl-src-vX.Y.Z.tar.gz`
- checksum source: the matching line in `SHA256SUMS`

The checked-in `PKGBUILD` pins the expected SHA-256 for the current published release.
After each new tag, update that checksum from the published `SHA256SUMS`.

## Update process

1. Release the tag and wait for the `Release` workflow to publish `oceandl-src-vX.Y.Z.tar.gz` and `SHA256SUMS`.
2. Copy the SHA-256 for `oceandl-src-vX.Y.Z.tar.gz` from `SHA256SUMS`.
3. Update `pkgver` and the pinned checksum in `PKGBUILD`.
4. Regenerate `.SRCINFO`:

```bash
cd packaging/arch/oceandl
makepkg --printsrcinfo > .SRCINFO
```

5. Verify the package spec against the public source archive before publishing or updating the AUR repo:

```bash
curl -L -O https://github.com/lainx86/oceandl/releases/download/vX.Y.Z/oceandl-src-vX.Y.Z.tar.gz
./scripts/verify_arch_pkgbuild.sh oceandl-src-vX.Y.Z.tar.gz packaging/arch/oceandl
```

## Local verification override

For local validation before a public tag exists, `PKGBUILD` accepts:

- `OCEANDL_SOURCE_URL`
- `OCEANDL_SOURCE_SHA256`

The verification script above uses those overrides automatically so the install path can be
tested without downloading from GitHub.

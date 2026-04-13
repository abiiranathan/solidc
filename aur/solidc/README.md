# solidc AUR Packaging

This directory contains the PKGBUILD and related files for publishing solidc to the Arch User Repository (AUR).

## Maintainer
Dr. Abiira Nathan <nabiira2by2@gmail.com>

## Package model
This is a **stable release** package (`solidc`), built from the tagged GitHub release tarball.
For a rolling VCS package that tracks `HEAD`, rename to `solidc-git` and switch to a `git+` source.

## Build and install locally

```sh
cd aur/solidc
makepkg -si
```

## Updating the checksum after a new release

```sh
curl -L https://github.com/abiiranathan/solidc/archive/refs/tags/<NEW_VER>.tar.gz -o solidc-<NEW_VER>.tar.gz
b2sum solidc-<NEW_VER>.tar.gz
```

Replace `b2sums=('SKIP')` in PKGBUILD with the real hash, bump `pkgver`, reset `pkgrel=1`,
then regenerate `.SRCINFO`:

```sh
makepkg --printsrcinfo > .SRCINFO
```

## AUR submission checklist
- [ ] `pkgdesc` in PKGBUILD and `.SRCINFO` match exactly
- [ ] `b2sums` contains real checksums (not `SKIP`) for a final release
- [ ] `.SRCINFO` was regenerated with `makepkg --printsrcinfo > .SRCINFO`
- [ ] `makepkg -si` succeeds cleanly on a clean build
- [ ] Only PKGBUILD, .SRCINFO, .gitignore, and README.md are committed to the AUR git repo

## Dependencies
`cmake` `git` `make` `gcc`
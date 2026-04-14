#!/usr/bin/env bash
set -euo pipefail

# Update the existing clone instead of re-cloning
git -C src/solidc pull origin main

makepkg --nobuild --holdver
makepkg --printsrcinfo > .SRCINFO

pkgver=$(grep '^\s*pkgver' .SRCINFO | head -1 | awk '{print $3}')

git add .SRCINFO PKGBUILD
git commit -m "Update PKGBUILD and .SRCINFO for version ${pkgver}"
git push origin master
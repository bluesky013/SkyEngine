# vcpkg Registry Ports

This directory contains vcpkg port definitions for packages that are not available
in the main [microsoft/vcpkg](https://github.com/microsoft/vcpkg) registry.

These ports are intended for submission to
[Zhuyin-Graphics/vcpkg-registry](https://github.com/Zhuyin-Graphics/vcpkg-registry).

## Ports

| Port | Version | Description |
|------|---------|-------------|
| sfmt | 1.5.4 | SIMD-oriented Fast Mersenne Twister pseudorandom number generator |
| gklib | 5.1.1 | Helper routines used by KarypisLab software (METIS, etc.) |
| ispc-texcomp | 2024-01-01 | ISPC Texture Compressor |

## How to Submit

Follow the steps in the registry's
[AddPorts.md](https://github.com/Zhuyin-Graphics/vcpkg-registry/blob/main/AddPorts.md):

1. Copy the port directories from `registry/ports/` into the registry's `ports/` directory
2. Run `vcpkg format-manifest` on each port's `vcpkg.json`
3. Compute the SHA512 hashes for `vcpkg_from_github()` in each `portfile.cmake`
   (replace the placeholder `0` values)
4. Commit the ports, then use `vcpkg x-add-version` to generate proper git-tree hashes:
   ```bash
   cd <vcpkg-registry>
   vcpkg x-add-version --x-builtin-ports-root=./ports \
     --x-builtin-registry-versions-dir=./versions sfmt
   vcpkg x-add-version --x-builtin-ports-root=./ports \
     --x-builtin-registry-versions-dir=./versions gklib
   vcpkg x-add-version --x-builtin-ports-root=./ports \
     --x-builtin-registry-versions-dir=./versions ispc-texcomp
   ```
5. Update `versions/baseline.json` with correct baselines
6. Commit and push, then open a merge request

## SHA512 Hash Computation

To compute the correct SHA512 hash for each port's `vcpkg_from_github()`:

```bash
# Download the source archive and compute its SHA512
# For sfmt:
wget -qO- https://github.com/MersenneTwister-Lab/SFMT/archive/refs/tags/1.5.4.tar.gz | sha512sum

# For gklib:
wget -qO- https://github.com/KarypisLab/GKlib/archive/refs/tags/METIS-v5.1.1-DistDGL-0.5.tar.gz | sha512sum
```

Or let vcpkg compute them automatically — when the hash is `0`, vcpkg will
download the archive, print the correct hash, and fail. Copy the hash from the
error output into the portfile.

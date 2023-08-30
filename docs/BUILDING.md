# Building WOWlet

Building for Linux and Windows via Docker is done in 3 steps:

1. Cloning this repository (+submodules)
2. Creating a base Docker image
3. Using the base image to compile a build

**important:** you only have to do step 2 (base docker image) once.

For Mac OS, scroll down.

# Linux

For more information, check the Dockerfile: `Dockerfile`.

### 1. Clone

```bash
git clone --branch master --recursive https://git.wownero.com/wowlet/wowlet.git
cd wowlet
```

Replace `master` with the desired version tag (e.g. `v3.1.0`) to build the release binary.

### 2. Base image

```bash
docker build --tag wowlet:linux --build-arg THREADS=6 .
```

Building the base image takes a while. **You only need to build the base image once.**

### 3. Build

```bash
docker run --rm -it -v $PWD:/wowlet -w /wowlet wowlet:linux sh -c 'make release-static -j6'
```

If you're re-running a build make sure to `rm -rf build/` first.

The resulting binary can be found in `build/bin/wowlet`.

# Windows

### 1. Clone

```bash
git clone --branch master --recursive https://git.wownero.com/wowlet/wowlet.git
cd wowlet
```

Replace `master` with the desired version tag (e.g. `v3.1.0`) to build the release binary.

### 2. Base image

```bash
docker build -f Dockerfile.windows --tag wowlet:win --build-arg THREADS=6 .
```

Building the base image takes a while. **You only need to build the base image once.**

### 3. Build

```bash
docker run --rm -it -v $PWD:/wowlet -w /wowlet wowlet:win sh -c 'make windows root=/depends target=x86_64-w64-mingw32 tag=win-x64 -j6'
```

If you're re-running a build make sure to `rm -rf build/` first.

The resulting binary can be found in `build/x86_64-w64-mingw32/release/bin/wowlet.exe`.

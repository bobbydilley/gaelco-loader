# Gaelco Loader

A loader for Gaelco Linux Games

## Supported Games

- Tokyo Cop
- Ring Riders
- Championship Tuning Race

## Download

Grab the latest `GaelcoLoader-x86_64.AppImage` from the
[**Releases**](../../releases) page (the `continuous` prerelease is rebuilt on
every push to `master`), then:

```
chmod +x GaelcoLoader-x86_64.AppImage
./GaelcoLoader-x86_64.AppImage /path/to/gameport
```

## Building

```
cmake -S . -B build
cmake --build build
```

Produces `build/gaelco` and `build/gaelco-preload.so`.

### AppImage

```
cmake --build build
scripts/build-app-image        # -> GaelcoLoader.AppImage
```

Or fully reproducibly with Docker (no host dependencies):

```
docker build --target build -t gaelco-loader-build .
cid=$(docker create gaelco-loader-build)
docker cp "$cid:/out/GaelcoLoader.AppImage" .
docker rm "$cid"
```

## Releases (CI)

`.github/workflows/release.yml` builds the AppImage in the Docker image on every
push to `master` and publishes it to a rolling `continuous` prerelease. Pushing a
`v*` tag (`git tag v1.0.0 && git push origin v1.0.0`) instead cuts a permanent,
"latest" release with auto-generated notes.

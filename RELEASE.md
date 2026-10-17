# Wowlet release process

GitHub `wownero/wowlet` owns source and public releases. Private `Builds/wowlet`
is the build ingress on our hardware. Fleet owns dispatch and publication plans.
A source merge, a build, a code signature and a published release are separate outcomes.

1. Prepare and review a GitHub pull request with the version, wallet-library pin,
   support documentation and release inputs. Publish a new library commit to its
   canonical repository before pinning it. Keep source and dependency revisions exact.
2. `make check` runs the cheap source checks. Run the private Linux, Windows and
   macOS workflows through reviewed Fleet admission with `expected_sha` equal to
   the protected Builds commit. Preserve canonical source and Builds commit identities
   separately. Do not run release builds on GitHub.
3. The build helpers verify embedded Wownero archives against `contrib/release/wownerod.json`.
   That file records the reviewed checksum provenance. Do not describe an unsigned
   checksum list as a PGP verification.
4. Collect artifacts by their run and artifact identities. Require a Linux AppImage,
   macOS archive, signed Windows archive and unsigned Windows archive. Check archive
   paths, bundled runtimes and packaging requirements. Never select an artifact by size.
5. Require Developer ID signing, accepted notarization and stapling for macOS.
   Require valid, timestamped Such Software LLC Authenticode signing for Windows.
   The EV token stays on its signing host; CI never receives its password or key.
6. Record any manual wallet checks actually performed. Manual testing and independent
   reproducibility are separate evidence; successful builds do not prove either.
   The unsigned Windows archive is provided for reproduction, without claiming an
   independent reproduction occurred.
7. Create `hashes.txt` over the four final archives. Sign it on the owner's signing
   machine with the existing release key `9F9930300530DF724761B7055A881347310EB79F`.
   Verify the detached signature and all hashes before publication. Never transfer
   the private key or put a passphrase in a command, environment or log.
8. Create an annotated `vX.Y.Z` tag from the exact reviewed canonical source, matching
   `CMakeLists.txt`. Use Fleet's reviewed publication plan to upload the four archives,
   `hashes.txt` and `hashes.txt.asc` to GitHub Releases. Refuse replacement of existing
   tags or assets; recover partial publication from retained intent.
9. Download the public assets again and verify their hashes and PGP signature. Record
   the tag, source commit, Builds commit, run identities, signing evidence and public
   URL in the private journal. Release notes should say when security fixes are included.

Additional architectures and Guix builds remain separate lanes. Do not claim a
platform is released merely because its workflow exists. Announcements are an
owner action, separate from publishing the release files.

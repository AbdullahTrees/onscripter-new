# Security policy

## Supported versions

Security fixes are provided for the latest `onscripter-new` release only.

## Reporting a vulnerability

Please report a suspected vulnerability privately through GitHub's **Report a
vulnerability** form for this repository. Do not include game assets, scripts,
saves, credentials, or other copyrighted/private data in a report.

Include the affected version, platform, reproduction steps, and a minimal
sanitized input when possible. Reports will be acknowledged as soon as a
maintainer is available. A fix and coordinated disclosure timeline will be
chosen according to severity and exploitability.

## Trust boundary

Game archives, scripts, media, save files, option files, and URLs can all reach
native parsers or third-party codecs. Only load data obtained from a trusted,
legal source. Dependency archives used for builds are pinned by SHA-256, but
release binaries should still be obtained from the official release page and
verified against `SHA256SUMS.txt`.

Security-sensitive archive, command-line, serialized-data, and regex logic has
standalone regression and fuzz coverage. Size caps and structural checks reduce
the impact of malformed inputs, but they do not turn bundled native image/audio/
video codecs into a sandbox. The engine does not intentionally execute shell
commands from game data; external launches use direct argument vectors or
properly quoted platform APIs.

Builds and tests should run with least privilege. Do not build unreviewed forks
with access to signing keys, release credentials, or private game assets.

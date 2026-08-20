# IBM dump header generation

`op-dump-packager` creates and validates the fixed IBM wrapper used by the BMC,
fault-data, and system dump paths. It replaces the former shell-based
`gendumpheader` implementation.

The serializer writes into a zero-initialized fixed-size buffer at explicit
offsets. Every write is bounds checked, size arithmetic is checked before a
header is built, and the complete result is validated before it is combined with
the payload.

## Format profiles

| Profile  | Payload          | ID input                     | Header size | Payload offset |
| -------- | ---------------- | ---------------------------- | ----------: | -------------: |
| `bmc`    | Zstandard tar    | 1-8 decimal digits           |     `0x274` |        `0x274` |
| `fault`  | Uncompressed tar | 1-8 decimal digits           |     `0x274` |        `0x274` |
| `system` | Gzip tar         | Exactly 8 hexadecimal digits |     `0x4D0` |        `0x4D0` |

The existing BMC parser command remains correct because `tail` counts bytes from
one:

```sh
tail -c +$((0x275)) "$name" > "$tmp_tarfile"
```

System dumps use a different format and begin their payload at `0x4D0`.

### BMC and fault layout

|  Offset |    Size | Content                          |
| ------: | ------: | -------------------------------- |
| `0x000` |  `0x40` | `FILE` directory entry           |
| `0x040` |  `0x30` | Payload `SECTION` entry          |
| `0x070` | `0x200` | `BMC DUMP` or `FLT DUMP` summary |
| `0x270` |  `0x04` | Dump-entry trailer               |
| `0x274` |       - | Payload                          |

The section and summary size fields contain `payload size + 0x204`. The BMC
summary dump-ID field remains zero for format compatibility. The filename uses
an eight-digit decimal ID, while the physical file keeps the community dump
manager's filename. Fault headers continue to contain `FLTDUMP` even though
older code referred to `NAGDUMP` in an unused variable.

### System layout

|  Offset |    Size | Content                             |
| ------: | ------: | ----------------------------------- |
| `0x000` |  `0x40` | `FILE` directory entry              |
| `0x040` |  `0x30` | `DUMP SUMMARY` section entry        |
| `0x070` |  `0x30` | `HARDWARE DATA` section entry       |
| `0x0A0` |  `0x30` | Empty final `HYPERVISOR DATA` entry |
| `0x0D0` | `0x400` | `SYS DUMP` summary                  |
| `0x4D0` |       - | Payload                             |

The dump ID is preserved as eight ASCII hexadecimal digits in the filename and
encoded as a big-endian `uint32_t` in the summary. Header version `0x0221` is
used for the legacy backend and `0x0222` for the next backend. The hardware
section retains its existing 32-bit payload limit.

System content type remains compatible with the existing ID prefixes: `00` maps
to hardware data, `20` to Hostboot data, and `30`/`40` to SBE data. An unknown
prefix is reported and retains the legacy zero content-type value.

## Metadata and fallback policy

D-Bus properties are read as typed values. A missing or invalid optional value
does not alter the header length:

| Field           | Accepted input                                                 | Fixed fallback              |
| --------------- | -------------------------------------------------------------- | --------------------------- |
| Model           | Exactly 8 printable, non-whitespace ASCII bytes                | `00000000`                  |
| System serial   | 1-7 ASCII alphanumeric bytes; shorter values are left padded   | `0000000`                   |
| BMC serial      | 1-12 ASCII alphanumeric bytes; shorter values are right padded | `000000000000`              |
| Hostname        | 1-32 printable, non-whitespace ASCII bytes                     | `Server-<model>-SN<serial>` |
| Event/PEL ID    | Typed `uint32_t`                                               | Zero                        |
| Originator type | Client, Internal, or SupportingService                         | Zero-filled                 |
| Originator ID   | Up to 32 printable ASCII bytes                                 | Zero-filled                 |

Non-ASCII, overlong, empty, or whitespace-only fields use the fixed fallback
rather than being truncated. This rule prevents metadata content from moving the
payload boundary.

## Packaging guarantees

The packager:

1. Opens and locks a regular, non-symlink archive and obtains its size once with
   `fstat`.
2. Checks the expected Zstandard, tar, or gzip signature for the selected
   profile.
3. Builds and validates the complete fixed header.
4. Writes header and payload to a unique temporary file in a private staging
   directory below the destination directory.
5. Verifies the source did not change, verifies final size, preserves the
   archive permission bits, and calls `fsync`.
6. Atomically replaces the locked source for in-place packaging or uses a
   no-clobber rename for a distinct destination.
7. Treats rename as the publication commit point, keeps the descriptor open
   across it, and closes it under the final name. Every staging artifact is
   removed on ordinary failure.

BMC and system packages are published directly into their requested destination
directories. Keeping the descriptor open across rename is deliberate: the
OpenPOWER watcher receives `IN_MOVED_TO`, while the close-only BMC watcher
receives `IN_CLOSE_WRITE` for the completed final name and never consumes the
temporary filename. Failed staging writes close inside the private,
non-recursively-watched directory, so they cannot consume the final-file watch.

## Validation

The dependency-free unit test covers exact sizes and offsets, decimal and
hexadecimal ID contracts, metadata boundaries, overflow rejection, the
eight-space Model incident, profile signatures, payload identity, permission
preservation, failure cleanup, no-clobber publication, watcher events, and both
in-place and distinct-destination atomic replacement. Golden hashes anchor the
BMC incident header and a known-good system header.

```sh
meson setup build \
    -Dtests=enabled \
    -Ddump-collection=disabled \
    -Dhostboot-dump-collection=disabled \
    -Dphal_backend=none
meson compile -C build
meson test -C build --print-errorlogs
```

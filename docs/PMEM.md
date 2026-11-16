# Persistent memory (Intel Optane, App Direct): `--dax`

**Status: not measured, and not yet run.** `--dax` is an option of `strata-poly`, off by default. Nothing on this
page has been run on persistent memory, so it has no numbers: it says what the option makes the engine do, what
that rests on, and how to try it. Without `--dax` the engine does what it did before.

## What it is for

`strata-poly` maps the model's GGUF and computes the experts that are not in VRAM on the CPU, straight from the
mapping ([HOW_IT_WORKS.md](HOW_IT_WORKS.md)). On a drive the mapped bytes are pages of the system's file cache. So
the engine holds them within the RAM that is free ([`--ram-experts-mib`](BACKENDS.md)), reads them through the
cache ahead of the page faults, and drops pages it no longer needs.

Windows and Linux document a file on a DAX volume differently: its mapping is the persistent memory's own pages,
and the file cache keeps no copy. `--dax` makes the engine plan on that. Whether it holds on a given PC, and what
the CPU's reads from persistent memory cost against RAM, is what has to be measured.

## What `--dax` does

When the system says **every file of the model** is mapped straight from persistent memory, the load says so and:

- keeps every expert outside VRAM mapped, with no RAM budget (as `--ram-experts-mib -1`); a `--ram-experts-mib N`
  given with it is not used, and the load says that;
- leaves those files out of the RAM it counts when it sizes the VRAM reserve;
- does not read the experts through the file cache ahead of the page faults (Windows); their pages are still
  touched once after the load;
- does not drop pages of those files after the load or between requests;
- does not warn about a container's memory limit near the files' size (Linux).

When the system does not say it of one of the model's files, the load names that file and goes on as without
`--dax`. A `--draft-model` file on a drive does not turn it off: that file is counted and handled as on a drive.

## What the system is asked

[`include/strata/platform/dax.hpp`](../include/strata/platform/dax.hpp):

- **Windows:** the volume's `FILE_DAX_VOLUME` flag (`GetVolumeInformationByHandleW`), and that the file is not
  compressed, encrypted or sparse: NTFS maps those through the file cache on a DAX volume too.
- **Linux:** the file's `STATX_ATTR_DAX` (`statx`), which kernels from 5.8 report. An older kernel does not say,
  so the answer there is no.

`STRATA_DAX=1` answers yes for every file and `STRATA_DAX=0` no, in the system's place. With `--dax`,
`STRATA_DAX=1` runs this path for a model on any drive: on a PC without persistent memory, to see that it loads
and answers and what the missing read-ahead costs; on a Linux kernel before 5.8, where the volume is DAX and the
kernel does not say.

## Trying it

1. Put the modules in App Direct mode and make a DAX volume: on Windows `format <drive>: /fs:ntfs /dax /q`; on
   Linux an fsdax namespace with ext4 or xfs mounted `-o dax`.
2. Copy the model's GGUF there (not compressed, not sparse) and start the engine with `--model` on it and `--dax`.
3. The load prints `--dax: the model's files are on a volume that maps persistent memory directly`.

## What is not done

- No measurement: the speed of the CPU's experts read from persistent memory against RAM. `--no-gpu-experts`
  with the model on the DAX volume and `--dax`, against the same run with it on a drive, would give it.
- No copy in RAM of the most-used experts above persistent memory. `--ram-experts-mib` is not used with `--dax`
  for that reason; whether such a copy is worth building depends on the measurement above.
- The conversation cache's files ([`src/poly/spill.cpp`](../src/poly/spill.cpp)) go to the temporary directory as
  before.
- `strata.exe` (the Qwen3.8-Flash-Next engine, `src/core`) has no such option: it keeps the experts in a pinned
  copy in RAM.

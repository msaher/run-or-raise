# `run-or-raise.exe`

A dependency-free windows utility for raising a window if it exists, or running an associate command if it doesn't. Born out of a desire to mimic my linux desktop usage when I'm stuck on WSL.

# Usage

```
$ run-or-raise.exe --help

Usage: run-or-raise.exe [options] CMD WINDOWCLASS

--key [ctr|shift|alt]+KEY CMD WINDOWCLASS
        Listen to KEY to run CMD or raise WINDOWCLASS. Can be passed multiple times.
--help show this help message

```

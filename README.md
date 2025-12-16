# Backup Manager

Automatic folder backup with real-time updates, written in low-level C.

## Overview

Backup Manager is a low-level Linux backup system written in pure C.  
It provides real-time directory mirroring using filesystem event monitoring, process isolation, and iterative directory traversal.

The project focuses on **correctness, robustness, and system-level mechanisms**, rather than high-level abstractions.

## Key Features

| Feature                    | Description                                                       |
| -------------------------- | ----------------------------------------------------------------- |
| ✅ Real-time backup         | Automatically synchronizes changes using Linux `inotify`          |
| ✅ Full directory mirroring | Source and target directories are kept structurally identical     |
| ✅ Iterative traversal      | All subdirectories are processed without recursive function calls |
| ✅ Process isolation        | Each backup runs in its own child process                         |
| ✅ Signal handling          | Clean shutdown using POSIX signals                                |
| ✅ Safe symlink handling    | Symbolic links are preserved and rewritten correctly              |
| ✅ Robust memory management | Explicit allocation control with clear ownership and cleanup      |
| ✅ POSIX-compliant          | Uses standard UNIX system calls only                              |

## How It Works

1. The main process starts an interactive CLI.
2. When `add` is called, the program forks a **child process** for the backup task.
3. The child process:
   - Performs an initial full synchronization (`sync_mirror`)
   - Registers all directories with `inotify`
   - Waits for filesystem events
4. On any detected event, a **safe full resynchronization** is performed.
5. The parent process tracks active backups and controls them via signals.

## Architecture Overview

- `sop-backup.c` — main program, CLI, signal handling  
- `kopia_operacje.c` — backup lifecycle operations (`add`, `end`, `restore`)  
- `proces_potomny.c` — child worker logic and `inotify` handling  
- `zarzadzanie.c` — directory mirroring and cleanup logic  
- `pomocnicze.c` — utilities (paths, parsing, filesystem helpers)

## Directory Synchronization Strategy

Instead of applying incremental diffs, the system:

- Iteratively traverses the entire source tree
- Copies only files that are **newer or missing**
- Removes files from the target that no longer exist in the source

This approach is:

- Safer against missed `inotify` events
- Easier to reason about correctness
- Efficient enough for typical user directories

## Command-Line Interface

```
add <source> <target1> [target2 ...]
end <source> <target>
list
restore <source> <backup>
exit
```

### Example

```
add "/home/user/Documents" "/mnt/backup/Documents"
```

## Technologies Used

- C (GNU11)
- POSIX system calls
- `fork`, `waitpid`, signals
- `inotify`
- `opendir`, `readdir`, `lstat`
- Manual memory management

## Project Goals

- Demonstrate low-level filesystem programming
- Practice safe process management
- Implement real-time synchronization without external libraries
- Ensure deterministic and debuggable behavior

## License

MIT License

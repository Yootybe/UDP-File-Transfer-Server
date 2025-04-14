# UDP File Transfer Server

This project implements a UDP-based file transfer service that:
- Splits data into packets
- Sends data from a client to a server using multiple file IDs
- Delivers packets in a randomized order
- Validates the transfer by comparing checksums on both ends

## Requirements

- C++17 compatible `g++` compiler

## Build

To build the project:

1. From the project root directory, run:

   ```bash
   make
   ```

This will generate the `server` and `client` binaries in the `build/` directory.

## Run & Test

There are two ways to test the service:

### 1. Use `make run`

This command:
- Builds the project
- Creates two random test files: 2MB and 1MB
- Starts the server
- Sends both files from the client to the server
- Verifies file integrity
- Automatically shuts down the server after clients finish

```bash
make run
```

### 2. Manually run from the `build/` directory

Start the server:

```bash
./build/server 12345
```

In another terminal, run the client:

```bash
./build/client /path/to/testfile 12345
```

- **Client arguments:**
  - First: Path to the test file (required)
  - Second: Port (optional, defaults to `12345`)
  
- **Server arguments:**
  - One optional argument for port (defaults to `12345`)

## Tested On

- Ubuntu 20.04
- g++ 9.4.0

---

## Note: Possible Server Improvements

Currently, the server is a single-threaded application that processes multiple file transfers sequentially. This approach is simple but not optimal for performance or scalability.

### Potential Enhancements:

- If the number of expected files is roughly equal to the number of CPU cores, the server logic could be modified to **spawn a new thread for each file**.
- If the server expects to handle significantly more files than available cores, a **concurrent (asynchronous or thread-pooled) architecture** would be more efficient.

These changes could improve throughput and better utilize system resources under heavier loads.

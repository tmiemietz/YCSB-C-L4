# sqlite-shm-srv

This is the server that implements the database driver for the `sqlite_shm`
backend of YCSB-C-L4.

### Building

This server is built as part of the overall build routine for this package.

### Running

The server can be started as follows:

```
sqlite-shm-srv
```

Note that the server expects a capability called `shm`. This is the server-side
end of the communication channel that the server maintains to its clients.

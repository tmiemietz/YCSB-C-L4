# sqlite-ipc-srv

This is the server that implements the database driver for the `sqlite_ipc`
backend of YCSB-C-L4.

### Building

This server is built as part of the overall build routine for this packge.

### Running

The server can be started as follows:

```
sqlite-ipc-srv
```

Note that the server expects a capability called `ipc`. This is the server-side
end of the communication channel that the server maintains to its clients.

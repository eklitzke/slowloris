# Slowpoke

Slowpoke is a program that helps you practice skills related to network
concurrency.

## Building

You'll need libevent installed. Then:

    ./autogen.sh
    ./configure
    make

The Slowpoke server will be in the `src/` directory, so you can find it after
building at `./src/slowpoke`.

## Running

You run the Slowpoke server like this:

    slowpoke -p PORT -t TIMEOUT_SECS

The default timeout is 1 seconds, and the default port is 9000.

## Protocol

Slowpoke uses asynchronous network I/O to handle a large number of connections.
The Slowpoke server sends messages to clients in the following format:

    SCORE TIMEOUT_SECS

After initiating a new connection the server will immediately send a message in
this format. The initial score is 0, so the first connection you initiate will
receive a line like this (although your timeout will likely be different):

    0 0.827368

This means the score is 0, and the timeout for this connection is 0.827368
seconds.

If you send any data whatsoever on this connection *before* the timeout elapses
the server will immediately terminate all connections and exit. It will print
the final score to stdout.

If you send any data whatsoever on the connection *after* the timeout elapses
the server will increment the score and send a new status line with a new
timeout. Therefore the second status line you see might be like this:

    1 0.333523

This would mean the score is now 1, and the new timeout is 0.333523 seconds.

After 10 seconds the server will terminate, regardless of whether you've
respected timeouts. The score will be printed to stdout, and the score will be
the same as the last score sent to the last connection that respected the
timeout rules. The 10 second timer starts only after the first connection is
made to the server.

The server doesn't actually care what data is sent by clients, or how many bytes
are sent. A good practice is to just have the client send the server a single
byte, but it should be OK to send larger buffers too.

## The Goal

The goal of a client is to try to maximize the score before the server exits.

For an extremely simple client you can create a regular blocking TCP connection.

For a more advanced client you can create multiple connections using
asynchronous I/O, threads, or any other concurrency mechanism. If you have
multiple connections to the server each will have its own timeout as described
in the protocol.

The server will concurrency up to the maximum number of file descriptors allowed
by the process.

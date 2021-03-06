# mx

C++11 coroutine await, yield, channels, i/o events (single header + link to boost).

This was originally part of my c++ util library [kit](https://github.com/flipcoder/kit), but I'm separating it.  I haven't
moved the net/socket stuff yet, but check out [kit's net.h sockets](https://github.com/flipcoder/kit/blob/master/kit/net/net.h) to see how to use this in that context.

It's "single header" but requires you link to some boost libs.

**This is a prototype.** There are still issues with stability and lack of features, maybe even thread safety. You've been warned!
This should serve as a proof-of-concept regardless.

Here are the basics: there is a singleton i/o multiplexer (i.e. "MX") which is initialized on first usage.
The number of circuits is by default set to your core count, but wraps back around beyond that.

You give the "circuit number" you wish to operate on.  This index is a hint on how the separate the tasks across
cores.  Because of the wraping of circuit indices, numbers that are closer together are more likely to be put on a different cores.

You can use local_shared_ptrs if you want, since each circuit has its own thread.

If you're familiar with Go, the mx's "coro" method (accessed by MX[0].coro\<void\> below) is similar to using "go".

```c++
// on circuit 0, launch coroutine, return future<void>
auto fut = MX[0].coro<void>([]{
    // do async stuff
    auto foo = MX_AWAIT(bar);

    // async sleep yield
    MX_SLEEP(chrono::milliseconds(100));
});

// use fut here

```

```c++
// on circuit 0, launch coroutine, using kit's sockets, which is also a prototype implementation
MX[0].coro<void>([&]{
    for(;;)
    {
        auto client = make_local_shared<TCPSocket>(MX_AWAIT(server->accept()));
        
        // coroutine per client
        MX[0].coro<void>([&, client]{
            int client_id = client_ids++;
            cout << "client " << client_id << " connected" << endl;
            try{
                for(;;)
                    MX_AWAIT(client->send(MX_AWAIT(client->recv())));
            }catch(const socket_exception& e){
            }
        });
    }
});

```

I don't have documentation of all features, check out mx.h if you're brave.

## Future

Currently, you can await more than one future in a circuit, but this causes a polling situation that requires a stabizing timer
to prevent wasting the CPU.  The next thing to add would be utilizing a shared method of polling (such as libuv)
and responding to user futures through callbacks instead of being polled.

## LICENSE

Open-source under MIT License.

See LICENSE file for details.

Copyright (c) Grady O'Connell, 2013



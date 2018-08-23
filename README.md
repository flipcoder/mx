# mx

C++ coroutine await, yield, channels, i/o events (single header + link to boost).

This was originally part of my c++ util library [kit](https://github.com/flipcoder/kit), but I'm separating it.  I haven't
moved the net/socket stuff yet, but check out [kit's net.h sockets](https://github.com/flipcoder/kit/blob/master/kit/net/net.h) to see how to use this in that context.

It's "single header" but requires you link to some boost libs.

**This is a prototype.** There are still issues with stability and lack of features, maybe even thread safety. You've been warned!
This should serve as a proof-of-concept regardless.

Here are the basics: there is a singleton i/o multiplexer (i.e. "MX") which is initialized on first usage.
The number of circuits is by default set to your core count, but wraps back around beyond that.

You give the "circuit number" you wish to operate on.  This index is a hint on how the separte the tasks across
cores.  Because of the wraping, numbers that are closer together are more likely to be put on a different core.

You can use local_shared_ptrs if you want, since these circuits operate on a single thread.

I don't have a way to awaken tasks on different circuits yet.

If you're familiar with Go, MX[0].coro\<void\> is similar to using "go" with
a void() lambda.

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

## Future

Currently, you can await one or more futures in a circuit, but this causes a polling situation that requires a stabizing timer
to prevent wasting the CPU.  Having a list of futures that respond to callbacks instead of being polled, would be the next thing
to implement if I were to continue this project.

## LICENSE

Open-source under MIT License.

See LICENSE file for details.

Copyright (c) Grady O'Connell, 2013


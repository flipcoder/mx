# mx

C++ coroutine await, yield, channels, i/o events (single header + link to boost)
    
This was originally part of my c++ util library [kit](https://github.com/flipcoder/kit), but I'm separating it.

It's "single header" but requires you link to some boost libs.

There may be some issues with stability atm, but this is a pretty cool proof of concept regardless.

Here are the basics: you have a singleton i/o multiplexer (i.e. "MX").
The number of circuits is, by default, set to your core count, but wraps back around when beyond that.

You give the "circuit number" you wish to operate on.  This index is a hint on how the separte the tasks across
cores.  Because of the wraping, Numbers that are closer together are more likely to be put on a different core.

You can use local smart ptrs if you want, since these circuits operate on a single thread.

I don't have a way to awaken tasks on different circuits yet.

If you're familiar with Go, MX[0].coro<void> is similar to using "go" with
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
// on circuit 0, launch coroutine
MX[0].coro<void>([&]{
    for(;;)
    {
        auto client = make_shared<TCPSocket>(MX_AWAIT(server->accept()));
        
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

## LICENSE

Open-source under MIT License.

See LICENSE file for details.

Copyright (c) Grady O'Connell, 2013


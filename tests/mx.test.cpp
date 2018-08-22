#include <catch.hpp>
#include "../mx/mx.h"
//#include "../mx/async/async.h"
//#include "../mx/async/async_fstream.h"
//#include "../include/mx/async/task.h"
//#include "../include/mx/async/.h"
//#include "../include/mx/async/multiplexer.h"
#include <atomic>
#include <vector>
#include <boost/thread.hpp>
#include <boost/chrono.hpp>
using namespace std;

#define LOCK_WAIT_MS 10

TEST_CASE("mx_task","[task]") {
    SECTION("empty task"){
        mx_task<void()> task([]{
            // empty
        });
        auto fut = task.get_future();
        task();
        REQUIRE(mx_ready(fut));
    }
    SECTION("retrying tasks"){
        mx_task<void(bool)> task([](bool err){
            // in non-coroutine contexts (task() instead of coro),
            //   MX_YIELD() recalls the function
            if(err)
                MX_YIELD();
        });
        auto fut = task.get_future();
        
        REQUIRE_THROWS(task(true));
        REQUIRE(fut.wait_for(std::chrono::seconds(0)) == 
            std::future_status::timeout);
        
        REQUIRE_NOTHROW(task(false));
        REQUIRE(mx_ready(fut));
    }
}

TEST_CASE("mx_channel","[]") {
    SECTION("basic usage"){
        mx_channel<int> chan;
        REQUIRE(chan.size() == 0);
        while(true){
            try{
                chan << 42;
                break;
            }catch(const mx_yield_exception& rt){}
        };
        REQUIRE(chan.size() == 1);
        int num = 0;
        while(true){
            try{
                chan >> num;
                break;
            }catch(const mx_yield_exception& rt){}
        };
        REQUIRE(num == 42);
    }
    SECTION("peeking, coroutines, explicit locking"){
        mx_io mx;
        auto chan = make_shared<mx_channel<int>>();
        auto cl = chan->lock();
        atomic<bool> started = ATOMIC_VAR_INIT(false);
        mx[0].coro<void>([&mx, chan, &started]{
            int x;
            for(int i=1;i<=3;++i)
            {
                MX_AWAIT_MX(mx, *chan << i);
            }
            started = true;
            for(int i=1;i<=3;++i)
            {
                //int n;
                //n = MX_AWAIT_MX(mx, chan->peek());
                //REQUIRE(n == i);
                MX_AWAIT_MX(mx, *chan >> x);
                //REQUIRE(x == i);
            }
        });
        REQUIRE(not started);
        boost::this_thread::sleep_for(boost::chrono::milliseconds(LOCK_WAIT_MS));
        REQUIRE(not started); // should still be awaiting lock

        // do something else in same , to prove coroutine is paused
        mx[0].task<void>([]{}).get();
        
        cl.unlock(); // give up our lock, so coroutine can resume
        while(not started) {
            boost::this_thread::sleep_for(boost::chrono::milliseconds(1));
        }
        REQUIRE(started); // intention of loop above
        mx.finish();
        REQUIRE(started);
    } 
    SECTION("retrying"){
        mx_io mx;
        mx_channel<int> chan;
        
        int sum = 0;
        mx[0].buffer(256);
        mx[0].task<void>([&chan]{
            // only way to stop this event is closing the  from the outside
            
            chan << 1; // retries task if this blocks
            
            MX_YIELD(); // keep this event going until chan is closed
        });
        mx[1].task<void>([&sum, &chan]{
            int num;
            chan >> num; // if this blocks, task is retried later
            sum += num;
            if(sum < 5)
                MX_YIELD();
            chan.close(); // done, triggers the other circuit to throw
        });
        mx.finish();
        REQUIRE(sum == 5);
    }
    SECTION("nested tasks"){
        mx_io mx;

        bool done = false;
        auto chan = make_shared<mx_channel<string>>();
        mx[0].task<void>([&mx, chan, &done]{
            auto ping = mx[0].task<void>([chan]{
                *chan << "ping";
            });
            auto pong = mx[0].task<string>([chan]{
                auto r = chan->get();
                r[1] = 'o';
                return r;
            });
            mx[0].when<void, string>(pong,[&done](future<string>& pong){
                auto str = pong.get();
                done = (str == "pong");
            });
        });
        mx.finish();
        REQUIRE(done);
    }
    SECTION("get_until") {
        mx_io mx;
        std::string msg = "hello world";
        size_t idx = 0;
        auto chan = make_shared<mx_channel<char>>();
        mx[0].task<void>([chan, &idx, &msg]{
            try{
                *chan << msg.at(idx);
                ++idx;
            }catch(const std::out_of_range&){
                return;
            }
            MX_YIELD();
        });
        auto result = mx[1].task<string>([chan]{
            return chan->get_until<string>(' ');
        });
        mx.finish();
        REQUIRE(result.get() == "hello");
    }
    SECTION("buffered streaming") {
        mx_io mx;
        std::string in = "12345";
        std::string out;
        //vector<char> in = {'1','2','3','4','5'};
        //vector<char> out;
        auto chan = make_shared<mx_channel<char>>();
        chan->buffer(3);
        {
            mx[0].task<void>([chan, &in]{
                try{
                    // usually this will continue after first chunk
                    //   but let's stop it early
                    //*chan << in;
                    chan->stream<string>(in);
                }catch(const mx_yield_exception&){
                    if(in.size() == 2) // repeat until 2 chars left
                        return;
                }
                MX_YIELD();
            });
            mx[1].task<void>([chan, &out]{
                //*chan >> out;
                chan->get<string>(out);
                if(out.size() == 3) // repeat until obtaining chars
                    return;
                MX_YIELD();
            });
        }
        mx.finish();
        REQUIRE(in == "45");
        REQUIRE(out == "123");
        //REQUIRE((in == vector<char>{'4','5'}));
        //REQUIRE((out == vector<char>{'1','2','3'}));
    }
}

//TEST_CASE("mx_taskQueue","[taskqueue]") {

//    SECTION("basic task queue") {
//        mx_taskQueue<void> tasks;
//        REQUIRE(!tasks);
//        REQUIRE(tasks.empty());
//        tasks([]{});
//        REQUIRE(!tasks.empty());
//    }
        
//    SECTION("run_once w/ futures") {
//        mx_taskQueue<int> tasks;
//        auto fut0 = tasks([]{
//            return 0;
//        });
//        auto fut1 = tasks([]{
//            return 1;
//        });
        
//        REQUIRE(tasks.size() == 2);
//        REQUIRE(tasks);
        
//        REQUIRE(fut0.wait_for(std::chrono::seconds(0)) == 
//            std::future_status::timeout);

//        tasks.run_once();
        
//        REQUIRE(fut0.get() == 0);
//        REQUIRE(fut1.wait_for(std::chrono::seconds(0)) == 
//            std::future_status::timeout);
//        REQUIRE(!tasks.empty());
//        REQUIRE(tasks.size() == 1);
        
//        tasks.run_once();
        
//        REQUIRE(fut1.get() == 1);
//        REQUIRE(tasks.empty());
//        REQUIRE(tasks.size() == 0);
//    }

//    SECTION("run_all"){
//        mx_taskQueue<void> tasks;
        
//        tasks([]{});
//        tasks([]{});
//        tasks([]{});
        
//        REQUIRE(tasks.size() == 3);
        
//        tasks.run();
        
//        REQUIRE(tasks.size() == 0);
//    }

//    SECTION("nested task enqueue"){
//        mx_taskQueue<void> tasks;
//        auto sum = make_shared<int>(0);
//        tasks([&tasks, sum]{
//            (*sum) += 1;
//            tasks([&tasks, sum]{
//                (*sum) += 10;
//            });
//        });
//        tasks.run();
//        REQUIRE(*sum == 11);
//        REQUIRE(sum.use_count() == 1);
//    }

//}

TEST_CASE("mx_io","[multiplexer]") {

    SECTION("thread wait on condition"){
        mx_io mx;
        std::atomic<int> num = ATOMIC_VAR_INIT(0);
        mx[0].task<void>([&num]{
            num = 42;
        });
        mx[1].when<void>(
            [&num]{return num == 42;},
            [&num]{num = 100;}
        );
        mx.finish();
        //while(num != 100){}
        REQUIRE(num == 100);
    }
    SECTION("thread wait on future"){
        mx_io mx;
        mx_task<int()> numbers([]{
            return 42;
        });
        auto fut = numbers.get_future();
        bool done = false;
        //numbers.run();
        //REQUIRE(fut.get() == 42);
        mx[0].when<void, int>(fut, [&done](std::future<int>& num){
            //done = true;
            done = (num.get() == 42);
        });
        numbers();
        mx.finish();
        REQUIRE(done);
    }
}

TEST_CASE("Coroutines","[coroutines]") {
    SECTION("Interleaved"){
        // In most apps, we'd use the singleton multiplexer "MX"
        //   and use MX_AWAIT() instead of MX_AWAIT_MX(mx, ...)
        // But since we want to isolate the multiplexer across unit tests
        //   we will declare a separate one here
        mx_io mx;
        
        // create an integer  
        auto chan = make_shared<mx_channel<int>>();
        
        // enforce context switching by assigning both tasks to same circuit
        //   and only allowing 1 integer across the  at once
        chan->buffer(1);
        const int C = 0;

        // schedule a coroutine to be our consumer
        auto nums_fut = mx[C].coro<vector<int>>([chan, &mx]{
            vector<int> nums;
            while(not chan->closed())
            {
                // recv some numbers from our 
                // MX_AWAIT() allows context switching instead of blocking
                int n = MX_AWAIT_MX(mx, chan->get());
                nums.push_back(n);
            }
            return nums;
        });
        // schedule a coroutine to be our producer of integers
        mx[C].coro<void>([chan, &mx]{
            // send some numbers through the 
            // MX_AWAIT() allows context switching instead of blocking
            MX_AWAIT_MX(mx, *chan << 1);
            MX_AWAIT_MX(mx, *chan << 2);
            MX_AWAIT_MX(mx, *chan << 3);
            chan->close();
        });

        mx.finish();

        // see if all the numbers got through the 
        REQUIRE((nums_fut.get() == vector<int>{1,2,3}));
    }
    SECTION("Exceptions and stack unwinding"){
        mx_io mx;
        
        struct UnwindMe {
            bool* unwound;
            UnwindMe(bool* b):
                unwound(b)
            {}
            ~UnwindMe() {
                *unwound = true;
            }
        };
        
        // TASK
        {
            auto unwound = mx_make_unique<bool>(false);
            bool* unwoundptr = unwound.get();
            auto fut = mx[0].task<void>([unwoundptr]{
                UnwindMe uw(unwoundptr);
                throw mx_interrupt(); // example exception
            });
            REQUIRE_THROWS(fut.get());
            REQUIRE(*unwound);
        }
        // COROUTINE
        {
            auto unwound = mx_make_unique<bool>(false);
            bool* unwoundptr = unwound.get();
            auto fut = mx[0].coro<void>([unwoundptr]{
                UnwindMe uw(unwoundptr);
                throw mx_interrupt(); // example exception
            });
            REQUIRE_THROWS(fut.get());
            REQUIRE(*unwound);
        }
        
        mx.finish();
    }
    SECTION("Stopping empty"){
        mx_io mx;
        mx.stop();
    }
    SECTION("Finishing empty"){
        mx_io mx;
        mx.finish();
    }
    SECTION("Stopping tasks"){
        mx_io mx;
        bool done = false;
        mx[0].task<void>([&mx, &done]{
            MX_YIELD_MX(mx); // retries func when not in coro (see task above)
            done = true;
        });
        boost::this_thread::sleep_for(boost::chrono::milliseconds(10));
        mx.stop();
        REQUIRE(done == false);
    }
    SECTION("Stopping coroutines"){
        mx_io mx;
        bool done = false;
        std::atomic<bool> ready = ATOMIC_VAR_INIT(false);
        mx[0].coro<void>([&mx, &ready, &done]{
            try{
                for(;;){
                    MX_YIELD_MX(mx);
                    ready = true;
                }
            }catch(...){
                done = true;
                throw;
            }
        });
        // wait for at least one yield to pass
        while(not ready){}
        // the coroutine should unwind, setting done to true
        mx.stop();
        REQUIRE(done == true);
    }
}

TEST_CASE("async_wrap","[async_wrap]") {
    SECTION("basic usage"){
        int num = 0;
        mx_async_wrap<int> val(42);
        REQUIRE(num != val.get());
        val.with<void>([&num](int& v){
            num = v;
        }).get();
        REQUIRE(num == 42);
        REQUIRE(num == val.get());
    }
}

//TEST_CASE("async_fstream","[async_fstream]") {
//    SECTION("basic usage"){
//        mx_io mx;
//        {
//            const std::string fn = "test.txt";
//            const std::string not_fn = "test_nonexist.txt";
//            async_fstream file(&mx[0]);
//            REQUIRE(not file.is_open().get());
//            file.open(fn).get();
//            REQUIRE(file.is_open().get());
//            REQUIRE(file.with<bool>([](fstream& f){
//                return f.is_open();
//            }).get() == true);
//            REQUIRE(file.filename().get() == fn);
//            //std::string buf = file.with<string>([](const std::string& b){
//            //    return b;
//            //}).get();
//            REQUIRE(file.buffer().get() == "test\n"); // contents of file
//            file.close().get();
//            REQUIRE(file.filename().get() == "");
            
//            // open behavior on non-existant file responds like fstream
//            file.open(not_fn).get();
//            REQUIRE(not file.is_open().get());

//            // failed opens still store file name
//            REQUIRE(not file.filename().get().empty());
//        }
//        mx.finish();
//    }
//}

TEST_CASE("Temp","[temp]") {
    SECTION("Some quick tests for debugging"){
        mx_io mx;
        mx.finish();
    }
}


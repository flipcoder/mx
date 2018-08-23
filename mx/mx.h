#ifndef MX_H_DY7UUXOY
#define MX_H_DY7UUXOY

#include <boost/thread.hpp>
#include <boost/noncopyable.hpp>
#include <boost/coroutine/all.hpp>
#include <boost/smart_ptr/make_local_shared.hpp>
#include <algorithm>
#include <atomic>
#include <deque>
#include <future>
#include <stdexcept>
#include <iostream>
#include <iterator>
#include <vector>
#include <queue>
#include <future>
#include <memory>
#include <utility>

#ifdef _MSC_VER
#include <iso646.h>
#endif

template<class T>
bool mx_ready(std::future<T>& fut)
{
    return fut.wait_for(std::chrono::seconds(0)) ==
        std::future_status::ready;
}

template<class T>
void mx_clear(T& t)
{
    t = T();
}

class mx_interrupt:
    public std::runtime_error
{
    public:
        mx_interrupt():
            std::runtime_error("interrupt")
        {}
        virtual ~mx_interrupt() throw() {}
};

template<class T, class... Args>
std::unique_ptr<T> mx_make_unique(Args&&... args)
{
    return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}

template<class Mutex=std::mutex>
class mx_mutexed
{
    public:

        mx_mutexed() = default;

        // never copy mutex data
        mx_mutexed(const mx_mutexed&) {}

        template<class Lock=std::unique_lock<Mutex>>
        Lock lock() const {
            return Lock(m_Mutex);
        }
        template<class Lock=std::unique_lock<Mutex>, class Strategy>
        Lock lock(Strategy strategy) const {
            return Lock(m_Mutex, strategy);
        }
        template<class Func>
        void with(Func cb){
            auto l = lock();
            cb();
        }

        //template<class Lock=std::unique_lock<Mutex>, class DeferStrategy>
        //std::optional<Lock> try_lock() {
        //}

        Mutex& mutex() const {
            return m_Mutex;
        }

    private:

        mutable Mutex m_Mutex;
};

template<
    class T,
    class Mutex=std::mutex
>
struct mx_mutex_wrap:
    mx_mutexed<Mutex>
{
    T data = T();

    mx_mutex_wrap():
        data(T())
    {}
    mx_mutex_wrap(T&& v):
        data(v)
    {}
    //mx_mutex_wrap(const T& v):
    //    data(std::forward(v))
    //{}

    T get() {
        auto l = this->lock();
        return data;
    }

    friend bool operator==(const mx_mutex_wrap& rhs, const T& lhs){
        auto l = rhs.lock();
        return rhs.data == lhs;
    }
    friend bool operator==(const T& rhs, const mx_mutex_wrap& lhs){
        auto l = lhs.lock();
        return lhs.data == rhs;
    }
    
    //mx_mutex_wrap& operator=(T&& rhs){
    //    auto l = this->lock();
    //    data = rhs;
    //    return *this;
    //}

    template<class R = void>
    R with(std::function<R(T&)> cb) {
        auto l = this->lock();
        return cb(data);
    }
    template<class R = void>
    R with(std::function<R(const T&)> cb) const {
        auto l = this->lock();
        return cb(data);
    }
    
    void set(T val){
        auto l = this->lock();
        data = val;
    }
};

class mx_yield_exception:
    public std::runtime_error
{
    public:
        mx_yield_exception():
            std::runtime_error("blocking operation requested yield")
        {}
        virtual ~mx_yield_exception() throw() {}
};


template<class R, class ...Args>
class mx_task;

template<class R, class ...Args>
class mx_task<R (Args...)>
{
    public:

        template<class ...T>
        explicit mx_task(T&&... t):
            m_Func(std::forward<T>(t)...)
        {}

        mx_task(mx_task&&) = default;
        mx_task& operator=(mx_task&&) = default;
        
        mx_task(const mx_task&) = delete;
        mx_task& operator=(const mx_task&) = delete;
        
        template<class ...T>
        void operator()(T&&... t) {
            try{
                m_Promise.set_value(m_Func(std::forward<T>(t)...));
            }catch(const mx_yield_exception& e){
                throw e;
            }catch(...){
                m_Promise.set_exception(std::current_exception());
            }
            //}catch(...){
            //    assert(false);
            //}
        }

        std::future<R> get_future() {
            return m_Promise.get_future();
        }

    private:

        std::function<R(Args...)> m_Func;
        std::promise<R> m_Promise;
};

template<class ...Args>
class mx_task<void(Args...)>
{
    public:

        template<class ...T>
        explicit mx_task(T&&... t):
            m_Func(std::forward<T>(t)...)
        {}

        mx_task(mx_task&&) = default;
        mx_task& operator=(mx_task&&) = default;
        
        mx_task(const mx_task&) = delete;
        mx_task& operator=(const mx_task&) = delete;
        
        template<class ...T>
        void operator()(T&&... t) {
            try{
                m_Func(std::forward<T>(t)...);
                m_Promise.set_value();
            //}catch(const boost::coroutines::detail::forced_unwind& e){
            //    throw e;
            }catch(const mx_yield_exception& e){
                throw e;
            }catch(...){
                m_Promise.set_exception(std::current_exception());
            }
        }

        std::future<void> get_future() {
            return m_Promise.get_future();
        }

    private:

        std::function<void(Args...)> m_Func;
        std::promise<void> m_Promise;

};

#define MX mx_io::get()

#ifndef MX_THREADS
#define MX_THREADS 0
#endif

#ifndef MX_FREQ
#define MX_FREQ 0
#endif

#ifndef CACHE_LINE_SIZE
#define CACHE_LINE_SIZE 64
#endif

typedef boost::coroutines::coroutine<void>::pull_type pull_coro_t;
typedef boost::coroutines::coroutine<void>::push_type push_coro_t;

// async await a future (continously mx_yield until future is available or exception)
#define MX_AWAIT_MX(MUX, EXPR) \
    [&]{\
        while(true){\
            try{\
                return (EXPR);\
            }catch(const mx_yield_exception&){\
                MUX.yield();\
            }\
        }\
    }()
#define MX_AWAIT(EXPR) MX_AWAIT_MX(MX, EXPR)

// async await a condition (continously mx_yield until condition is true)
#define MX_YIELD_WHILE_MX(MUX, EXPR) \
    [&]{\
        while((EXPR)){\
            MUX.yield();\
        }\
    }()
#define MX_YIELD_WHILE(EXPR) MX_YIELD_WHILE_MX(MX, EXPR)
#define MX_YIELD_UNTIL_MX(MUX, EXPR) MX_YIELD_WHILE_MX(MUX, not (EXPR))
#define MX_YIELD_UNTIL(EXPR) MX_YIELD_WHILE_MX(MX, not (EXPR))

#define MX_YIELD_MX(MUX) MUX.yield();
#define MX_YIELD() MX_YIELD_MX(MX)

#define MX_AWAIT_HINT_MX(MUX, HINT, EXPR) \
    [&]{\
        bool once = false;\
        while(true)\
        {\
            try{\
                return (EXPR);\
            }catch(const mx_yield_exception&){\
                if(not once) {\
                    MUX.this_circuit().this_unit().cond = [=]{\
                        return (HINT);\
                    };\
                    once = true;\
                }\
                MUX.yield();\
            }\
            if(once)\
            {\
                mx_clear(MUX.this_circuit().this_unit().cond);\
            }\
        }\
    }()

// TODO: deduce type?
//#define MX_GO(ID,TYPE) MX[ID].coro<TYPE>

#define MX_AWAIT_HINT(HINT, EXPR) MX_AWAIT_HINT_MX(MUX, HINT, EXPR)

// coroutine async sleep()
#define MX_SLEEP(TIME) mx_io::sleep(TIME);

// deprecated
//#define SLEEP(TIME) mx_io::sleep(TIME);

#define MX_EPSILON 0.00001

class mx_io
{
    public:
        
        // singleton
        static mx_io& get()
        {
            static mx_io instance;
            return instance;
        }


        struct Unit
        {
            Unit(
                std::function<bool()> rdy,
                std::function<void()> func,
                std::unique_ptr<push_coro_t>&& push,
                pull_coro_t* pull
            ):
                m_Ready(rdy),
                m_Func(func),
                m_pPush(std::move(push)),
                m_pPull(pull)
            {}
            
            Unit(
                std::function<bool()> rdy,
                std::function<void()> func
            ):
                m_Ready(rdy),
                m_Func(func)
            {}

            bool is_coroutine() const {
                return m_pPull;
            }
            
            // only a hint, assume ready if functor is 'empty'
            std::function<bool()> m_Ready; 
            mx_task<void()> m_Func;
            std::unique_ptr<push_coro_t> m_pPush;
            pull_coro_t* m_pPull = nullptr;
            // TODO: idletime hints for load balancing?
        };

        class Circuit:
            //public IAsync,
            public mx_mutexed<boost::mutex>
        {
        public:
            
            Circuit(mx_io* mx, unsigned idx):
                m_pMX(mx),
                m_Index(idx)
            {
                run();
            }
            virtual ~Circuit() {}

            void yield() {
                if(m_pCurrentUnit && m_pCurrentUnit->m_pPull)
                    (*m_pCurrentUnit->m_pPull)();
                else
                    throw mx_yield_exception();
                //else
                //    boost::this_thread::yield();
            }
            
            template<class T = void>
            std::future<T> when(std::function<bool()> cond, std::function<T()> cb) {
                while(true) {
                    boost::this_thread::interruption_point();
                    {
                        auto l = lock();
                        if(!m_Buffered || m_Units.size() < m_Buffered) {
                            auto cbt = mx_task<T()>(std::move(cb));
                            auto fut = cbt.get_future();
                            auto cbc = boost::make_local_shared<mx_task<T()>>(std::move(cbt));
                            m_Units.emplace_back(cond, [cbc]() {
                                (*cbc)();
                            });
                            m_CondVar.notify_one();
                            return fut;
                        }
                    }
                    boost::this_thread::yield();
                }
            }

            template<class T=void>
            std::future<T> coro(std::function<T()> cb) {
                while(true) {
                    boost::this_thread::interruption_point();
                    {
                        auto l = lock();
                        if(!m_Buffered || m_Units.size() < m_Buffered) {
                            auto cbt = mx_task<T()>(std::move(cb));
                            auto fut = cbt.get_future();
                            auto cbc = boost::make_local_shared<mx_task<T()>>(std::move(cbt));
                            m_Units.emplace_back(
                                std::function<bool()>(),
                                std::function<void()>()
                            );
                            auto* pullptr = &m_Units.back().m_pPull;
                            m_Units.back().m_pPush = mx_make_unique<push_coro_t>(
                                [cbc, pullptr](pull_coro_t& sink){
                                    *pullptr = &sink;
                                    (*cbc)();
                                }
                            );
                            auto* coroptr = m_Units.back().m_pPush.get();
                            m_Units.back().m_Ready = std::function<bool()>(
                                [coroptr]() -> bool {
                                    return bool(*coroptr);
                                }
                            );
                            m_Units.back().m_Func = mx_task<void()>(std::function<void()>(
                                [coroptr]{
                                    (*coroptr)();
                                    if(*coroptr) // not completed?
                                        throw mx_yield_exception(); // continue
                                }
                            ));
                            m_CondVar.notify_one();
                            return fut;
                        }
                    }
                    boost::this_thread::yield();
                }

            }
            
            template<class T = void>
            std::future<T> task(std::function<T()> cb) {
                return when(std::function<bool()>(), cb);
            }
            
            // TODO: handle single-direction channels that may block
            //template<class T>
            //std::shared_ptr<mx_channel<T>> channel(
            //    std::function<void(std::shared_ptr<mx_channel<T>>)> worker
            //) {
            //    auto chan = boost::make_shared<mx_channel<>>();
            //    // ... inside lambda if(chan->closed()) remove?
            //}
            
            // TODO: handle multi-direction channels that may block

            template<class R, class T>
            std::future<R> when(std::future<T>& fut, std::function<R(std::future<T>&)> cb) {
                auto futc = boost::make_local_shared<std::future<T>>(std::move(fut));
                
                return task<void>([cb, futc]() {
                    if(futc->wait_for(std::chrono::seconds(0)) ==
                        std::future_status::ready)
                    {
                        cb(*futc);
                    }
                    else
                        throw mx_yield_exception();
                });
            }

            void run() {
                m_Thread = boost::thread([this]{
                    m_pMX->m_ThreadToCircuit.with<void>(
                        [this](std::map<boost::thread::id, unsigned>& m){
                            m[boost::this_thread::get_id()] = m_Index;
                        }
                    );
                    unsigned idx = 0;
                    try{
                        while(next(idx)){}
                    }catch(const boost::thread_interrupted&){
                        m_Units.clear(); // this will unwind coros immediately
                    }
                });
                //#ifdef __WIN32__
                //    SetThreadPriority(m_Thread.native_handle(), THREAD_PRIORITY_BELOW_NORMAL);
                //#endif
            }
            //void forever() {
            //    if(m_Thread.joinable()) {
            //        m_Thread.join();
            //    }
            //}
            void finish_nojoin() {
                m_Finish = true;
                {
                    auto l = this->lock<boost::unique_lock<boost::mutex>>();
                    m_CondVar.notify_one();
                }
            }
            void join() {
                if(m_Thread.joinable())
                    m_Thread.join();
            }
            void finish() {
                m_Finish = true;
                {
                    auto l = this->lock<boost::unique_lock<boost::mutex>>();
                    m_CondVar.notify_one();
                }
                if(m_Thread.joinable()) {
                    m_Thread.join();
                }
            }
            void stop() {
                if(m_Thread.joinable()) {
                    m_Thread.interrupt();
                    {
                        auto l = this->lock<boost::unique_lock<boost::mutex>>();
                        m_CondVar.notify_one();
                    }
                    m_Thread.join();
                }
            }
            bool empty() const {
                auto l = this->lock<boost::unique_lock<boost::mutex>>();
                return m_Units.empty();
            }
            void sync() {
                while(true){
                    if(empty())
                        return;
                    boost::this_thread::yield();
                };
            }
            size_t buffered() const {
                auto l = lock();
                return m_Buffered;
            }
            void unbuffer() {
                auto l = lock();
                m_Buffered = 0;
            }
            void buffer(size_t sz) {
                auto l = lock();
                m_Buffered = sz;
            }

            //virtual bool poll_once() override { assert(false); }
            //virtual void run() override { assert(false); }
            //virtual void run_once() override { assert(false); }
            
            Unit* this_unit() { return m_pCurrentUnit; }
            
            // expressed in maximum acceptable ticks per second when idle
            void frequency(float freq) {
                auto lck = this->lock<boost::unique_lock<boost::mutex>>();
                m_Frequency = freq;
            }
            
        private:
            
            void stabilize()
            {
                // WARNING: lock is assumed
                if(m_Frequency <= MX_EPSILON)
                    return; // no stabilization
                const float inv_freq = 1.0f / m_Frequency;
                if(m_Clock != std::chrono::time_point<std::chrono::system_clock>())
                {
                    while(true)
                    {
                        auto now = std::chrono::system_clock::now();
                        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>
                            (now - m_Clock).count() * 0.001f;
                        if(elapsed < inv_freq) // in seconds
                        {
                            auto lck = this->lock<boost::unique_lock<boost::mutex>>(boost::defer_lock);
                            if(not lck.try_lock())
                                break;
                            m_CondVar.timed_wait(lck, boost::posix_time::milliseconds(
                                int((inv_freq - elapsed)*1000.0f)
                            ));
                        }
                        else
                            break;
                    }
                }
                m_Clock = std::chrono::system_clock::now();
            }
            
            // returns false only on empty() && m_Finish
            virtual bool next(unsigned& idx) {
                auto lck = this->lock<boost::unique_lock<boost::mutex>>();
                //if(l.try_lock())
                // wait until task queued or thread interrupt
                while(true){
                    boost::this_thread::interruption_point();
                    if(not m_Units.empty())
                        break;
                    else if(m_Finish) // finished AND empty
                        return false;
                    m_CondVar.wait(lck);
                    boost::this_thread::yield();
                    continue;
                }
                
                if(idx >= m_Units.size()) {
                    stabilize();
                    idx = 0;
                }
                
                auto& task = m_Units[idx];
                if(!task.m_Ready || task.m_Ready()) {
                    lck.unlock();
                    m_pCurrentUnit = &m_Units[idx];
                    try{
                        task.m_Func();
                    }catch(const mx_yield_exception&){
                        lck.lock();
                        const size_t sz = m_Units.size();
                        idx = std::min<unsigned>(idx+1, sz);
                        if(idx == sz) {
                            stabilize();
                            idx = 0;
                        }
                        return true;
                    }
                    m_pCurrentUnit = nullptr;
                    lck.lock();
                    m_Units.erase(m_Units.begin() + idx);
                }
                return true;
            }
            
            Unit* m_pCurrentUnit = nullptr;
            std::deque<Unit> m_Units;
            boost::thread m_Thread;
            size_t m_Buffered = 0;
            std::atomic<bool> m_Finish = ATOMIC_VAR_INIT(false);
            mx_io* m_pMX;
            unsigned m_Index=0;
            boost::condition_variable m_CondVar;
            std::chrono::time_point<std::chrono::system_clock> m_Clock;
            float m_Frequency = 1.0f * MX_FREQ;
        };
        
        friend class Circuit;
        
        mx_io(bool init_now=true):
            m_Concurrency(std::max<unsigned>(1U,
                MX_THREADS ? MX_THREADS :
                    boost::thread::hardware_concurrency()
            ))
        {
            if(init_now)
                init();
        }
        virtual ~mx_io() {
            finish();
        }
        void init() {
            for(unsigned i=0;i<m_Concurrency;++i)
                m_Circuits.emplace_back(make_tuple(
                    mx_make_unique<Circuit>(this, i), CacheLinePadding()
                ));
            //m_Multicircuit = mx_make_unique<Circuit>(this, i);
        }
        //void join() {
        //    for(auto& s: m_Circuits)
        //        s.join();
        //}
        void stop() {
            for(auto& s: m_Circuits)
                std::get<0>(s)->stop();
        }

        Circuit& any_circuit(){
            // TODO: load balancing would be nice here
            return *std::get<0>((m_Circuits[std::rand() % m_Concurrency]));
        }
        
        Circuit& circuit(unsigned idx) {
            return *std::get<0>((m_Circuits[idx % m_Concurrency]));
        }
        Circuit& operator[](unsigned idx) {
            return *std::get<0>((m_Circuits[idx % m_Concurrency]));
        }
        
        void yield(){
            try{
                this_circuit().yield();
            }catch(const std::out_of_range&){
                throw mx_yield_exception();
            }
        }

        static void sleep(std::chrono::milliseconds ms) {
            if(ms == std::chrono::milliseconds(0))
                return;
            auto t0 = std::chrono::steady_clock::now();
            while(true) {
                auto t1 = std::chrono::steady_clock::now();
                if(std::chrono::duration_cast<std::chrono::milliseconds>(
                    t1 - t0
                ) >= ms) {
                    break;
                }
                MX_YIELD();
            }
        }
        
        Circuit& this_circuit(){
            return *m_ThreadToCircuit.with<Circuit*>(
                [this](std::map<boost::thread::id, unsigned>& m
            ){
                return &circuit(m.at(boost::this_thread::get_id()));
            });
        }

        size_t size() const {
            return m_Concurrency;
        }

        void finish() {
            for(auto& s: m_Circuits)
                std::get<0>(s)->finish_nojoin();
            for(auto& s: m_Circuits)
                std::get<0>(s)->join();
            //for(auto& s: m_Multicircuit)
            //    std::get<0>(s)->finish_nojoin();
            //for(auto& s: m_Multicircuit)
            //    std::get<0>(s)->join();
        }
        
        //template <class Time>
        //static bool retry(int count, Time delay, std::function<bool()> func)
        //{
        //    for(int i=0; i<count || count<0; count<0 || ++i)
        //    {
        //        if(func())
        //            return true;
        //        SLEEP(delay);
        //    }
        //    return false;
        //}
        
    private:

        struct CacheLinePadding
        {
            volatile int8_t pad[CACHE_LINE_SIZE * 2];
        };

        const unsigned m_Concurrency;
        std::vector<std::tuple<std::unique_ptr<Circuit>, CacheLinePadding>> m_Circuits;
        //std::unique_ptr<Circuit> m_Multicircuit;

        // read-write mutex might be more optimal here
        mx_mutex_wrap<std::map<boost::thread::id, unsigned>> m_ThreadToCircuit;
};

template<class T, class Mutex=std::mutex>
class mx_channel:
    public mx_mutexed<Mutex>
    //public std::enable_shared_from_this<mx_channel<T>>
{
    public:

        virtual ~mx_channel() {}

        // Put into stream
        void operator<<(T val) {
            auto l = this->lock(std::defer_lock);
            if(!l.try_lock())
                throw mx_yield_exception();
            if(m_bClosed)
                throw std::runtime_error("channel closed");
            if(!m_Buffered || m_Vals.size() < m_Buffered)
            {
                m_Vals.push_back(std::move(val));
                m_bNewData = true;
                return;
            }
            throw mx_yield_exception();
        }
        template<class Buffer=std::vector<T>>
        void stream(Buffer& vals) {
            auto l = this->lock(std::defer_lock);
            if(!l.try_lock())
                throw mx_yield_exception();
            if(m_bClosed)
                throw std::runtime_error("channel closed");
            size_t capacity = 0;
            size_t buflen = vals.size();
            bool partial = false;
            if(m_Buffered)
            {
                capacity = m_Buffered - m_Vals.size();
                if(buflen > capacity)
                {
                    buflen = capacity;
                    partial = true;
                }
            }
            if(buflen)
            {
                m_Vals.insert(m_Vals.end(),
                    make_move_iterator(vals.begin()),
                    make_move_iterator(vals.begin() + buflen)
                );
                vals.erase(vals.begin(), vals.begin() + buflen);
                m_bNewData = true;
                if(partial)
                    throw mx_yield_exception();
                return;
            }
            throw mx_yield_exception();
        }
        void operator<<(std::vector<T>& vals) {
            stream(vals);
        }

        //void operator<<(std::vector<T> val) {
        //    auto l = this->lock(std::defer_lock);
        //    if(!l.try_lock())
        //        throw mx_yield_exception();
        //    if(m_bClosed)
        //        throw std::runtime_error("channel closed");
        //    if(!m_Buffered || m_Vals.size() < m_Buffered) {
        //        m_Vals.push_back(std::move(val));
        //        m_bNewData = true;
        //        return;
        //    }
        //    throw mx_yield_exception();
        //}
        
        // Get from stream
        void operator>>(T& val) {
            if(not m_bNewData)
                throw mx_yield_exception();
            auto l = this->lock(std::defer_lock);
            if(!l.try_lock())
                throw mx_yield_exception();
            //if(m_bClosed)
            //    throw std::runtime_error("channel closed");
            //m_bNewData = false;
            if(!m_Vals.empty())
            {
                val = std::move(m_Vals.front());
                m_Vals.pop_front();
                if(m_Vals.empty())
                    m_bNewData = false;
                return;
            }
            throw mx_yield_exception();
        }
        void operator>>(std::vector<T>& vals) {
            get(vals);
        }
        template<class Buffer=std::vector<T>>
        void get(Buffer& vals) {
            if(not m_bNewData)
                throw mx_yield_exception();
            auto l = this->lock(std::defer_lock);
            if(!l.try_lock())
                throw mx_yield_exception();
            //if(m_bClosed)
            //    throw std::runtime_error("channel closed");
            m_bNewData = false;
            if(!m_Vals.empty())
            {
                vals.insert(vals.end(),
                    make_move_iterator(m_Vals.begin()),
                    make_move_iterator(m_Vals.end())
                );
                m_Vals.clear();
                return;
            }
            throw mx_yield_exception();
        }
        template<class Buffer=std::vector<T>>
        Buffer get() {
            Buffer buf;
            get(buf);
            return buf;
        }

        T peek() {
            if(not m_bNewData)
                throw mx_yield_exception();
            auto l = this->lock(std::defer_lock);
            if(!l.try_lock())
                throw mx_yield_exception();
            //if(m_bClosed)
            //    throw std::runtime_error("channel closed");
            if(!m_Vals.empty())
                return m_Vals.front();
            throw mx_yield_exception();
        }
        
        // NOTE: full buffers with no matching tokens will never
        //       trigger this
        template<class R=std::vector<T>>
        R get_until(T token) {
            if(not m_bNewData)
                throw mx_yield_exception();
            auto l = this->lock(std::defer_lock);
            if(!l.try_lock())
                throw mx_yield_exception();
            //if(m_bClosed)
            //    throw std::runtime_error("channel closed");
            for(size_t i=0;i<m_Vals.size();++i)
            {
                if(m_Vals[i] == token)
                {
                    R r(make_move_iterator(m_Vals.begin()),
                        make_move_iterator(m_Vals.begin() + i));
                    m_Vals.erase(
                        m_Vals.begin(),
                        m_Vals.begin() + i + 1
                    );
                    return r;
                }
            }
            throw mx_yield_exception();
        }
        T get() {
            if(not m_bNewData)
                throw mx_yield_exception();
            auto l = this->lock(std::defer_lock);
            if(!l.try_lock())
                throw mx_yield_exception();
            m_bNewData = 0;
            //if(m_bClosed)
            //    throw std::runtime_error("channel closed");
            if(!m_Vals.empty()) {
                auto r = std::move(m_Vals.front());
                m_Vals.pop_front();
                return r;
            }
            throw mx_yield_exception();
        }

        //operator bool() const {
        //    return m_bClosed;
        //}
        bool ready() const {
            return m_bNewData;
        }
        bool empty() const {
            auto l = this->lock();
            return m_Vals.empty();
        }
        size_t size() const {
            auto l = this->lock();
            return m_Vals.size();
        }
        size_t buffered() const {
            auto l = this->lock();
            return m_Buffered;
        }
        void unbuffer() {
            auto l = this->lock();
            m_Buffered = 0;
        }
        void buffer(size_t sz) {
            auto l = this->lock();
            //if(sz > m_Buffered)
            //    m_Vals.reserve(sz);
            m_Buffered = sz;
        }
        void close() {
            // atomic
            m_bClosed = true;
        }
        bool closed() const {
            // atomic
            return m_bClosed;
        }

    private:
        
        size_t m_Buffered = 0;
        std::deque<T> m_Vals;
        std::atomic<bool> m_bClosed = ATOMIC_VAR_INIT(false);
        std::atomic<bool> m_bNewData = ATOMIC_VAR_INIT(false);
};


template<class T>
class mx_async_wrap
{
private:
    
    mx_io::Circuit* const m_pCircuit;
    T m_Data;
    
public:

    mx_async_wrap(mx_io::Circuit* circuit = &MX[0]):
        m_pCircuit(circuit),
        m_Data(T())
    {}
    mx_async_wrap(T&& v, mx_io::Circuit* circuit = &MX[0]):
        m_pCircuit(circuit),
        m_Data(v)
    {}
    mx_async_wrap(const T& v, mx_io::Circuit* circuit = &MX[0]):
        m_pCircuit(circuit),
        m_Data(std::forward(v))
    {}
    template<class R = void>
    std::future<R> with(std::function<R(T&)> cb) {
        return m_pCircuit->task<R>([this, cb]{
            return cb(m_Data);
        });
    }
    template<class R = void>
    std::future<R> with(std::function<R(const T&)> cb) const {
        return m_pCircuit->task<R>([this, cb]{
            return cb(m_Data);
        });
    }
    
    // blocks until getting a copy of the wrapped m_Data
    T operator*() const {
        return get();
    }
    T get() const {
        return m_pCircuit->task<T>([this]{
            return m_Data;
        }).get();
    }

    const mx_io::Circuit& circuit() const { return *m_pCircuit; }
};


#endif


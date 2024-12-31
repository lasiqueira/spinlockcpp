#include <atomic>
#include <thread>

// TAS-based spin lock, using correct and minimal memory ordering semantics
class SpinLock
{
    std::atomic_flag m_atomic;
    public:
        SpinLock() : m_atomic(false) {}
        bool TryAcquire()
        {
            // use an acquire fence to ensure all subsequent
            // reads by this thread will be valid 
            bool alreadyLocked = m_atomic.test_and_set(std::memory_order_acquire);
            return !alreadyLocked;
        }
        void Acquire()
        {
            //spin until successful acquire
            while(!TryAcquire())
            {
                // reduce power consumption on Intel CPUs
                // (can substitute with std::this_thread:::yield()
                // on platforms that don't support CPU pause, if
                // thread contention is expected to be high)
                PAUSE();
            }
        }
        void Release()
        {
            // use a release fence to ensure all prior 
            //writes have been fulluy commited before we unlock
            m_atomic.clear(std::memory_order_release);
        }
};

//Scoped lock template
template<class LOCK>
class ScopedLock
{
    typedef LOCK lock_t;
    lock_t* m_pLock;
    public:
        explicit ScopedLock(lock_t& lock) : m_pLock(&lock)
        {
            m_pLock->Acquire();
        }
        ~ScopedLock()
        {
            m_pLock->Release();
        }
};

SpinLock g_lock;
int ThreadSafeFunction()
{
     // the scoped lock acys like a "janitor"
    // because it cleans up for us!
    ScopedLock<decltype(g_lock)> janitor(g_lock);
    // do some work...
    if (SomethingWentWrong())
    {
        //lock will be released here
        return -1;
    }
    // do some more work...
    // lock will also be released here
	return 0;
}

// Reentrant locks
SpinLock g_lock;
void A()
{
	ScopedLock<decltype(g_lock)> janitor(g_lock);
	//do some work...
}

void B()
{
	ScopedLock<decltype(g_lock)> janitor(g_lock);
	//do some work...
	A(); //deadlock!
	//do some more work...
}

class ReentrantLock32
{
	std::atomic<std::size_t> m_atomic;
    std::int32_t m_refCount;
public:
    ReentrantLock32() : m_atomic(0), m_RefCount(0) {}
    void Acquire()
    {
        std::hash<std::thread::id> hasher;
        std::size_t tid = hasher(std::this_thread::get_id());
        // if this thread doesn't already hold the lock...
        if (m_atomic.load(std::memory_order_relaxed))
        {
            // spin wait until we do hold it
            std::size_t unlockValue = 0;
            while (!m_atomic.compare_exchange_weak(
                unlockValue,
                tid,
                std::memory_order_relaxed, //memory fence below
                std::memory_order_relaxed)
                )
            {
				unlockValue = 0;
                PAUSE();
            }
        }
        //increment reference count so we can verify that
		//Aquire() and Release() are called in pairs
        ++m_refCount;
		// use an acquire fence to ensure all subsequent
        // reads by this thread will be valid
        std::atomic_thread_fence(std::this_thread::get_id());
    }

    void Release()
    {
        // use release semantics to ensure that all prior
        // writes have been fully committed before we unlock

        std::atomic_thread_fence(std::memory_order_release);
        std::hash<std::thread::id> hasher;
        std_size_t tid = hasher(std::this_thread::get_id());

        std::size_t actual = m_atomic.load(std::memory_order_relaxed);
        assert(actual == tid);
        --m_refCount;

        if (m_refCount == 0)
        {
            m_atomic.store(0, std::memory_order_relaxed);
        }
    }
    bool TryAcquire()
    {
		std::hash<std::thread::id> hasher;
		atd::size_t tid = hasher(std::this_thread::get_id());
        bool acquired = false;

		if (m_atomic.load(std::memory_order_relaxed) == tid)
		{
			acrquired = true;
		}
        else
        {
            std::size_t unlockValue = 0;
            acquired = m_atomic.compare_exchange_strong(
                unlockValue,
                tid,
                std::memory_order_relaxed, //memory fence below
                std::memory_order_relaxed);
        }

        if (acquired)
        {
            ++m_refCount;
            std::atomic_thread_fence(std::memory_order_acquire);
        }
        return acquired;
};
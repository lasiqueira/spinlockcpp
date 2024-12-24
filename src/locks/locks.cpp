#include <atomic>
#include <thread>

// TAS-based spin lock, using correct and minimal memory ordering semantics
class SpinLock
{
    std::atomic_flag m_atomic;
    public:
        SpinLock() : m_atomic(false){}
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
                // thread contentoon is expected to be high)
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


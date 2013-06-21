/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file	zypp/base/InterProcessMutex.h
 *
*/

#ifndef ZYPP_BASE_INTER_PROCESS_MUTEX_H
#define ZYPP_BASE_INTER_PROCESS_MUTEX_H

#include <boost/date_time/posix_time/posix_time.hpp>

#include "zypp/base/PtrTypes.h"
#include "zypp/base/Exception.h"
#include "zypp/Callback.h"
#include "zypp/Pathname.h"

///////////////////////////////////////////////////////////////////
namespace zypp
{
  ///////////////////////////////////////////////////////////////////
  namespace base
  {
    struct InterProcessLockReport;
    struct InterProcessLockException;

    ///////////////////////////////////////////////////////////////////
    /// \class InterProcessMutex
    /// \brief Wrapper around boost::interprocess::file_lock mutex
    ///
    /// Mutex to synchronise filesystem access across different
    /// processes (not threads!) using libzypp. Used e.g. in class
    /// \ref IPMutex.
    ///
    /// The underlying mutex is created per pathname on demand goes out
    /// of scope if the last \ref InterProcessMutex drops it's reference.
    ///
    /// \note The mutex file must exist!
    ///
    /// \note The mutex is \b not \b upgradable! Switching from \c SHARED_LOCK
    /// to \c EXCLUSIVE_LOCK state is not atomic. The mutex may need to unlock
    /// before regaining the EXCLUSIVE_LOCK in order to avoid a deadlock.
    ///
    /// While waiting for a lock to become available via \ref lock or \ref lockSharable,
    /// the \ref InterProcessLockReport callback is triggered regulary (every 3 seconds).
    /// If the lock can not be obtained within 180 seconds or the callback aborts waiting,
    /// an \ref InterProcessLockException is thrown. You can set the environment variable
    /// \c $ZYPP_MAX_LOCK_WAIT to adjust the maximum time waiting for a lock. Set it to 0 to
    /// wait forever.
    ///
    /// \see \ref SharableLock and \ref ScopedLock
    ///
    /// \see http://www.boost.org/doc/html/boost/interprocess/file_lock.html
    ///////////////////////////////////////////////////////////////////
    class InterProcessMutex
    {
      friend std::ostream & operator<<( std::ostream & str, const InterProcessMutex & obj );
    public:
      typedef unsigned sec_type;

      /** Type to indicate to the mutex constructor it should fake locking. */
      struct FakeLockType {};
      /** An object indicating that mutex operation must be faked. */
      static constexpr FakeLockType fakeLock = FakeLockType();
      /** A  virtual pathname (\c "/<fakeloc>") indicating that mutex operation must be faked. */
      static Pathname fakeLockPath();

      /** Type to indicate to a mutex lock constructor that it must not lock the mutex. */
      struct DeferLockType {};
      /** An object indicating that lock operation must be defered. */
      static constexpr DeferLockType deferLock = DeferLockType();

      /** Type to indicate to a mutex lock constructor that it must try to lock the mutex. */
      struct TryToLockType {};
      /** An object indicating that a tryLock() operation must be executed. */
      static constexpr TryToLockType tryToLock = TryToLockType();

    public:
      /** Default Ctor (use as placeholder only) */
      InterProcessMutex();

      /** Fake mutex (no locking at all) */
      InterProcessMutex( FakeLockType );

      /** Ctor creating mutex for \a path_r
       * \note Passing \ref fakeLockPath as argument will fake the mutex (no locking at all).
       * \throws boost::interprocess::interprocess_exception if \a path_r does not exist or is not read/writable.
       */
      explicit InterProcessMutex( const Pathname & path_r );

    public:
      /** Representing the mutex internal state */
      enum State
      { UNLOCKED, SHARED_LOCK, EXCLUSIVE_LOCK };

      /** Return the mutex internal state */
      State state() const;

      /** The underlying mutex file (for logging) */
      Pathname mutexFile() const;

      /** Conversion to bool: Whether a underlying mutex is available (i.e. not default constructed) */
      explicit operator bool() const
      { return bool(_pimpl); }

      /** Whether locking is faked. */
      bool isFakeLock() const
      { return mutexFile() == fakeLockPath(); }

    private:
      /** Base class to aquire and automatically release a lock. */
      template<InterProcessMutex::State _targetState> class Lock;

    public:
      /** Aquire and automatically release a sharable lock. */
      typedef Lock<SHARED_LOCK> SharableLock;

      /** Aquire and automatically release an exclusive lock. */
      typedef Lock<EXCLUSIVE_LOCK> ScopedLock;

      /** Callback sending keepalive while waiting to obtain a lock. */
      typedef InterProcessLockReport LockReport;

      /** Base class for lock exceptions. */
      typedef InterProcessLockException LockException;

    public:
      /** \name Exclusive locking */
      //@{
	/** Wait until a lock was obtained (trigger \ref InterProcessLockReport while waiting).
	 * \throws InterProcessLockException upon timeout or if aborted by callback. Mutex
	 * was set to UNLOCKED state to avoid deadlock.
	 */
	void lock();
	/** Wait until a lock was obtained (no callback or timeout).
	 *  \see \ref lock)
	 */
	void sleepLock();
	/** Try to obtain a lock immediately (do not wait). */
	bool tryLock();
	/** Try to obtain a lock before \a abs_time is reached. */
	bool timedLock( const boost::posix_time::ptime & absTime_r );
	/** Try to obtain a lock within \a seconds_r seconds. */
	bool waitLock( sec_type seconds_r )
	{ return timedLock( wait( seconds_r ) ); }
	/** Release the lock. */
	void unlock();
      //@}

    public:
      /** \name Sharable locking */
      //@{
	/** Wait until a lock was obtained (trigger \ref InterProcessLockReport while waiting).
	 * \throws InterProcessLockException
	 */
	void lockSharable();
	/** Wait until a lock was obtained (no callback or timeout).
	 *  \see \ref lock)
	 */
	void sleepLockSharable();
	/** Try to obtain a lock immediately (do not wait). */
	bool tryLockSharable();
	/** Try to obtain a lock before \a abs_time is reached. */
	bool timedLockSharable( const boost::posix_time::ptime & absTime_r );
	/** Try to obtain a lock within \a seconds_r seconds. */
	bool waitLockSharable( sec_type seconds_r )
	{ return timedLockSharable( wait( seconds_r ) ); }
	/** Release the lock. */
	void unlockSharable();
      //@}

    public:
      /** Convenience for timed_* methods.
       * \code
       *   InterProcessMutex m;
       *   m.timedLockSharable( InterProcessMutex::wait( 5 ) ); // timeout in 5 seconds
       * \endcode
       */
      static boost::posix_time::ptime wait( sec_type seconds_r )
      { return boost::posix_time::second_clock::universal_time() + boost::posix_time::seconds( seconds_r ); }

    public:
      class Impl;
      Impl & backdoor() { return *_pimpl; }
    private:
      RW_pointer<Impl> _pimpl;
  };
  ///////////////////////////////////////////////////////////////////

  /** \relates InterProcessMutex::State String representation */
  std::string asString( const InterProcessMutex::State obj );

  /** \relates InterProcessMutex::State Stream output */
  inline std::ostream & operator<<( std::ostream & str, const InterProcessMutex::State obj )
  { return str << asString( obj ); }

  /** \relates InterProcessMutex Stream output */
  std::ostream & operator<<( std::ostream & str, const InterProcessMutex & obj );

  ///////////////////////////////////////////////////////////////////
  /// \class InterProcessMutex::Lock<InterProcessMutex::State>
  /// \brief Aquire and automatically release a lock.
  ///
  /// Unlike the boost::SharableLock we will not unconditionally unlock
  /// the mutex when going out of scope. \ref SharableLock and \ref ScopedLock
  /// maintain counted references to the underlying mutex and the lockstate will
  /// be adjusted accordingly.
  ///
  /// A SharableLock will also succeed if the underlying mutex is in EXCLUSIVE_LOCK
  /// state. Once all ScopedLock references are gone, the mutex will go into either
  /// SHARED_LOCK or UNLOCKED state, depending on whether SharableLock references exist.
  ///
  /// \ingroup g_RAII
  /// \code
  ///   InterProcessMutex mutex;
  ///
  ///   {
  ///     // construct a sharable lock
  ///     InterProcessMutex::SharableLock slock( mutex );
  ///     ...
  ///   } // shared lock is automatically released
  /// \endcode
  /// \code
  ///   {
  ///     // try to construct a sharable lock
  ///     InterProcessMutex::SharableLock slock( mutex, InterProcessMutex::tryToLock );
  ///     if ( slock.owns() )
  ///     {
  ///       // obtained a sharable lock
  ///       ...
  ///     }
  ///   } // shared lock is automatically released if obtained
  /// \endcode
  /// \code
  ///   {
  ///     // try to construct a sharable lock within 5 seconds
  ///     InterProcessMutex::SharableLock slock( mutex, 5 );
  ///     if ( slock.owns() )
  ///     {
  ///       // obtained a sharable lock
  ///       ...
  ///     }
  ///   } // shared lock is automatically released if obtained
  /// \endcode
  /// \code
  ///   {
  ///     // may or may not need a sharable lock
  ///     InterProcessMutex::SharableLock slock( mutex, InterProcessMutex::deferLock );
  ///     // no lock obtained by now
  ///     if ( lock_needed() )
  ///     {
  ///       // obtain a sharable lock:
  ///       slock.lock();
  ///       ...
  ///     }
  ///     // still locked sharable if obtained
  ///   } // shared lock is automatically released if obtained
  /// \endcode
  /// \code
  /// {
  ///   InterProcessMutex mutex( mutexPath );
  ///   {
  ///     InterProcessMutex::SharableLock l( mutex );
  ///     // state: SHARED_LOCK
  ///     {
  ///       InterProcessMutex::ScopedLock l( mutex );
  ///       // state: EXCLUSIVE_LOCK
  ///       {
  ///         InterProcessMutex::SharableLock l( mutex );
  ///         // state: EXCLUSIVE_LOCK; superseeded by outer lock
  ///       }
  ///       // state: EXCLUSIVE_LOCK
  ///     }
  ///     // state: SHARED_LOCK ) // no unlock as shared refs exists
  ///   }
  ///   // state: UNLOCKED
  /// \endcode
  ///////////////////////////////////////////////////////////////////
  template<InterProcessMutex::State _targetState>
  class InterProcessMutex::Lock
  {
  public:
    Lock();
    explicit Lock( InterProcessMutex mutex_r );
    Lock( InterProcessMutex mutex_r, DeferLockType );
    Lock( InterProcessMutex mutex_r, TryToLockType );
    Lock( InterProcessMutex mutex_r, const boost::posix_time::ptime & absTime_r );
    Lock( InterProcessMutex mutex_r, sec_type seconds_r ) : Lock( std::forward<InterProcessMutex>(mutex_r), wait( seconds_r ) ) {}
  public:
    /** \name Locking
     * Whether exclusive or sharable locking depends on the \c _targetState.
     * \see \ref SharableLock and \ref ScopedLock
     */
    //@{
      void lock();
      void sleepLock();
      bool tryLock();
      bool timedLock( const boost::posix_time::ptime & absTime_r );
      bool waitLock( sec_type seconds_r )
      { return timedLock( wait( seconds_r ) ); }
      void unlock();
    //@}
  public:
    /** Wheter we hold a lock. */
    bool owns() const;
    /** Conversion to bool: \ref owns() */
    explicit operator bool() const
    { return owns(); }
    /** Access to the underlying \ref InterProcessMutex. */
    InterProcessMutex mutex() const;
  public:
    /** Stream output */
    std::ostream & dumpOn( std::ostream & str_r ) const;
  public:
    class Impl;
  private:
    RW_pointer<Impl> _pimpl;
  };
  ///////////////////////////////////////////////////////////////////

  /** \relates InterProcessMutex::Lock<InterProcessMutex::State> Stream output */
  template<InterProcessMutex::State _targetState>
  std::ostream & operator<<( std::ostream & str, const InterProcessMutex::Lock<_targetState> & obj )
  { return obj.dumpOn( str ); }

  ///////////////////////////////////////////////////////////////////
  /// \class InterProcessLockReport
  /// \brief Callback sending keepalive while waiting to obtain a lock.
  /// \see \ref InterProcessMutex::lock and InterProcessMutex::lockSharable
  ///////////////////////////////////////////////////////////////////
  struct InterProcessLockReport : public callback::ReportBase
  {
    typedef InterProcessMutex::sec_type sec_type;

    /** Keepalive trigger while waiting to obtain a lock.
     * \param mutexFile_r	The underlying mutex file
     * \param targetState_r	Attempt to lock shared or exclusive
     * \param total_r		Number of seconds waited so far
     * \param next_r		(ret) Modify number of seconds until next callback
     * \param timeout_r		(ret) Modify the active timeout value
     * \return Whether to continue waiting (\c true) or to abort (\c false).
     */
    virtual bool waitForLock( const Pathname & mutexFile_r,
			      InterProcessMutex::State targetState_r,
			      sec_type total_r,
			      sec_type & next_r,
			      sec_type & timeout_r )
    { return true; }
  };
  ///////////////////////////////////////////////////////////////////

  ///////////////////////////////////////////////////////////////////
  /// \class InterProcessLockException
  /// \brief Base for exceptions thrown if a lock can not be aquired.
  /// \see \ref InterProcessMutex
  ///////////////////////////////////////////////////////////////////
  struct InterProcessLockException : public Exception
  {
    typedef InterProcessMutex::sec_type sec_type;

    InterProcessLockException( const Pathname & mutexFile_r, InterProcessMutex::State targetState_r, sec_type total_r, sec_type timeout_r );

    bool aborted() const { return( _timeout == 0 || _total < _timeout ); }
    bool timeout() const { return !aborted(); }

    Pathname			_mutexFile;	//< the underlying mutex file
    InterProcessMutex::State	_targetState;	//< attempted to lock shared or exclusive
    sec_type			_total;		//< number of seconds waited to aquire the lock
    sec_type			_timeout;	//< timeout limit in seconds (0 = no timeout)
  };
  ///////////////////////////////////////////////////////////////////

  ///////////////////////////////////////////////////////////////////
  /// \class InterProcessLockTimeoutException
  /// \brief Exception thrown if waiting for a lock was aborted due to timeout.
  /// \see \ref InterProcessMutex
  ///////////////////////////////////////////////////////////////////
  struct InterProcessLockTimeoutException : public InterProcessLockException
  {
    InterProcessLockTimeoutException( const Pathname & mutexFile_r, InterProcessMutex::State targetState_r, sec_type total_r, sec_type timeout_r )
    : InterProcessLockException( mutexFile_r, targetState_r, total_r, timeout_r )
    {}
  };
  ///////////////////////////////////////////////////////////////////

  ///////////////////////////////////////////////////////////////////
  /// \class InterProcessLockAbortException
  /// \brief Exception thrown if waiting for a lock was aborted by callback.
  /// \see \ref InterProcessMutex
  ///////////////////////////////////////////////////////////////////
  struct InterProcessLockAbortException : public InterProcessLockException
  {
    InterProcessLockAbortException( const Pathname & mutexFile_r, InterProcessMutex::State targetState_r, sec_type total_r, sec_type timeout_r )
    : InterProcessLockException( mutexFile_r, targetState_r, total_r, timeout_r )
    {}
  };
  ///////////////////////////////////////////////////////////////////

  } // namespace base
  ///////////////////////////////////////////////////////////////////
} // namespace zypp
///////////////////////////////////////////////////////////////////
#endif


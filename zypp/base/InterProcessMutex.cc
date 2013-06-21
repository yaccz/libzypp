/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file	zypp/base/InterProcessMutex.cc
 *
*/
#include <iostream>

#include <boost/interprocess/sync/file_lock.hpp>
#include <boost/interprocess/sync/sharable_lock.hpp>
#include <boost/interprocess/sync/scoped_lock.hpp>

#include "zypp/base/LogTools.h"
#include "zypp/base/Gettext.h"
#include "zypp/base/NonCopyable.h"
#include "zypp/base/Functional.h"
#include "zypp/base/String.h"
#include "zypp/Pathname.h"

#include "zypp/base/InterProcessMutex.h"


#undef ZYPP_BASE_LOGGER_LOGGROUP
#define ZYPP_BASE_LOGGER_LOGGROUP "zypp::MTX"

#if ( 0 )
#define TAG DBG << __PRETTY_FUNCTION__ << endl;
#else
#define TAG
#endif

///////////////////////////////////////////////////////////////////
namespace zypp
{
  ///////////////////////////////////////////////////////////////////
  namespace base
  {
    namespace env
    {
      /**  */
      inline unsigned ZYPP_MAX_LOCK_WAIT( unsigned default_r = 0U )
      {
	const char * env = getenv("ZYPP_MAX_LOCK_WAIT");
	if ( env )
	  return str::strtonum<unsigned>( env );
	return default_r;
      }
    }

    ///////////////////////////////////////////////////////////////////
    namespace
    {
      ///////////////////////////////////////////////////////////////////
      /// \class PhoenixMap<PhoenixKeyType, PhoenixValueType>
      /// \brief Map of phoenix-singletons
      ///
      /// A weak_ptr to the created PhoenixValue is stored in the _phoenixMap,
      /// this way subsequent requests for the same PhoenixKey can reuse the
      /// PhoenixValue if it is still in scope. Otherwise the PhoenixValue
      /// is re-created.
      ///
      /// \note PhoenixValueType must be constructible from PhoenixKeyType.
      ///////////////////////////////////////////////////////////////////
      template <class PhoenixKeyType, class PhoenixValueType>
      class PhoenixMap : private base::NonCopyable
      {
      public:
	shared_ptr<PhoenixValueType> get( const PhoenixKeyType & key_r )
	{
	  shared_ptr<PhoenixValueType> ret( _phoenixMap[key_r].lock() ); // weak_ptr lock ;)
	  if ( ! ret )
	  {
	    DBG << "New Phoenix " << key_r << endl;
	    ret.reset( new PhoenixValueType( key_r ) );
	    _phoenixMap[key_r] = ret;
	  }
	  else
	  {
	    DBG << "Reuse Phoenix " << key_r << endl;
	  }
	  return ret;

	}
      private:
	std::map<PhoenixKeyType, weak_ptr<PhoenixValueType> > _phoenixMap;
      };
      ///////////////////////////////////////////////////////////////////

      /** \relates InterProcessMutex */
      typedef PhoenixMap<Pathname, InterProcessMutex::Impl> PhoenixMapType;

      /** \relates InterProcessMutex PhoenixMap of active InterProcessMutexes. */
      inline PhoenixMapType & phoenixMap()
      {
	static PhoenixMapType _phoenixMap;
	return _phoenixMap;
      }
    } // namespace
    ///////////////////////////////////////////////////////////////////

    std::ostream & operator<<( std::ostream & str, const InterProcessMutex::Impl & obj );

    ///////////////////////////////////////////////////////////////////
    /// \class InterProcessMutex::Impl
    /// \brief \ref InterProcessMutex implementation
    ///////////////////////////////////////////////////////////////////
    class InterProcessMutex::Impl : private base::NonCopyable
    {
      sec_type initialLockWait()
      { return 3U; }

      sec_type maxLockWait()
      { static sec_type _val = env::ZYPP_MAX_LOCK_WAIT( 180U ); return _val; }

    public:
      /** Underlying boost::interprocess::mutex */
      typedef boost::interprocess::file_lock LockType;

    public:
      Impl( FakeLockType )
      : _mutexFile( InterProcessMutex::fakeLockPath() )
      , _state( UNLOCKED )
      {}

      Impl( const Pathname & path_r )
      : _mutexFile( path_r )
      , _mutex( new LockType( path_r.c_str() ) )
      , _state( UNLOCKED )
      {}

      ~Impl()
      { DBG << "Burn Phoenix " << _mutexFile << endl; }

    public:
      State state() const
      { return _state; }

      Pathname mutexFile() const
      { return _mutexFile; }

    public:
      void lock()
      {
	if ( ! timedLock( wait( initialLockWait() ) ) )
	{
	  if ( _state != UNLOCKED )
	  {
	    MIL << "Drop " << SHARED_LOCK << " lock to avoid deadlock;" << *this << endl;
	    unlock();
	  }

	  callback::SendReport<InterProcessLockReport> report;
	  sec_type total   = 0;
	  sec_type next    = initialLockWait();
	  sec_type timeout = maxLockWait();
	  WAR << "No " << EXCLUSIVE_LOCK << " lock within " << next << "/" << timeout << "; wait " << next << "; " << *this << endl;
	  do {
	    total += next;
	    if ( timeout && total >= timeout )
	    {
	      ERR << "No " << EXCLUSIVE_LOCK << " lock within " << total << "/" << timeout << "; Give up. " << *this << endl;
	      throw InterProcessLockTimeoutException( _mutexFile, EXCLUSIVE_LOCK, total, timeout );
	    }
	    if ( ! report->waitForLock( _mutexFile, EXCLUSIVE_LOCK, total, next, timeout ) )
	    {
	      ERR << "No " << EXCLUSIVE_LOCK << " lock within " << total << "/" << timeout << "; Give up requested. " << *this << endl;
	      throw InterProcessLockAbortException( _mutexFile, EXCLUSIVE_LOCK, total, timeout );
	    }
	  } while( ! timedLock( wait( next ) ) );
	  MIL << "Got " << EXCLUSIVE_LOCK << " lock after " << total+next << "/" << timeout << ";" << *this << endl;
	}
      }

      void sleepLock()
      { if ( _state != EXCLUSIVE_LOCK )		{ if ( _mutex ) _mutex->lock(); _state = EXCLUSIVE_LOCK; } }

      bool tryLock()
      {
	if ( _state == EXCLUSIVE_LOCK )		return true;
        if ( !_mutex || _mutex->try_lock() )	{ _state = EXCLUSIVE_LOCK; return true; }
        return false;
      }

      bool timedLock( const boost::posix_time::ptime & absTime_r )
      {
	if ( _state == EXCLUSIVE_LOCK )		return true;
	if ( !_mutex || _mutex->timed_lock( absTime_r ) ) { _state = EXCLUSIVE_LOCK; return true; }
	return false;
      }

      void unlock()	// not bound to EXCLUSIVE_LOCK
      {  if ( _state != UNLOCKED ) 		{ if ( _mutex ) _mutex->unlock(); _state = UNLOCKED; } }

    public:
      void lockSharable()
      {
	if ( ! timedLockSharable( wait( initialLockWait() ) ) )
	{
	  callback::SendReport<InterProcessLockReport> report;
	  sec_type total   = 0;
	  sec_type next    = initialLockWait();
	  sec_type timeout = maxLockWait();
	  WAR << "No " << SHARED_LOCK << " lock within " << next << "/" << timeout << "; wait " << next << "; " << *this << endl;
	  do {
	    total += next;
	    if ( timeout && total >= timeout )
	    {
	      ERR << "No " << SHARED_LOCK << " lock within " << total << "/" << timeout << "; Give up. " << *this << endl;
	      throw InterProcessLockTimeoutException( _mutexFile, SHARED_LOCK, total, timeout );
	    }
	    if ( ! report->waitForLock( _mutexFile, SHARED_LOCK, total, next, timeout ) )
	    {
	      ERR << "No " << SHARED_LOCK << " lock within " << total << "/" << timeout << "; Give up requested. " << *this << endl;
	      throw InterProcessLockAbortException( _mutexFile, SHARED_LOCK, total, timeout );
	    }
	  } while( ! timedLockSharable( wait( next ) ) );
	  MIL << "Got " << SHARED_LOCK << " lock after " << total+next << "/" << timeout << ";" << *this << endl;
	}
      }

      void sleepLockSharable()
      { if ( _state != SHARED_LOCK )		{ if ( _mutex ) _mutex->lock_sharable(); _state = SHARED_LOCK; } }

      bool tryLockSharable()
      {
	if ( _state == SHARED_LOCK )		return true;
	if ( !_mutex || _mutex->try_lock_sharable() ) { _state = SHARED_LOCK; return true; }
	return false;
      }

      bool timedLockSharable( const boost::posix_time::ptime & absTime_r )
      {
	if ( _state == SHARED_LOCK )		return true;
	if ( !_mutex || _mutex->timed_lock_sharable( absTime_r ) ) { _state = SHARED_LOCK; return true; }
	return false;
      }

      void unlockSharable()	// not bound to SHARED_LOCK
      {  if ( _state != UNLOCKED )		{ if ( _mutex ) _mutex->unlock_sharable(); _state = UNLOCKED; } }

    public:
      /** Shared_ptr with cusom deleter used as reference to a lockstate.
       * \see \ref getRef
       */
      typedef shared_ptr<void> LockStateRef;

      /** Aquire a reference to a lockstate.
       * \see \ref unref
       */
      LockStateRef getRef( State state_r )
      {
	if ( state_r == UNLOCKED )	// NOOP
	  return shared_ptr<void>( (void*)0 );

	weak_ptr<void>& lockRef( state_r == EXCLUSIVE_LOCK ? _scopedRefs : _sharedRefs );
	shared_ptr<void> ret( lockRef.lock() ); // weak_ptr lock ;)
	if ( ! ret )
	{
	  lockRef = ret = shared_ptr<void>( (void*)state_r, bind( mem_fun_ref( &Impl::unref ), ref(*this), _1 ) );
	  dumpOn( MIL << "+++ " << state_r << " " ) << endl;
	}
	else
	  dumpOn( DBG << "+++ " << state_r << " " ) << endl;
	return ret;
      }

    private:
      /** Custom deleter for lockstate references.
       * \see \ref getRef
       */
      void unref( void * p )
      {
	// * If the actual mutex state does not match the droped
	// lockstate reference we simply do nothing.Either we are
	// superseeded buy a higher lockstate, or someone manually
	// fiddled with the mutex.
	// * Note that this is a shared_pointers custom deleter,
	// so the coresponding lockRef is expired. No need to test.
	State expiredState = State(long(p)); // due to the way it was created in getRef()
	if ( expiredState == _state )
	{
	  switch ( _state )
	  {
	    case EXCLUSIVE_LOCK:
	      // Here: _scopedRefs.expired()
	      if ( _sharedRefs.expired() )
		unlock();
	      else
		lockSharable();
	      break;
	    case SHARED_LOCK:
	      // Here: _sharedRefs.expired()
	      if ( !_scopedRefs.expired() )
		INT << "Unexpected mutex state: have scopedRefs but in SHARED_LOCK state!" << endl;
	      unlockSharable();
	      break;
	    case UNLOCKED:
	      INT << "Unexpected mutex state: had refs but in UNLOCKED state" << endl;
	      break;
	  }
	  dumpOn( MIL << "--- " << expiredState << " " ) << endl;
	}
	else
	  dumpOn( DBG << "--- " << expiredState << " " ) << endl;
     }

    private:
      const Pathname	_mutexFile;	///< only for log and debug
      scoped_ptr<LockType> _mutex;
      State		_state;
      weak_ptr<void>	_sharedRefs;	///< references held by \ref SharableLock
      weak_ptr<void>	_scopedRefs;	///< references held by \ref ScopedLock

    public:
      /** Stream output */
      std::ostream & dumpOn( std::ostream & str ) const
      {
	return str << "[" << _state
		   << "(" << _sharedRefs.use_count()
		   << "," << _scopedRefs.use_count()
		   << ")" << _mutexFile
		   << "]";
      }
    };
    ///////////////////////////////////////////////////////////////////

    /** \relates InterProcessMutex::Impl Stream output */
    inline std::ostream & operator<<( std::ostream & str, const InterProcessMutex::Impl & obj )
    { return obj.dumpOn( str ); }

    ///////////////////////////////////////////////////////////////////
    // class InterProcessMutex
    ///////////////////////////////////////////////////////////////////

    Pathname InterProcessMutex::fakeLockPath()
    { static const Pathname _p( "/<fakelock>" ); return _p; }


    InterProcessMutex::InterProcessMutex()
    {}

    InterProcessMutex::InterProcessMutex( FakeLockType flag_r )
    : _pimpl( new Impl( flag_r ) )
    {}

    InterProcessMutex::InterProcessMutex( const Pathname & path_r )
    : _pimpl( path_r != fakeLockPath() ? phoenixMap().get( path_r )
					 : shared_ptr<Impl>( new Impl( fakeLock ) ) )
    {}


    InterProcessMutex::State InterProcessMutex::state() const
    { return _pimpl->state(); }

    Pathname InterProcessMutex::mutexFile() const
    { return _pimpl->mutexFile(); }


    void InterProcessMutex::lock()
    { TAG; return _pimpl->lock(); }

    void InterProcessMutex::sleepLock()
    { TAG; return _pimpl->sleepLock(); }

    bool InterProcessMutex::tryLock()
    { TAG; return _pimpl->tryLock(); }

    bool InterProcessMutex::timedLock( const boost::posix_time::ptime & absTime_r )
    { TAG; return _pimpl->timedLock( absTime_r ); }

    void InterProcessMutex::unlock()
    { TAG; return _pimpl->unlock(); }


    void InterProcessMutex::lockSharable()
    { TAG; return _pimpl->lockSharable(); }

    void InterProcessMutex::sleepLockSharable()
    { TAG; return _pimpl->sleepLockSharable(); }

    bool InterProcessMutex::tryLockSharable()
    { TAG; return _pimpl->tryLockSharable(); }

    bool InterProcessMutex::timedLockSharable( const boost::posix_time::ptime & absTime_r )
    { TAG; return _pimpl->timedLockSharable( absTime_r ); }

    void InterProcessMutex::unlockSharable()
    { TAG; return _pimpl->unlockSharable(); }


    std::ostream & operator<<( std::ostream & str, const InterProcessMutex & obj )
    {
      if ( obj._pimpl )
	return obj._pimpl->dumpOn( str );
      return str << "[NO MUTEX]";
    }

    ///////////////////////////////////////////////////////////////////
    // enum InterProcessMutex::State
    ///////////////////////////////////////////////////////////////////

    std::string asString( const InterProcessMutex::State obj )
    {
      switch ( obj )
      {
	case InterProcessMutex::UNLOCKED:	return "-nl-";	break;
	case InterProcessMutex::SHARED_LOCK:	return "shar";	break;
	case InterProcessMutex::EXCLUSIVE_LOCK:	return "EXCL";	break;
      }
      INT << "InterProcessMutex::State(?)" << endl;
      return "InterProcessMutex::State(?)";
    }

    ///////////////////////////////////////////////////////////////////
    namespace
    {
      ///////////////////////////////////////////////////////////////////
      /// \class lock_exception
      /// \brief Thrown by LockImplBase if the mutex is not usable.
      class lock_exception : public boost::interprocess::lock_exception
      {
      public:
	lock_exception()
	:  boost::interprocess::lock_exception()
	{}

	virtual const char* what() const throw()
	{ return "boost::interprocess::lock_exception: no mutex";  }
      };
      ///////////////////////////////////////////////////////////////////

      ///////////////////////////////////////////////////////////////////
      /// \class LockImplTraits<InterProcessMutex::State>
      /// \brief Traits class selecting shared/exclusive InterProcessMutex calls.
      ///////////////////////////////////////////////////////////////////
      template<InterProcessMutex::State _targetState>
      struct LockImplTraits
      {
	typedef void (InterProcessMutex::*LockFnc)();
	typedef void (InterProcessMutex::*SleepLockFnc)();
	typedef bool (InterProcessMutex::*TryLockFnc)();
	typedef bool (InterProcessMutex::*TimedLockFnc)( const boost::posix_time::ptime & absTime_r );

	static const LockFnc		lock;
	static const SleepLockFnc	sleepLock;
	static const TryLockFnc		tryLock;
	static const TimedLockFnc	timedLock;
      };

      /** NoOp for UNLOCKED state */
      template<>
      struct LockImplTraits<InterProcessMutex::UNLOCKED>
      {};

      template<>
      const LockImplTraits<InterProcessMutex::SHARED_LOCK>::LockFnc
	    LockImplTraits<InterProcessMutex::SHARED_LOCK>::lock	= &InterProcessMutex::lockSharable;
      template<>
      const LockImplTraits<InterProcessMutex::SHARED_LOCK>::SleepLockFnc
	    LockImplTraits<InterProcessMutex::SHARED_LOCK>::sleepLock	= &InterProcessMutex::sleepLockSharable;
      template<>
      const LockImplTraits<InterProcessMutex::SHARED_LOCK>::TryLockFnc
	    LockImplTraits<InterProcessMutex::SHARED_LOCK>::tryLock	= &InterProcessMutex::tryLockSharable;
      template<>
      const LockImplTraits<InterProcessMutex::SHARED_LOCK>::TimedLockFnc
	    LockImplTraits<InterProcessMutex::SHARED_LOCK>::timedLock	= &InterProcessMutex::timedLockSharable;

      template<>
      const LockImplTraits<InterProcessMutex::EXCLUSIVE_LOCK>::LockFnc
	    LockImplTraits<InterProcessMutex::EXCLUSIVE_LOCK>::lock	= &InterProcessMutex::lock;
      template<>
      const LockImplTraits<InterProcessMutex::EXCLUSIVE_LOCK>::SleepLockFnc
	    LockImplTraits<InterProcessMutex::EXCLUSIVE_LOCK>::sleepLock= &InterProcessMutex::sleepLock;
      template<>
      const LockImplTraits<InterProcessMutex::EXCLUSIVE_LOCK>::TryLockFnc
	    LockImplTraits<InterProcessMutex::EXCLUSIVE_LOCK>::tryLock	= &InterProcessMutex::tryLock;
      template<>
      const LockImplTraits<InterProcessMutex::EXCLUSIVE_LOCK>::TimedLockFnc
	    LockImplTraits<InterProcessMutex::EXCLUSIVE_LOCK>::timedLock= &InterProcessMutex::timedLock;

    } // namespace
    ///////////////////////////////////////////////////////////////////

    ///////////////////////////////////////////////////////////////////
    /// \class InterProcessMutex::Lock<InterProcessMutex::State>::Impl
    /// \brief \ref InterProcessMutex::Lock<InterProcessMutex::State> implementation
    ///////////////////////////////////////////////////////////////////
    template<InterProcessMutex::State _targetState>
    class InterProcessMutex::Lock<_targetState>::Impl : private base::NonCopyable
    {
      typedef LockImplTraits<_targetState>		Traits;
      typedef InterProcessMutex::Impl::LockStateRef	LockStateRef;
    public:
      Impl()
      {}

      explicit Impl( InterProcessMutex mutex_r )
      : _mutex( mutex_r )
      { lock(); }

      Impl( InterProcessMutex mutex_r, InterProcessMutex::DeferLockType )
      : _mutex( mutex_r )
      {}

      Impl( InterProcessMutex mutex_r, InterProcessMutex::TryToLockType )
      : _mutex( mutex_r )
      { tryLock(); }

      Impl( InterProcessMutex mutex_r, const boost::posix_time::ptime & absTime_r )
      : _mutex( mutex_r )
      { timedLock( absTime_r ); }

    public:
      void lock()
      {
	assertMutex();
	if ( needStateChange() )
	{
	  (_mutex.*Traits::lock)();
	}
	getRef();
      }

      void sleepLock()
      {
	assertMutex();
	if ( needStateChange() )
	{
	  (_mutex.*Traits::sleepLock)();
	}
	getRef();
      }

      bool tryLock()
      {
	assertMutex();
	if ( needStateChange() && !(_mutex.*Traits::tryLock)() )
	{
	  unref();
	  return false;
	}
	getRef();
	return true;
      }

      bool timedLock( const boost::posix_time::ptime & absTime_r )
      {
	assertMutex();
	if ( needStateChange() && !(_mutex.*Traits::timedLock)( absTime_r ) )
	{
	  unref();
	  return false;
	}
	getRef();
	return true;
      }

      void unlock()
      {
	assertMutex();
	unref();
      }

    public:
      bool owns() const
      { return _mutexRef; }

      InterProcessMutex mutex() const
      { return _mutex; }

      /** Stream output */
      std::ostream & dumpOn( std::ostream & str ) const
      {
	return str << "[" << ( owns() ? _targetState : InterProcessMutex::UNLOCKED ) << _mutex << "]";
      }

    private:
      void assertMutex() const
      { if ( !_mutex ) throw lock_exception(); }

      /** Whether we actually need to obtain a lock. */
      bool needStateChange() const
      { return _mutex.state() < _targetState; }

    private:
      void getRef()
      { if ( !_mutexRef ) _mutexRef = _mutex.backdoor().getRef( _targetState ); }

      void unref()
      { _mutexRef.reset(); }

    private:
      InterProcessMutex	_mutex;		///< reference to the mutex itself
      LockStateRef	_mutexRef;	///< reference if we hold a lock
    };
    ///////////////////////////////////////////////////////////////////

    ///////////////////////////////////////////////////////////////////
    // class InterProcessMutex::Lock
    ///////////////////////////////////////////////////////////////////

    template<InterProcessMutex::State _targetState>
    InterProcessMutex::Lock<_targetState>::Lock()
    {}

    template<InterProcessMutex::State _targetState>
    InterProcessMutex::Lock<_targetState>::Lock( InterProcessMutex mutex_r )
    : _pimpl( new Impl( mutex_r ) )
    {}

    template<InterProcessMutex::State _targetState>
    InterProcessMutex::Lock<_targetState>::Lock( InterProcessMutex mutex_r, DeferLockType flag_r )
    : _pimpl( new Impl( mutex_r, flag_r ) )
    {}

    template<InterProcessMutex::State _targetState>
    InterProcessMutex::Lock<_targetState>::Lock( InterProcessMutex mutex_r, TryToLockType flag_r )
    : _pimpl( new Impl( mutex_r, flag_r ) )
    {}

    template<InterProcessMutex::State _targetState>
    InterProcessMutex::Lock<_targetState>::Lock( InterProcessMutex mutex_r, const boost::posix_time::ptime & absTime_r )
    : _pimpl( new Impl( mutex_r, absTime_r ) )
    {}


    template<InterProcessMutex::State _targetState>
    void InterProcessMutex::Lock<_targetState>::lock()
    { return _pimpl->lock(); }

    template<InterProcessMutex::State _targetState>
    void InterProcessMutex::Lock<_targetState>::sleepLock()
    { return _pimpl->sleepLock(); }

    template<InterProcessMutex::State _targetState>
    bool InterProcessMutex::Lock<_targetState>::tryLock()
    { return _pimpl->tryLock(); }

    template<InterProcessMutex::State _targetState>
    bool InterProcessMutex::Lock<_targetState>::timedLock( const boost::posix_time::ptime & absTime_r )
    { return _pimpl->timedLock( absTime_r ); }

    template<InterProcessMutex::State _targetState>
    void InterProcessMutex::Lock<_targetState>::unlock()
    { return _pimpl->unlock(); }


    template<InterProcessMutex::State _targetState>
    bool InterProcessMutex::Lock<_targetState>::owns() const
    { return _pimpl && _pimpl->owns(); }

    template<InterProcessMutex::State _targetState>
    InterProcessMutex InterProcessMutex::Lock<_targetState>::mutex() const
    { return _pimpl ? _pimpl->mutex() : InterProcessMutex(); }

    template<InterProcessMutex::State _targetState>
    std::ostream & InterProcessMutex::Lock<_targetState>::dumpOn( std::ostream & str ) const
    {
      if ( _pimpl )
	return _pimpl->dumpOn( str );
      return str << "[NO LOCK]";
    }

    ///////////////////////////////////////////////////////////////////
    // class InterProcessMutexLockException
    ///////////////////////////////////////////////////////////////////
    namespace
    {
      std::string messageInterProcessLockException( const Pathname & mutexFile_r,
						    InterProcessMutex::State targetState_r,
						    InterProcessLockException::sec_type total_r,
						    InterProcessLockException::sec_type timeout_r )
      {
	// translators: will finally look like: "...lock on file <filename>: <reason>"
	std::string f( targetState_r == InterProcessMutex::SHARED_LOCK
		       ? _("Unable to obtain a shared lock on file %s")
		       : _("Unable to obtain an exclusive lock on file %s") );
	f += ": ";
	f += timeout_r == 0 || total_r < timeout_r
	     ? _("Aborted after %u seconds.")
	     : _("Timeout after %u seconds.");

	return str::form( f.c_str(), mutexFile_r.c_str(), total_r );
      }
    }

    InterProcessLockException::InterProcessLockException( const Pathname & mutexFile_r,
							  InterProcessMutex::State targetState_r,
							  sec_type total_r,
							  sec_type timeout_r )
    : Exception( messageInterProcessLockException( mutexFile_r, targetState_r, total_r, timeout_r ) )
    , _mutexFile( mutexFile_r )
    , _targetState( targetState_r )
    , _total( total_r )
    , _timeout( timeout_r )
    {}

    ///////////////////////////////////////////////////////////////////
    // Explicit instantiations:
    ///////////////////////////////////////////////////////////////////

    template class InterProcessMutex::Lock<InterProcessMutex::SHARED_LOCK>;
    template class InterProcessMutex::Lock<InterProcessMutex::EXCLUSIVE_LOCK>;

  } // namespace base
  ///////////////////////////////////////////////////////////////////
} // namespace zypp
///////////////////////////////////////////////////////////////////

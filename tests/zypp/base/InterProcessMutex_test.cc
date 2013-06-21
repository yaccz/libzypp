
#include <sys/wait.h>

#include <iostream>
#include <boost/test/auto_unit_test.hpp>
#include <boost/interprocess/sync/file_lock.hpp>
#include <boost/interprocess/sync/sharable_lock.hpp>
#include <boost/interprocess/sync/scoped_lock.hpp>

#include "zypp/TmpPath.h"
#include "zypp/base/LogTools.h"
#include "zypp/base/LogControl.h"
#include "zypp/base/Exception.h"
#include "zypp/base/InterProcessMutex.h"

using boost::unit_test::test_suite;
using boost::unit_test::test_case;
using namespace boost::unit_test;

using namespace std;
using namespace zypp;
using zypp::base::InterProcessMutex;

# if ( 1 )
static filesystem::TmpFile tmpFile;
static Pathname mutexPath( tmpFile.path() );
#else
static Pathname mutexPath( InterProcessMutex::fakeLockPath() );
#endif

/** Fork to check the extern visible state of the lockfile. */
int lockStatus()
{
  int pid = fork();
  if ( pid < 0 )
  {
    ERR << "lockStatus fork failed" << endl;
    return 98;
  }
  else if ( pid == 0 )
  {
    // child:
    zypp::base::LogControl::TmpLineWriter shutUp;
    boost::interprocess::file_lock qmutex( mutexPath.c_str() );
    if ( qmutex.try_lock() )
    {
      qmutex.unlock();
      exit( 0 );
    }
    else if ( qmutex.try_lock_sharable() )
    {
      qmutex.unlock_sharable();
      exit( 1 );
    }
    // else
    exit( 2 );
  }
  else
  {
    // parent:
    int ret;
    int status = 0;
    do
    {
      ret = waitpid( pid, &status, 0 );
    }
    while ( ret == -1 && errno == EINTR );

    if ( WIFEXITED( status ) )
    {
      _MIL("___MTX___") << "lockStatus " << WEXITSTATUS( status ) << endl;
      return WEXITSTATUS( status );
    }
    _ERR("___MTX___") << "lockStatus failed" << endl;
    return 99;
  }
}


BOOST_AUTO_TEST_CASE(basic_file_lock_behavior)
{
  // cant test this for a fake lock
  if ( mutexPath == InterProcessMutex::fakeLockPath() )
    return;

  // Test basic behavior of boost::file_lock regarding exceptions
  // thrown (or not thrown) on certain command combinations.
  //
  // E.g. lock exclusively then unlock shared -> no exception
  //
  typedef boost::interprocess::file_lock LockType;
  LockType mutex( mutexPath.c_str() );

  // Normal sequence
  mutex.lock();
  mutex.unlock();

  mutex.lock_sharable();
  mutex.unlock_sharable();

  // mixed lock unlock (should work as unlock == unlockSharable)
  mutex.lock();
  mutex.unlock_sharable();

  mutex.lock_sharable();
  mutex.unlock();

  // double unlock
  mutex.unlock();
  mutex.unlock();
  mutex.unlock_sharable();
  mutex.unlock_sharable();

  // double unlock
  mutex.lock();
  mutex.lock();
  mutex.lock_sharable();
  mutex.lock_sharable();

  //
  mutex.unlock();
  mutex.lock_sharable();
  mutex.lock_sharable();
  boost::interprocess::scoped_lock<LockType> a;
  boost::interprocess::scoped_lock<LockType> b;
  BOOST_CHECK_THROW( b.lock(), boost::interprocess::lock_exception );
}

#define CHECK_STATE(STATE) BOOST_CHECK_EQUAL( mutex.state(), InterProcessMutex::STATE )
#define CHECK_NOT_STATE(STATE) BOOST_CHECK( mutex.state() != InterProcessMutex::STATE )

#define SWITCH_STATE(CMD,STATE) mutex.CMD(); CHECK_STATE( STATE )

BOOST_AUTO_TEST_CASE(basic_mutex)
{
  // Basic operations switch to the requested mutex state. This differs from
  // InterProcessMutex::{SharableLock,ScopedLock} where requesting e.g. a
  // SharableLock is also fulfilled by staying in EXCLUSIVE_LOCK state.

  BOOST_CHECK( ! InterProcessMutex() );	// default constructed evaluates as 'false'

  InterProcessMutex mutex( mutexPath );
  BOOST_CHECK( mutex );			// non-default constructed evaluates as 'true'
  CHECK_STATE( UNLOCKED );

  SWITCH_STATE( lockSharable,	SHARED_LOCK );
  SWITCH_STATE( lock,		EXCLUSIVE_LOCK );
  SWITCH_STATE( unlockSharable,	UNLOCKED );	//<  unlockSharable == unlock

  SWITCH_STATE( lock,		EXCLUSIVE_LOCK );
  SWITCH_STATE( lockSharable,	SHARED_LOCK );
  SWITCH_STATE( unlock,		UNLOCKED );	//<  unlockSharable == unlock
}


template<class LockType>
void basicLockTest()
{
  // basic behavior of SharableLock/ScopedLock is the same.
  {
    // default constructed evaluates as 'false'
    LockType _lock;
    BOOST_CHECK( ! _lock.owns() );
    BOOST_CHECK( ! _lock.mutex() );
  }

  {
    // using default constructed mutex evaluates as 'false'
    InterProcessMutex mutex;
    LockType _lock( mutex, InterProcessMutex::deferLock );
    BOOST_CHECK( ! _lock.owns() );
    BOOST_CHECK( ! _lock.mutex() );
    BOOST_CHECK_THROW( _lock.lock(), boost::interprocess::lock_exception );
  }

  InterProcessMutex mutex( mutexPath );

  {
    // lock, unlock, lock
    LockType _lock( mutex, InterProcessMutex::deferLock );
    BOOST_CHECK( ! _lock.owns() );
    BOOST_CHECK( _lock.mutex() );

    _lock.lock();
    BOOST_CHECK( _lock.owns() );
    CHECK_NOT_STATE( UNLOCKED );

    _lock.unlock();
    BOOST_CHECK( !_lock.owns() );
    CHECK_STATE( UNLOCKED );

    BOOST_CHECK( _lock.tryLock() );
    CHECK_NOT_STATE( UNLOCKED );

    {
      // nested lock:
      LockType _lock( mutex, InterProcessMutex::deferLock );
      BOOST_CHECK( ! _lock.owns() );
      BOOST_CHECK( _lock.mutex() );

      _lock.lock();
      BOOST_CHECK( _lock.owns() );
      CHECK_NOT_STATE( UNLOCKED );
    }
    // still locked by outer ref
    CHECK_NOT_STATE( UNLOCKED );
  }

  CHECK_STATE( UNLOCKED );

  {
    LockType _lock( mutex );
    BOOST_CHECK( _lock.owns() );
    CHECK_NOT_STATE( UNLOCKED );
    {
      // nested lock:
      LockType _lock( mutex );
      BOOST_CHECK( _lock.owns() );
      CHECK_NOT_STATE( UNLOCKED );

      // explicit 'unlock' overrules any ref!!
      _lock.mutex().unlock();
      CHECK_STATE( UNLOCKED );
    }
    // no longer locked!
    CHECK_STATE( UNLOCKED );
  }

  CHECK_STATE( UNLOCKED );
}


BOOST_AUTO_TEST_CASE(basic_lock)
{
  basicLockTest<InterProcessMutex::SharableLock>();
  basicLockTest<InterProcessMutex::ScopedLock>();
}


BOOST_AUTO_TEST_CASE(mixed_lock)
{
  INT << endl;
  InterProcessMutex mutex( mutexPath );
  {
    InterProcessMutex::SharableLock l( mutex );
    CHECK_STATE( SHARED_LOCK );
    {
      InterProcessMutex::ScopedLock l( mutex );
      CHECK_STATE( EXCLUSIVE_LOCK );
      {
	InterProcessMutex::SharableLock l( mutex );
	CHECK_STATE( EXCLUSIVE_LOCK ); // superseeded by outer lock
      }
      CHECK_STATE( EXCLUSIVE_LOCK );
    }
    CHECK_STATE( SHARED_LOCK ); // no unlock as shared ref exists
  }
  CHECK_STATE( UNLOCKED ); // unlock

  INT << endl;
  {
    InterProcessMutex::ScopedLock l( mutex );
    CHECK_STATE( EXCLUSIVE_LOCK );
    {
      InterProcessMutex::ScopedLock l( mutex );
      CHECK_STATE( EXCLUSIVE_LOCK ); // superseeded by outer lock
    }
    CHECK_STATE( EXCLUSIVE_LOCK );
  }
  CHECK_STATE( UNLOCKED ); // unlock
}

#include "Tools.h"
#include <sys/wait.h>

#include <zypp/IPMutex.h>

#include <boost/interprocess/sync/file_lock.hpp>
#include <boost/interprocess/sync/sharable_lock.hpp>
#include <boost/interprocess/sync/scoped_lock.hpp>

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
    boost::interprocess::file_lock qmutex( IPMutex().mutexFile().c_str() );
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

#define LTAG(X) MIL << X << " " << #X << endl;

static weak_ptr<void> wp;

void ssendp( void * )
{
  SEC << "endp " << wp.use_count() << endl;
}

shared_ptr<void> getH()
{
  shared_ptr<void> ret( wp.lock() );
  if ( ! ret )
  {
    MIL << endl;
    ret.reset( static_cast<void*>((void*)1), &ssendp );
    wp = ret;
  }
  return ret;
}
shared_ptr<void> getH1()
{
  static shared_ptr<void> ret( static_cast<void*>(0), &ssendp );
  return ret;
}

/******************************************************************
**
**      FUNCTION NAME : main
**      FUNCTION TYPE : int
*/
int main( int argc, char * argv[] )
{
  INT << "===[START]==========================================" << endl;
  ///////////////////////////////////////////////////////////////////

  IPMutex mutex;
  MIL << mutex << endl;
  mutex.lock();
  MIL << mutex << endl;
  {
    IPMutex mutex;
    MIL << mutex << endl;

  }
  WAR << mutex << endl;

  ///////////////////////////////////////////////////////////////////
  INT << "===[END]============================================" << endl << endl;
  return 0;




  IPMutex::SharableLock slocka( mutex, IPMutex::deferLock );
  LTAG( slocka );
  if ( 1 )
  {
    IPMutex::SharableLock slock( IPMutex() );
    LTAG( slock );
    LTAG( slocka );
    sleep( 3 );
    {
      IPMutex::SharableLock slock2( mutex );
      LTAG( slock );
      LTAG( slock2 );
      sleep( 3 );
      INT << IPMutex() << endl;
    }
    LTAG( slock );
    sleep( 3 );
  }
  MIL << '-' << '(' << lockStatus() << ')' << '(' << mutex << ')' <<endl;

  ///////////////////////////////////////////////////////////////////
  INT << "===[END]============================================" << endl << endl;
  return 0;
}


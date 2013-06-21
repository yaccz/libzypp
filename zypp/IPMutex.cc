/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file	zypp/IPMutex.cc
 */
//#include <unistd.h>
#include <iostream>

#include "zypp/base/LogTools.h"
#include "zypp/base/InterProcessMutex.h"

#include "zypp/PathInfo.h"
#include "zypp/IPMutex.h"

using std::endl;

///////////////////////////////////////////////////////////////////
namespace zypp
{
  namespace env
  {
    /** Hack to circumvent the currently poor --root support. */
    inline Pathname ZYPP_LOCKFILE_ROOT()
    { return getenv("ZYPP_LOCKFILE_ROOT") ? getenv("ZYPP_LOCKFILE_ROOT") : "/"; }
  }

  ///////////////////////////////////////////////////////////////////
  namespace
  {
    const Pathname & defaultMutexDir()
    { static const Pathname _val( "/var/run/zypp" ); return _val; }

    const std::string & defaultMutexName()
    { static const std::string _val( "common.lock" ); return _val; }

    const Pathname & defaultMutexFile()
    { static const Pathname _val( defaultMutexDir() / defaultMutexName() ); return _val; }

    Pathname makeMutexFile( const std::string & mutexName_r )
    { return defaultMutexDir() / (mutexName_r.empty() ? defaultMutexName() : mutexName_r); }


    /** \relates IPMutex Use the underlying mutex file (fake for non-root). */
    Pathname IPMutexUseLockFile( const Pathname & mutexFile_r, bool create_r = false )
    {
      PathInfo lockfile( mutexFile_r );

      if ( lockfile.userMayRW() )
	return lockfile.path();	// if admin manually adjusted rw for non-root users: we lock

      if ( ! lockfile.isExist() && create_r && filesystem::assert_file( lockfile.path(), 0644 ) == 0 )
      {
	addmod( lockfile.path(), 0660 );
	return lockfile.path();	// successfully created even for non-root: we lock
      }

      return( geteuid() == 0 ? lockfile.path() : base::InterProcessMutex::fakeLockPath() );
    }

    /** \relates IPMutex Create the underlying mutex file in necessary (fake for non-root). */
    Pathname IPMutexCreateLockFile( const Pathname & mutexFile_r, const Pathname & sysroot_r = Pathname() )
    {
#warning probably obsolete ZYPP_LOCKFILE_ROOT
      // ZYPP_LOCKFILE_ROOT was used as we needed to know the systems root at the
      // time the global lock was created. If locks are created at demand this is
      // probably no longer necessary.
      return IPMutexUseLockFile( (sysroot_r.empty() ? env::ZYPP_LOCKFILE_ROOT() : sysroot_r) / mutexFile_r, /*create*/true );
    }

  } // namespace
  ///////////////////////////////////////////////////////////////////

  IPMutex::IPMutex()
  : base::InterProcessMutex( IPMutexCreateLockFile( defaultMutexFile() ) )
  {}

  IPMutex::IPMutex( const std::string & mutexName_r )
  : base::InterProcessMutex( IPMutexCreateLockFile( makeMutexFile( mutexName_r ) ) )
  {}


  IPMutex::IPMutex( base::InterProcessMutex && mutex_r )
  : base::InterProcessMutex( mutex_r )
  {}


  IPMutex IPMutex::rooted( const Pathname & sysroot_r )
  { return base::InterProcessMutex( IPMutexCreateLockFile( defaultMutexFile(), sysroot_r ) ); }

  IPMutex IPMutex::rooted( const Pathname & sysroot_r, const std::string & mutexName_r  )
  { return base::InterProcessMutex( IPMutexCreateLockFile( makeMutexFile( mutexName_r ), sysroot_r ) ); }


  IPMutex IPMutex::usepath( const Pathname & mutexPath_r )
  { return base::InterProcessMutex( IPMutexUseLockFile( mutexPath_r ) ); }

} // namespace zypp
///////////////////////////////////////////////////////////////////

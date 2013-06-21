/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file	zypp/IPMutex.h
 */
#ifndef ZYPP_IPMUTEX_H
#define ZYPP_IPMUTEX_H

#include <iosfwd>

#include "zypp/base/InterProcessMutex.h"

///////////////////////////////////////////////////////////////////
namespace zypp
{
  ///////////////////////////////////////////////////////////////////
  /// \class IPMutex
  /// \brief Common interprocess mutex
  ///
  /// Commom mutex to synchronize filesystem access across different
  /// processes (not threads!) using libzypp. The underlying mutex
  /// files for root are created in \c /var/run/zypp/ below the
  /// directory specified as system root. The common mutex file
  /// name is \c common.lock.
  ///
  /// Locking for non-root user is faked as it would require read/write
  /// access to the mutex file.
  ///
  /// \see \ref base::InterProcessMutex for details
  ///////////////////////////////////////////////////////////////////
  class IPMutex : public base::InterProcessMutex
  {
  public:
    /** Aquire and automatically release a sharable lock.
     * \see \ref base::InterProcessMutex::Lock for details
     */
    using base::InterProcessMutex::SharableLock;

    /** Aquire and automatically release an exclusive lock.
     * \see \ref base::InterProcessMutex::Lock for details
     */
    using base::InterProcessMutex::ScopedLock;

    /** Callback sending keepalive while waiting to obtain a lock.
     * \see \ref base::InterProcessMutex for details
     */
    using base::InterProcessMutex::LockReport;

    /** Base class for lock exceptions.
     * \see \ref base::InterProcessMutex for details
     */
    using base::InterProcessMutex::LockException;

  public:
    /** Common mutex to synchronize filesystem access (guess the systems root). */
    IPMutex();

    /** Special purpose mutex (or \c common.lock if empty; guess the systems root). */
    explicit IPMutex( const std::string & mutexName_r );

  public:
    /** \name Construct mutexes for rooted systems. */
    //@{
      /** Common mutex to synchronize filesystem access (assume system root at \a sysroot_r). */
      static IPMutex rooted( const Pathname & sysroot_r );

      /** Special purpose mutex (\c common.lock if empty; assume system root at \a sysroot_r). */
      static IPMutex rooted( const Pathname & sysroot_r, const std::string & mutexName_r  );
    //@}

  public:
    /** \name Use a user defined mutex file.
     * \note In contrast to the other ctors the userdefined
     * mutex file is not created, but must exist.
     */
    //@{
      /** Use the user defined mutex file at \a mutexPath_r. */
      static IPMutex usepath( const Pathname & mutexPath_r );

      /** Use the user defined mutex file at \a sysroot_r/mutexPath_r. */
      static IPMutex usepath( const Pathname & sysroot_r, const Pathname & mutexPath_r )
      { return usepath( sysroot_r / mutexPath_r ); }
    //@}

  private:
    IPMutex( base::InterProcessMutex && mutex_r );
  };
  ///////////////////////////////////////////////////////////////////

} // namespace zypp
///////////////////////////////////////////////////////////////////
#endif // ZYPP_IPMUTEX_H

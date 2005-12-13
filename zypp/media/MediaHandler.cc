/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file zypp/media/MediaHandler.cc
 *
*/

#include <iostream>
#include <fstream>
#include <sstream>

#include "zypp/base/Logger.h"
#include "zypp/base/String.h"
#include "zypp/media/MediaHandler.h"

using namespace std;

// use directory.yast on every media (not just via ftp/http)
#define NONREMOTE_DIRECTORY_YAST 1

namespace zypp {
  namespace media {

///////////////////////////////////////////////////////////////////
//
//	CLASS NAME : MediaHandler
//
///////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////
//
//
//	METHOD NAME : MediaHandler::MediaHandler
//	METHOD TYPE : Constructor
//
//	DESCRIPTION :
//
MediaHandler::MediaHandler ( const Url &      url_r,
			     const Pathname & attach_point_r,
			     const Pathname & urlpath_below_attachpoint_r,
			     const bool       does_download_r )
    : _attachPoint( attach_point_r )
    , _tmp_attachPoint( false )
    , _does_download( does_download_r )
    , _isAttached( false )
    , _url( url_r )
{
  if ( _attachPoint.empty() ) {
    ///////////////////////////////////////////////////////////////////
    // provide a default attachpoint
    ///////////////////////////////////////////////////////////////////

    Pathname aroot;
    PathInfo adir;
    const char * defmounts[] = { "/var/adm/mount", "/var/tmp", /**/NULL/**/ };
    for ( const char ** def = defmounts; *def; ++def ) {
      adir( *def );
      if ( adir.isDir() && adir.userMayRWX() ) {
	aroot = adir.path();
	break;
      }
    }
    if ( aroot.empty() ) {
      ERR << "Create attach point: Can't find a writable directory to create an attach point" << endl;
      return;
    }

    Pathname abase( aroot + "AP_" );
    Pathname apoint;

    for ( unsigned i = 1; i < 1000; ++i ) {
      adir( Pathname::extend( abase, str::hexstring( i ) ) );
      if ( ! adir.isExist() && mkdir( adir.path() ) == 0 ) {
	apoint = adir.path();
	break;
      }
    }

    if ( apoint.empty() ) {
      ERR << "Unable to create an attach point below " << aroot << endl;
      return;
    }

    // success
    _attachPoint = apoint;
    _tmp_attachPoint = true;
    MIL << "Created default attach point " << _attachPoint << endl;

  } else {
    ///////////////////////////////////////////////////////////////////
    // check if provided attachpoint is usable.
    ///////////////////////////////////////////////////////////////////
    PathInfo adir( _attachPoint );
    if ( !adir.isDir() ) {
      ERR << "Provided attach point is not a directory: " << adir << endl;
      _attachPoint = Pathname();
    }

  }

  ///////////////////////////////////////////////////////////////////
  // must init _localRoot after _attachPoint is determined.
  ///////////////////////////////////////////////////////////////////

  if ( !_attachPoint.empty() ) {
    _localRoot = _attachPoint + urlpath_below_attachpoint_r;
  }
}

///////////////////////////////////////////////////////////////////
//
//
//	METHOD NAME : MediaHandler::~MediaHandler
//	METHOD TYPE : Destructor
//
//	DESCRIPTION :
//
MediaHandler::~MediaHandler()
{
  if ( _isAttached ) {
    INT << "MediaHandler deleted with media attached." << endl;
    return; // no cleanup if media still mounted!
  }

  if ( _tmp_attachPoint ) {
    int res = recursive_rmdir( _attachPoint );
    if ( res == 0 ) {
      MIL << "Deleted default attach point " << _attachPoint << endl;
    } else {
      ERR << "Failed to Delete default attach point " << _attachPoint
	<< " errno(" << res << ")" << endl;
    }
  }
}

///////////////////////////////////////////////////////////////////
//
//
//	METHOD NAME : MediaHandler::attach
//	METHOD TYPE : PMError
//
//	DESCRIPTION :
//
void MediaHandler::attach( bool next )
{
  if ( _isAttached )
    return;

  if ( _attachPoint.empty() ) {
    ERR << "Error::E_bad_attachpoint" << endl;
    ZYPP_THROW( MediaException("Error::E_bad_attachpoint") );
  }

  try {
    attachTo( next ); // pass to concrete handler
  }
  catch (const MediaException & excpt_r)
  {
    WAR << "Attach failed: " << excpt_r << " " << *this << endl;
    ZYPP_RETHROW(excpt_r);
  }
  _isAttached = true;
  MIL << "Attached: " << *this << endl;
}




///////////////////////////////////////////////////////////////////
//
//
//	METHOD NAME : MediaHandler::localPath
//	METHOD TYPE : Pathname
//
Pathname MediaHandler::localPath( const Pathname & pathname ) const {
    if ( _localRoot.empty() )
        return _localRoot;

    // we must check maximum file name length
    // this is important for fetching the suseservers, the
    // url with all parameters can get too long (bug #42021)

    return _localRoot + pathname.absolutename();
}





///////////////////////////////////////////////////////////////////
//
//
//	METHOD NAME : MediaHandler::disconnect
//	METHOD TYPE : PMError
//
void MediaHandler::disconnect()
{
  if ( !_isAttached )
    return;

  try {
    disconnectFrom(); // pass to concrete handler
  }
  catch (const MediaException & excpt_r)
  {
    WAR << "Disconnect failed: " << excpt_r << " " << *this << endl;
    ZYPP_RETHROW(excpt_r);
  }
  MIL << "Disconnected: " << *this << endl;
}

///////////////////////////////////////////////////////////////////
//
//
//	METHOD NAME : MediaHandler::release
//	METHOD TYPE : PMError
//
//	DESCRIPTION :
//
void MediaHandler::release( bool eject )
{
  if ( !_isAttached ) {
    if ( eject )
      forceEject();
    return;
  }

  try {
    releaseFrom( eject ); // pass to concrete handler
  }
  catch (const MediaException & excpt_r)
  {
    WAR << "Release failed: " << excpt_r << " " << *this << endl;
    ZYPP_RETHROW(excpt_r);
  }
  _isAttached = false;
  MIL << "Released: " << *this << endl;
}

///////////////////////////////////////////////////////////////////
//
//
//	METHOD NAME : MediaHandler::provideFile
//	METHOD TYPE : PMError
//
//	DESCRIPTION :
//
void MediaHandler::provideFileCopy( Pathname srcFilename,
                                       Pathname targetFilename ) const
{
  if ( !_isAttached ) {
    INT << "Error::E_not_attached" << " on provideFileCopy(" << srcFilename
        << "," << targetFilename << ")" << endl;
    ZYPP_THROW(MediaNotAttachedException(url()));
  }

  try {
    getFileCopy( srcFilename, targetFilename ); // pass to concrete handler
  }
  catch (const MediaException & excpt_r)
  {
    WAR << "provideFileCopy(" << srcFilename << "," << targetFilename << "): " << excpt_r << endl;
    ZYPP_RETHROW(excpt_r);
  }
  DBG << "provideFileCopy(" << srcFilename << "," << targetFilename  << ")" << endl;
}

void MediaHandler::provideFile( Pathname filename ) const
{
  if ( !_isAttached ) {
    INT << "Error: Not attached on provideFile(" << filename << ")" << endl;
    ZYPP_THROW(MediaNotAttachedException(url()));
  }

  try {
    getFile( filename ); // pass to concrete handler
  }
  catch (const MediaException & excpt_r)
  {
    WAR << "provideFile(" << filename << "): " << excpt_r << endl;
    ZYPP_RETHROW(excpt_r);
  }
  DBG << "provideFile(" << filename << ")" << endl;
}


///////////////////////////////////////////////////////////////////
//
//
//	METHOD NAME : MediaHandler::provideDir
//	METHOD TYPE : PMError
//
//	DESCRIPTION :
//
void MediaHandler::provideDir( Pathname dirname ) const
{
  if ( !_isAttached ) {
    INT << "Error: Not attached on provideDir(" << dirname << ")" << endl;
    ZYPP_THROW(MediaNotAttachedException(url()));
  }
  try {
    getDir( dirname, /*recursive*/false ); // pass to concrete handler
  }
  catch (const MediaException & excpt_r)
  {
    WAR << "provideDir(" << dirname << "): " << excpt_r << endl;
    ZYPP_RETHROW(excpt_r);
  }
  MIL << "provideDir(" << dirname << ")" << endl;
}

///////////////////////////////////////////////////////////////////
//
//
//	METHOD NAME : MediaHandler::provideDirTree
//	METHOD TYPE : PMError
//
//	DESCRIPTION :
//
void MediaHandler::provideDirTree( Pathname dirname ) const
{
  if ( !_isAttached ) {
    INT << "Error Not attached on provideDirTree(" << dirname << ")" << endl;
    ZYPP_THROW(MediaNotAttachedException(url()));
  }

  try {
    getDir( dirname, /*recursive*/true ); // pass to concrete handler
  }
  catch (const MediaException & excpt_r)
  {
    WAR << "provideDirTree(" << dirname << "): " << excpt_r << endl;
    ZYPP_RETHROW(excpt_r);
  }

  MIL << "provideDirTree(" << dirname << ")" << endl;
}

///////////////////////////////////////////////////////////////////
//
//
//	METHOD NAME : MediaHandler::releasePath
//	METHOD TYPE : PMError
//
//	DESCRIPTION :
//
void MediaHandler::releasePath( Pathname pathname ) const
{
  if ( ! _does_download || _attachPoint.empty() )
    return;

  PathInfo info( localPath( pathname ) );

  if ( info.isFile() ) {
    unlink( info.path() );
  } else if ( info.isDir() ) {
    if ( info.path() != _localRoot ) {
      recursive_rmdir( info.path() );
    } else {
      clean_dir( info.path() );
    }
  }
}

///////////////////////////////////////////////////////////////////
//
//
//	METHOD NAME : MediaHandler::dirInfo
//	METHOD TYPE : PMError
//
//	DESCRIPTION :
//
void MediaHandler::dirInfo( list<string> & retlist,
			       const Pathname & dirname, bool dots ) const
{
  retlist.clear();

  if ( !_isAttached ) {
    INT << "Error: Not attached on dirInfo(" << dirname << ")" << endl;
    ZYPP_THROW(MediaNotAttachedException(url()));
  }

  try {
    getDirInfo( retlist, dirname, dots ); // pass to concrete handler
  }
  catch (const MediaException & excpt_r)
  {
    WAR << "dirInfo(" << dirname << "): " << excpt_r << endl;
    ZYPP_RETHROW(excpt_r);
  }
  MIL << "dirInfo(" << dirname << ")" << endl;
}

///////////////////////////////////////////////////////////////////
//
//
//	METHOD NAME : MediaHandler::dirInfo
//	METHOD TYPE : PMError
//
//	DESCRIPTION :
//
void MediaHandler::dirInfo( filesystem::DirContent & retlist,
                            const Pathname & dirname, bool dots ) const
{
  retlist.clear();

  if ( !_isAttached ) {
    INT << "Error: Not attached on dirInfo(" << dirname << ")" << endl;
    ZYPP_THROW(MediaNotAttachedException(url()));
  }

  try {
    getDirInfo( retlist, dirname, dots ); // pass to concrete handler
  }
  catch (const MediaException & excpt_r)
  {
    WAR << "dirInfo(" << dirname << "): " << excpt_r << endl;
    ZYPP_RETHROW(excpt_r);
  }
  MIL << "dirInfo(" << dirname << ")" << endl;
}

///////////////////////////////////////////////////////////////////
//
//
//	METHOD NAME : MediaHandler::getDirectoryYast
//	METHOD TYPE : PMError
//
void MediaHandler::getDirectoryYast( std::list<std::string> & retlist,
					const Pathname & dirname, bool dots ) const
{
  retlist.clear();

  filesystem::DirContent content;
  try {
    getDirectoryYast( content, dirname, dots );
  }
  catch (const MediaException & excpt_r)
  {
    ZYPP_RETHROW(excpt_r);
  }

  // convert to std::list<std::string>
  for ( filesystem::DirContent::const_iterator it = content.begin(); it != content.end(); ++it ) {
    retlist.push_back( it->name );
  }
}

///////////////////////////////////////////////////////////////////
//
//
//	METHOD NAME : MediaHandler::getDirectoryYast
//	METHOD TYPE : PMError
//
void MediaHandler::getDirectoryYast( filesystem::DirContent & retlist,
                                     const Pathname & dirname, bool dots ) const
{
  retlist.clear();

  // look for directory.yast
  Pathname dirFile = dirname + "directory.yast";
  try {
    getFile( dirFile );
  }
  catch (const MediaException & excpt_r)
  {
    ERR << "provideFile(" << dirFile << "): " << excpt_r << endl;
    ZYPP_RETHROW(excpt_r);
  }
  DBG << "provideFile(" << dirFile << "): " << "OK" << endl;

  // using directory.yast
  ifstream dir( localPath( dirFile ).asString().c_str() );
  if ( dir.fail() ) {
    ERR << "Unable to load '" << localPath( dirFile ) << "'" << endl;
    ZYPP_THROW(MediaSystemException(url(),
      "Unable to load '" + localPath( dirFile ).asString() + "'"));
  }

  string line;
  while( getline( dir, line ) ) {
    if ( line.empty() ) continue;
    if ( line == "directory.yast" ) continue;

    // Newer directory.yast append '/' to directory names
    // Remaining entries are unspecified, although most probabely files.
    filesystem::FileType type = filesystem::FT_NOT_AVAIL;
    if ( *line.rbegin() == '/' ) {
      line.erase( line.end()-1 );
      type = filesystem::FT_DIR;
    }

    if ( dots ) {
      if ( line == "." || line == ".." ) continue;
    } else {
      if ( *line.begin() == '.' ) continue;
    }

    retlist.push_back( filesystem::DirEntry( line, type ) );
  }
}

/******************************************************************
**
**
**	FUNCTION NAME : operator<<
**	FUNCTION TYPE : ostream &
*/
ostream & operator<<( ostream & str, const MediaHandler & obj )
{
  str << obj.url() << ( obj.isAttached() ? "" : " not" )
    << " attached; localRoot \"" << obj.localRoot() << "\"";
  return str;
}

///////////////////////////////////////////////////////////////////
//
//
//	METHOD NAME : MediaHandler::getFile
//	METHOD TYPE : PMError
//
//	DESCRIPTION : Asserted that media is attached.
//                    Default implementation of pure virtual.
//
void MediaHandler::getFile( const Pathname & filename ) const
{
    PathInfo info( localPath( filename ) );
    if( info.isFile() ) {
        return;
    }

    if (info.isExist())
      ZYPP_THROW(MediaNotAFileException(url(), localPath(filename)));
    else
      ZYPP_THROW(MediaFileNotFoundException(url(), filename));
}


void MediaHandler::getFileCopy ( const Pathname & srcFilename, const Pathname & targetFilename ) const
{
  try {
    getFile(srcFilename);
  }
  catch (const MediaException & excpt_r)
  {
    ZYPP_RETHROW(excpt_r);
  }

  if ( copy( localPath( srcFilename ), targetFilename ) != 0 ) {
    ZYPP_THROW(MediaWriteException(targetFilename));
  }
}



///////////////////////////////////////////////////////////////////
//
//
//	METHOD NAME : MediaHandler::getDir
//	METHOD TYPE : PMError
//
//	DESCRIPTION : Asserted that media is attached.
//                    Default implementation of pure virtual.
//
void MediaHandler::getDir( const Pathname & dirname, bool recurse_r ) const
{
  PathInfo info( localPath( dirname ) );
  if( info.isDir() ) {
    return;
  }

  if (info.isExist())
    ZYPP_THROW(MediaNotADirException(url(), localPath(dirname)));
  else
    ZYPP_THROW(MediaFileNotFoundException(url(), dirname));
}

///////////////////////////////////////////////////////////////////
//
//
//	METHOD NAME : MediaHandler::getDirInfo
//	METHOD TYPE : PMError
//
//	DESCRIPTION : Asserted that media is attached and retlist is empty.
//                    Default implementation of pure virtual.
//
void MediaHandler::getDirInfo( std::list<std::string> & retlist,
                               const Pathname & dirname, bool dots ) const
{
  PathInfo info( localPath( dirname ) );
  if( ! info.isDir() ) {
    ZYPP_THROW(MediaNotADirException(url(), localPath(dirname)));
  }

#if NONREMOTE_DIRECTORY_YAST
  // use directory.yast if available
  try {
    getDirectoryYast( retlist, dirname, dots );
  }
  catch (const MediaException & excpt_r)
  {
#endif

  // readdir
    int res = readdir( retlist, info.path(), dots );
  if ( res )
    ZYPP_THROW(MediaSystemException(url(), "readdir failed"));

#if NONREMOTE_DIRECTORY_YAST
  }
#endif

  return;
}

///////////////////////////////////////////////////////////////////
//
//
//	METHOD NAME : MediaHandler::getDirInfo
//	METHOD TYPE : PMError
//
//	DESCRIPTION : Asserted that media is attached and retlist is empty.
//                    Default implementation of pure virtual.
//
void MediaHandler::getDirInfo( filesystem::DirContent & retlist,
                               const Pathname & dirname, bool dots ) const
{
  PathInfo info( localPath( dirname ) );
  if( ! info.isDir() ) {
    ZYPP_THROW(MediaNotADirException(url(), localPath(dirname)));
  }

#if NONREMOTE_DIRECTORY_YAST
  // use directory.yast if available
  try {
    getDirectoryYast( retlist, dirname, dots );
  }
  catch (const MediaException & excpt_r)
  {
#endif

  // readdir
  int res = readdir( retlist, info.path(), dots );
  if ( res )
    ZYPP_THROW(MediaSystemException(url(), "readdir failed"));
#if NONREMOTE_DIRECTORY_YAST
  }
#endif
}

  } // namespace media
} // namespace zypp

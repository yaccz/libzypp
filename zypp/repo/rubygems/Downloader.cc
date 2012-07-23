
#include <iostream>
#include <fstream>

#include "zypp/base/LogTools.h"
#include "zypp/base/String.h"
#include "zypp/base/Regex.h"
#include "zypp/OnMediaLocation.h"
#include "zypp/MediaSetAccess.h"
#include "zypp/Fetcher.h"
#include "zypp/Locale.h"
#include "zypp/ZConfig.h"
#include "zypp/repo/MediaInfoDownloader.h"
#include "zypp/repo/rubygems/Downloader.h"
#include "zypp/parser/ParseException.h"
#include "zypp/base/UserRequestException.h"
#include "zypp/KeyContext.h" // for SignatureFileChecker

using std::endl;

namespace zypp
{
  namespace repo
  {
    namespace rubygems
    {

      Downloader::Downloader( const RepoInfo &repoinfo, const Pathname &delta_dir )
      : repo::Downloader(repoinfo), _delta_dir(delta_dir)
      {
      }

      RepoStatus Downloader::status( MediaSetAccess &media )
      {
	Pathname content = media.provideFile( repoInfo().path() + "/Marshal.4.8.Z");
	return RepoStatus(content);
      }

      // search old repository file file to run the delta algorithm on
      static Pathname search_deltafile( const Pathname &dir, const Pathname &file )
      {

	Pathname deltafile(dir + file.basename());
	if (PathInfo(deltafile).isExist())
	  return deltafile;
	return Pathname();
      }

      void Downloader::download( MediaSetAccess & media,
				 const Pathname & dest_dir,
				 const ProgressData::ReceiverFnc & progress )
      {
	if ( ! repoInfo().gpgCheck() )
	{
	  WAR << "Signature checking disabled in config of repository " << repoInfo().alias() << endl;
	}
	enqueue( OnMediaLocation( repoInfo().path() + "/Marshal.4.8.Z", 1 ),
		 FileChecker(NullFileChecker()) );
		 start( dest_dir, media );
		 reset();
      }

    } // ns rubygems
  } // ns source
} // ns zypp

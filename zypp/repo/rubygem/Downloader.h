/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/

#ifndef ZYPP_SOURCE_RUBYGEM_DOWNLOADER
#define ZYPP_SOURCE_RUBYGEM_DOWNLOADER

#include "zypp/Url.h"
#include "zypp/Pathname.h"
#include "zypp/ProgressData.h"
#include "zypp/RepoInfo.h"
#include "zypp/RepoStatus.h"
#include "zypp/MediaSetAccess.h"
#include "zypp/repo/Downloader.h"

namespace zypp
{
  namespace repo
  {
    namespace rubygem
    {

      /**
       * \short Downloader for rubygem repositories
       * Encapsulates all the knowledge of which files have
       * to be downloaded to the local disk.
       */
      class Downloader : public repo::Downloader
      {
      public:
        /**
         * \short Constructor from the repository information
         *
         * The repository information allows more context to be given
         * to the user when something fails.
         *
         * \param info Repository information
         */
        Downloader( const RepoInfo &info, const Pathname &delta_dir = Pathname() );

        /**
         * \short Download metadata to a local directory
         *
         * \param media Media access to the repository url
         * \param dest_dir Local destination directory
         * \param progress progress receiver
         */
        virtual void download( MediaSetAccess &media,
			       const Pathname &dest_dir,
			       const ProgressData::ReceiverFnc & progress = ProgressData::ReceiverFnc() );
        /**
         * \short Status of the remote repository
         */
        virtual RepoStatus status( MediaSetAccess &media );

      private:
	Pathname _delta_dir;
      };

    } // ns rubygem
  } // ns source
} // ns zypp

#endif // ZYPP_SOURCE_RUBYGEM_DOWNLOADER

/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file zypp/source/yum/YUMPackageImpl.cc
 *
*/

#include "zypp/source/yum/YUMPackageImpl.h"
#include "zypp/base/String.h"
#include "zypp/base/Logger.h"

using namespace std;
using namespace zypp::detail;
using namespace zypp::parser::yum;

///////////////////////////////////////////////////////////////////
namespace zypp
{ /////////////////////////////////////////////////////////////////
  ///////////////////////////////////////////////////////////////////
  namespace source
  { /////////////////////////////////////////////////////////////////
    namespace yum
    {
      ///////////////////////////////////////////////////////////////////
      //
      //        CLASS NAME : YUMPackageImpl
      //
      ///////////////////////////////////////////////////////////////////

      /** Default ctor
      */
      YUMPackageImpl::YUMPackageImpl(
	Source_Ref source_r,
	const zypp::parser::yum::YUMPrimaryData & parsed,
	const zypp::parser::yum::YUMFileListData & filelist,
	const zypp::parser::yum::YUMOtherData & other
      )
      : _summary(parsed.summary),
	_description(),
	_buildtime(strtol(parsed.timeBuild.c_str(), 0, 10)),
	_buildhost(parsed.buildhost),
	_url(parsed.url),
	_vendor( parsed.vendor),
	_license( parsed.license),
	_packager(parsed.packager),
	_group(parsed.group),
	_changelog(), // TODO
	_type(parsed.type),
	_license_to_confirm(), // TODO add to metadata
	_authors(parsed.authors),
	_keywords( parsed.keywords),
	_mediaid(strtol(parsed.media.c_str(), 0, 10)),
	_checksum(parsed.checksumType,
		  parsed.checksum),
	_filenames(),
	_location(parsed.location),
	_delta_rpms(),
	_patch_rpms(),

	_install_only(parsed.installOnly),
	_source(source_r)
#if 0
      : _size_package(strtol(parsed.sizePackage.c_str(), 0, 10)),
	_size_archive(strtol(parsed.sizeArchive.c_str(), 0, 10)),
	_size_installed(strtol(parsed.sizeInstalled.c_str(), 0, 10)),
	_sourcepkg(parsed.sourcerpm),
	_dir_sizes(parsed.dirSizes),
#endif
      {
	_description.setText(parsed.description);
	_license_to_confirm = parsed.license_to_confirm.asString();
	for (std::list<FileData>::const_iterator it = filelist.files.begin();
	     it != filelist.files.end();
	     it++)
	{
	  _filenames.push_back(it->name);
	}
	for (std::list<zypp::parser::yum::ChangelogEntry>::const_iterator
		it = other.changelog.begin();
	     it != other.changelog.end();
	     it++)
	{
	  _changelog.push_back(ChangelogEntry(strtol(it->date.c_str(), 0, 10),
					      it->author,
					      it->entry));
	}
      }

      YUMPackageImpl::YUMPackageImpl(
	Source_Ref source_r,
	const zypp::parser::yum::YUMPatchPackage & parsed
      )
      : _summary(parsed.summary),
	_description(),
	_buildtime(strtol(parsed.timeBuild.c_str(), 0, 10)),
	_buildhost(parsed.buildhost),
	_url(parsed.url),
	_vendor( parsed.vendor),
	_license( parsed.license),
	_packager( parsed.packager),
	_group(parsed.group),
	_changelog(), // TODO
	_type(parsed.type),
	_license_to_confirm(),
	_authors(parsed.authors),
	_keywords( parsed.keywords),
	_mediaid(strtol(parsed.media.c_str(), 0, 10)),
	_checksum(parsed.checksumType,
		  parsed.checksum),
	_filenames(),
  _location( parsed.plainRpms.empty() ? Pathname() : parsed.plainRpms.front().filename),
	_delta_rpms(),
	_patch_rpms(),

	_install_only(parsed.installOnly),
	_source(source_r)
#if 0
      : _size_package( strtol(parsed.sizePackage.c_str(), 0, 10)),
	_size_archive( strtol(parsed.sizeArchive.c_str(), 0, 10)),
	_size_installed( strtol(parsed.sizeInstalled.c_str(), 0, 10)),
	_sourcepkg( parsed.sourcerpm),
	_dir_sizes(parsed.dirSizes),
#endif
      {
	_description.setText(parsed.description);
	_license_to_confirm = parsed.license_to_confirm.asString();
	for (std::list<FileData>::const_iterator it = parsed.files.begin();
	     it != parsed.files.end();
	     it++)
	{
	  _filenames.push_back(it->name);
	}
	for (std::list<zypp::parser::yum::YUMPatchRpm>::const_iterator it
		= parsed.patchRpms.begin();
	     it != parsed.patchRpms.end();
	     it++)
	{
	  std::list<BaseVersion> base_versions;
	  for (std::list<YUMBaseVersion>::const_iterator bit
			= it->baseVersions.begin();
	       bit != it->baseVersions.end();
	       bit++)
	  {
	    Edition base_edition(bit->ver, bit->rel, bit->epoch);
	    BaseVersion base_version(base_edition,
				     CheckSum("md5", bit->md5sum),
				     strtol(bit->buildtime.c_str(), 0, 10));
	    base_versions.push_back(base_version);
	  }
	  PatchRpm patch(Arch(it->arch),
			 it->filename,
			 strtol(it->downloadsize.c_str(), 0, 10),
			 CheckSum("md5", it->md5sum),
			 strtol(it->buildtime.c_str(), 0, 10),
			 base_versions
	  );
	  _patch_rpms.push_back(patch);

	}
	for (std::list<zypp::parser::yum::YUMDeltaRpm>::const_iterator it
		= parsed.deltaRpms.begin();
	     it != parsed.deltaRpms.end();
	     it++)
	{
	  Edition base_edition(it->baseVersion.ver,
			       it->baseVersion.rel,
			       it->baseVersion.epoch);
	  BaseVersion base_version(base_edition,
				   CheckSum("md5", it->baseVersion.md5sum),
				   strtol(it->baseVersion.buildtime.c_str(),
					0, 10));
	  DeltaRpm delta(Arch(it->arch),
			 it->filename,
			 strtol(it->downloadsize.c_str(), 0, 10),
			 CheckSum("md5", it->md5sum),
			 strtol(it->buildtime.c_str(), 0, 10),
			 base_version
	  );
	  _delta_rpms.push_back(delta);
	}
	for (std::list<zypp::parser::yum::ChangelogEntry>::const_iterator
		it = parsed.changelog.begin();
	     it != parsed.changelog.end();
	     it++)
	{
	  _changelog.push_back(ChangelogEntry(strtol(it->date.c_str(), 0, 10),
					      it->author,
					      it->entry));
	}
      }


      /** Package summary */
      TranslatedText YUMPackageImpl::summary() const
      { return _summary; }

      /** Package description */
      TranslatedText YUMPackageImpl::description() const
      { return _description; }

      ByteCount YUMPackageImpl::size() const
#warning fixme
      { return 0; }

      /** */
      Date YUMPackageImpl::buildtime() const
      { return _buildtime; }

      /** */
      std::string YUMPackageImpl::buildhost() const
      { return _buildhost; }

      /** */
      Date YUMPackageImpl::installtime() const
      { return PackageImplIf::installtime(); }

      /** */
      std::string YUMPackageImpl::distribution() const
#warning fixme
      { return string(); }

      /** */
      Vendor YUMPackageImpl::vendor() const
      { return _vendor; }

      /** */
      Label YUMPackageImpl::license() const
      { return _license; }

      /** */
      std::string YUMPackageImpl::packager() const
      { return _packager; }

      /** */
      PackageGroup YUMPackageImpl::group() const
      { return _group; }

      /** */
      Changelog YUMPackageImpl::changelog() const
      { return _changelog; }

      /** */
      Pathname YUMPackageImpl::location() const
      { return _location; }

      /** Don't ship it as class Url, because it might be
       * in fact anything but a legal Url. */
      std::string YUMPackageImpl::url() const
      { return _url; }

      /** */
      std::string YUMPackageImpl::os() const
      // metadata doesn't priovide this attribute
      { return PackageImplIf::os(); }

      /** */
      Text YUMPackageImpl::prein() const
      // metadata doesn't priovide this attribute
      { return PackageImplIf::prein(); }

      /** */
      Text YUMPackageImpl::postin() const
      // metadata doesn't priovide this attribute
      { return PackageImplIf::postin(); }

      /** */
      Text YUMPackageImpl::preun() const
      // metadata doesn't priovide this attribute
      { return PackageImplIf::preun(); }

      /** */
      Text YUMPackageImpl::postun() const
      // metadata doesn't priovide this attribute
      { return PackageImplIf::postun(); }

      /** */
      ByteCount YUMPackageImpl::sourcesize() const
#warning fixme
      { return 0; }

      /** */
      ByteCount YUMPackageImpl::archivesize() const
#warning fixme
      { return 0; }

      /** */
      std::list<std::string> YUMPackageImpl::authors() const
      { return _authors; }

      /** */
      std::list<std::string> YUMPackageImpl::filenames() const
      { return _filenames; }

      License YUMPackageImpl::licenseToConfirm() const
      { return _license_to_confirm; }

      /** */
      std::string YUMPackageImpl::type() const
      { return _type; }

      /** */
      std::list<std::string> YUMPackageImpl::keywords() const
      { return _keywords; }

      bool YUMPackageImpl::installOnly() const
      { return _install_only; }

      unsigned YUMPackageImpl::mediaId() const
      { return _mediaid; }

      CheckSum YUMPackageImpl::checksum() const
      { return _checksum; }

      std::list<DeltaRpm> YUMPackageImpl::deltaRpms() const
      { return _delta_rpms; }

      std::list<PatchRpm> YUMPackageImpl::patchRpms() const
      { return _patch_rpms; }

      Source_Ref YUMPackageImpl::source() const
      { return _source; }

#if 0
      /** */
      unsigned YUMPackageImpl::packageSize() const
      { return _size_package; }
      /** */
      unsigned YUMPackageImpl::archiveSize() const
      { return _size_archive; }
      /** */
      unsigned YUMPackageImpl::installedSize() const
      { return _size_installed; }
// FIXME do not understand items below
      /** */
      bool YUMPackageImpl::providesSources() const
      {
	return false;
      }
      /** */
      std::string YUMPackageImpl::instSrcLabel() const
      {
	return "";
      }
      /** */
      std::string YUMPackageImpl::instSrcVendor() const
      {
	return "";
      }
      /** */
      unsigned YUMPackageImpl::instSrcRank() const
      {
	return 0;
      }
      /** */
      std::string YUMPackageImpl::buildhost() const
      {
	return _buildhost;
      }
      /** */
      std::string YUMPackageImpl::distribution() const
      {
	return "";
      }
      /** */
      std::string YUMPackageImpl::vendor() const
      {
	return _vendor;
      }
      /** */
      std::string YUMPackageImpl::license() const
      {
	return _license;
      }
      /** */
      std::list<std::string> YUMPackageImpl::licenseToConfirm() const
      {
	return std::list<std::string>();
      }
      /** */
      std::string YUMPackageImpl::packager() const
      {
	return _packager;
      }
      /** */
      std::string YUMPackageImpl::group() const
      {
	return _group;
      }
      /** */
      std::list<std::string> YUMPackageImpl::changelog() const
      {}
      /** */
      std::string YUMPackageImpl::url() const
      {
	return _url;
      }
      /** */
      std::string YUMPackageImpl::os() const
      {}
      /** */
      std::list<std::string> YUMPackageImpl::prein() const
      {}
      /** */
      std::list<std::string> YUMPackageImpl::postin() const
      {}
      /** */
      std::list<std::string> YUMPackageImpl::preun() const
      {}
      /** */
      std::list<std::string> YUMPackageImpl::postun() const
      {}
      /** */
      std::string YUMPackageImpl::sourcepkg() const
      { return _sourcepkg; }
      /** */
      std::list<std::string> YUMPackageImpl::authors() const
      { return _authors; }
      /** */
      std::list<std::string> YUMPackageImpl::filenames() const
      {}
      /** */
      std::string YUMPackageImpl::location() const
      {}
      /** */
      std::string YUMPackageImpl::md5sum() const
      {}
      /** */
      std::string YUMPackageImpl::externalUrl() const
      {}
      /** */
      std::list<Edition> YUMPackageImpl::patchRpmBaseVersions() const
      {}
      /** */
      unsigned YUMPackageImpl::patchRpmSize() const
      {}
      /** */
      bool YUMPackageImpl::forceInstall() const
      {}
      /** */
      std::string YUMPackageImpl::patchRpmMD5() const
      {}
      /** */
      bool YUMPackageImpl::isRemote() const
      {}
      /** */
      bool YUMPackageImpl::prefererCandidate() const
      {}

#endif

    } // namespace yum
    /////////////////////////////////////////////////////////////////
  } // namespace source
  ///////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////
} // namespace zypp
///////////////////////////////////////////////////////////////////

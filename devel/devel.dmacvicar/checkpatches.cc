#include <iostream>
#include <fstream>
#include <sstream>
#include <streambuf>

#include "boost/filesystem/operations.hpp" // includes boost/filesystem/path.hpp
#include "boost/filesystem/fstream.hpp"    // ditto

#include <boost/iostreams/device/file_descriptor.hpp>

#include <zypp/base/Logger.h>
#include <zypp/Locale.h>
#include <zypp/ZYpp.h>
#include <zypp/ZYppFactory.h>
#include <zypp/zypp_detail/ZYppReadOnlyHack.h>
#include <zypp/SourceManager.h>
#include <zypp/TranslatedText.h>
///////////////////////////////////////////////////////////////////

#include <zypp/parser/yum/YUMParser.h>
#include <zypp/base/Logger.h>
#include <zypp/NVRAD.h>
#include <zypp/target/rpm/RpmDb.h>
#include <zypp/source/yum/YUMScriptImpl.h>
#include <zypp/source/yum/YUMMessageImpl.h>
#include <zypp/source/yum/YUMPackageImpl.h>
#include <zypp/source/yum/YUMSourceImpl.h>

#include <map>
#include <set>

#include <zypp/CapFactory.h>
#include <zypp/KeyRing.h>
#include <zypp/Digest.h>

#include "checkpatches-keyring-callbacks.h"

using namespace zypp::detail;

using namespace std;
using namespace zypp;
using namespace zypp::parser::yum;
using namespace zypp::source::yum;

#define ZYPP_CHECKPATCHES_LOG "/var/log/zypp-checkpatches.log"

//using namespace DbXml;

int main(int argc, char **argv)
{
  const char *logfile = getenv("ZYPP_LOGFILE");
  if (logfile != NULL)
    zypp::base::LogControl::instance().logfile( logfile );
  else
    zypp::base::LogControl::instance().logfile( ZYPP_CHECKPATCHES_LOG );
  
  std::string previous_token;
  
  if (argc > 2)
  {
    cerr << "usage: " << argv[0] << " [<previous token>]" << endl;
    return 1;
  }
  else if ( argc == 2 )
  {
    previous_token = std::string(argv[1]);
  }
  
  SourceManager_Ptr manager;
  manager = SourceManager::sourceManager();
  
  ZYpp::Ptr God = getZYpp();
  KeyRingCallbacks keyring_callbacks;
  DigestCallbacks digest_callbacks;
  
  try
  {
    manager->restore("/");
  }
  catch (Exception & excpt_r)
  {
    ZYPP_CAUGHT (excpt_r);
    ERR << "Couldn't restore sources" << endl;
    return false;
  }
  
  std::string token;
  stringstream token_stream;
  for ( SourceManager::Source_const_iterator it = manager->Source_begin(); it !=  manager->Source_end(); ++it )
  {
    Source_Ref src = manager->findSource(it->alias());
    src.refresh();
    
    token_stream << "[" << src.alias() << "| " << src.url() << src.timestamp() << "]";
    
    MIL << "Source: " << src.alias() << " from " << src.timestamp() << std::endl;  
  }
  //static std::string digest(const std::string& name, std::istream& is
  token = token_stream.str();
  cout << Digest::digest("sha1", token_stream) << std::endl;
  
  if ( token == previous_token )
  {
    cout << 0 << std::endl;
    return 0;
  }
  
  // something changed
  God->initTarget("/");
  for ( SourceManager::Source_const_iterator it = manager->Source_begin(); it !=  manager->Source_end(); ++it )
  {
    // skip non YUM sources for now
    if ( it->type() == "YUM" )
      God->addResolvables(it->resolvables());
  }
  
  God->resolver()->establishPool();
  
  int count = 0;
  int security_count = 0;
  for ( ResPool::byKind_iterator it = God->pool().byKindBegin<Patch>(); it != God->pool().byKindEnd<Patch>(); ++it )
  {
    if ( it->status().isNeeded() )
    {
      Resolvable::constPtr res = it->resolvable();
      Patch::constPtr patch = asKind<Patch>(res);
      count++;
      if (patch->category() == "security")
        security_count++;
      
      cerr << patch->name() << " " << patch->edition() << " " << "[" << patch->category() << "]" << std::endl;
    }
  }
  
  MIL << "Patches " << security_count << " " << count << std::endl;
  
  if ( security_count > 0 )
    return 2;
  
  if ( count > 0 )
    return 1;
  
  return 0;
}



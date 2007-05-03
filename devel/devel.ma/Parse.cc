#include "Tools.h"

#include <zypp/base/PtrTypes.h>
#include <zypp/base/Exception.h>
#include <zypp/base/ProvideNumericId.h>

#include "zypp/ZYppFactory.h"
#include "zypp/ResPoolProxy.h"
#include <zypp/SourceManager.h>
#include <zypp/SourceFactory.h>

#include "zypp/ZYppCallbacks.h"
#include "zypp/NVRAD.h"
#include "zypp/ResPool.h"
#include "zypp/ResFilters.h"
#include "zypp/CapFilters.h"
#include "zypp/Package.h"
#include "zypp/Pattern.h"
#include "zypp/Language.h"
#include "zypp/PackageKeyword.h"
#include "zypp/NameKindProxy.h"
#include "zypp/pool/GetResolvablesToInsDel.h"

#include "zypp/parser/tagfile/TagFileParser.h"
#include "zypp/parser/TagParser.h"
#include "zypp/parser/susetags/PackagesFileReader.h"

using namespace std;
using namespace zypp;
using namespace zypp::functor;

using zypp::parser::tagfile::TagFileParser;
using zypp::parser::TagParser;

///////////////////////////////////////////////////////////////////

static const Pathname sysRoot( "/Local/ROOT" );

///////////////////////////////////////////////////////////////////

struct ConvertDbReceive : public callback::ReceiveReport<target::ScriptResolvableReport>
{
  virtual void start( const Resolvable::constPtr & script_r,
                      const Pathname & path_r,
                      Task task_r )
  {
    SEC << __FUNCTION__ << endl
    << "  " << script_r << endl
    << "  " << path_r   << endl
    << "  " << task_r   << endl;
  }

  virtual bool progress( Notify notify_r, const std::string & text_r )
  {
    SEC << __FUNCTION__ << endl
    << "  " << notify_r << endl
    << "  " << text_r   << endl;
    return true;
  }

  virtual void problem( const std::string & description_r )
  {
    SEC << __FUNCTION__ << endl
    << "  " << description_r << endl;
  }

  virtual void finish()
  {
    SEC << __FUNCTION__ << endl;
  }

};

///////////////////////////////////////////////////////////////////

struct MediaChangeReceive : public callback::ReceiveReport<media::MediaChangeReport>
{
  virtual Action requestMedia( Source_Ref source
                               , unsigned mediumNr
                               , Error error
                               , const std::string & description )
  {
    SEC << __FUNCTION__ << endl
    << "  " << source << endl
    << "  " << mediumNr << endl
    << "  " << error << endl
    << "  " << description << endl;
    return IGNORE;
  }
};

///////////////////////////////////////////////////////////////////

namespace container
{
  template<class _Tp>
    bool isIn( const std::set<_Tp> & cont, const typename std::set<_Tp>::value_type & val )
    { return cont.find( val ) != cont.end(); }
}

///////////////////////////////////////////////////////////////////

struct AddResolvables
{
  bool operator()( const Source_Ref & src ) const
  {
    getZYpp()->addResolvables( src.resolvables() );
    return true;
  }
};

///////////////////////////////////////////////////////////////////


std::ostream & operator<<( std::ostream & str, const iostr::EachLine & obj )
{
  str << "(" << obj.valid() << ")[" << obj.lineNo() << "|" << obj.lineStart() << "]{" << *obj << "}";
  return str;

}

#include "zypp/ProgressData.h"

///////////////////////////////////////////////////////////////////
namespace zypp
{ /////////////////////////////////////////////////////////////////

  ///////////////////////////////////////////////////////////////////
  namespace parser
  { /////////////////////////////////////////////////////////////////
   ///////////////////////////////////////////////////////////////////
    namespace susetags
    { /////////////////////////////////////////////////////////////////

      bool exampleReceiver( ProgressData::value_type v )
      {
	WAR << "got ->" << v << endl;
	return( v <= 100 ); // Abort if ( v > 100
      }

      class Example
      {
	public:

	  Example( const ProgressData::ReceiverFnc & fnc_r = ProgressData::ReceiverFnc() )
	  : _fnc( fnc_r )
	  {}

	  void SendTo( const ProgressData::ReceiverFnc & fnc_r )
	  { _fnc = fnc_r; }

	public:

	  void action()
	  {
	    ProgressData tics( 10 );    // Expect range 0 -> 10
	    tics.name( "test ticks" );  // Some arbitrary name
	    tics.sendTo( _fnc );        // Send reports to _fnc
	    tics.toMin();               // start sending min (0)

	    for ( int i = 0; i < 10; ++i )
	    {
	      if ( ! tics.set( i ) )
		return; // user requested abort
	    }

	    tics.toMax(); // take care 100% are reported on success
	  }

	  void action2()
	  {
	    ProgressData tics;          // Just send 'still alive' messages
	    tics.name( "test ticks" );  // Some arbitrary name
	    tics.sendTo( _fnc );        // Send reports to _fnc
	    tics.toMin();               // start sending min (0)

	    for ( int i = 0; i < 10; ++i )
	    {
	      if ( ! tics.set( i ) )
		return; // user requested abort
	    }

	    tics.toMax(); //
	  }

	private:
	  ProgressData::ReceiverFnc _fnc;
      };


      //ProgressData makeProgressData( const InputStream & input_r )
      //{
      //  ProgressData ret;
      //  ret.name( input_r.name() );
      //  if ( input_r.size() > 0 )
      //    ret.range( input_r.size() );
      //  return ret;
      //}

      void simpleParser( const InputStream & input_r,
			 const ProgressData::ReceiverFnc & fnc_r = ProgressData::ReceiverFnc() )
      {
	ProgressData ticks( makeProgressData( input_r ) );
	ticks.sendTo( fnc_r );
	ticks.toMin(); // start sending min (0)

	iostr::EachLine line( input_r );
	for( ; line; line.next() )
	{
	  /* process the line */

	  if ( ! ticks.set( input_r.stream().tellg() ) )
	    return; // user requested abort
	}

	ticks.toMax(); // take care 100% are reported on success
      }


      /////////////////////////////////////////////////////////////////
    } // namespace susetags
    ///////////////////////////////////////////////////////////////////
    /////////////////////////////////////////////////////////////////
  } // namespace parser
  ///////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////
} // namespace zypp
///////////////////////////////////////////////////////////////////

using namespace zypp::parser::susetags;

/******************************************************************
**
**      FUNCTION NAME : main
**      FUNCTION TYPE : int
*/
int main( int argc, char * argv[] )
{
  //zypp::base::LogControl::instance().logfile( "log.restrict" );
  INT << "===[START]==========================================" << endl;

  TagParser c;
  c.parse( "packages.gz", exampleReceiver );

  return ( 0 );

  //Pathname p( "lmd/suse/setup/descr/packages" );
  Pathname p( "packages" );

  if ( 1 )
  {
    Pathname p( "packages" );
    Measure x( p.basename() );
    TagFileParser tp( (zypp::parser::ParserProgress::Ptr()) );
    tp.parse( p );
  }

  if ( 0 ) {
    Pathname p( "p" );
    Measure x( p.basename() );
    PackagesFileReader tp;
    tp.parse( p );
  }
  if ( 1 ) {
    Pathname p( "p.gz" );
    Measure x( p.basename() );
    PackagesFileReader tp;
    tp.parse( p );
  }
  if ( 1 ) {
    Pathname p( "packages" );
    Measure x( p.basename() );
    PackagesFileReader tp;
    tp.parse( p );
  }
  if ( 1 ) {
    Pathname p( "packages.gz" );
    Measure x( p.basename() );
    PackagesFileReader tp;
    tp.parse( p );
  }

  if ( 0 )
  {
    Measure x( "lmd.idx" );
    std::ifstream fIndex( "lmd.idx" );
    for( iostr::EachLine in( fIndex ); in; in.next() )
    {
      Measure x( *in );
      std::ifstream fIn( (*in).c_str() );
      for( iostr::EachLine l( fIn ); l; l.next() )
      {
	;
      }
    }
  }
  INT << "===[END]============================================" << endl << endl;
  zypp::base::LogControl::instance().logNothing();
  return 0;
}


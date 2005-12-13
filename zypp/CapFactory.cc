/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file	zypp/CapFactory.cc
 *
*/
#include <iostream>
#include <functional>
#include <set>
#include <map>

#include "zypp/base/Logger.h"
#include "zypp/base/Exception.h"
#include "zypp/base/String.h"

#include "zypp/CapFactory.h"
#include "zypp/capability/Capabilities.h"

using std::endl;

///////////////////////////////////////////////////////////////////
namespace
{ /////////////////////////////////////////////////////////////////
  using ::zypp::Resolvable;
  using ::zypp::capability::CapabilityImpl;
  using ::zypp::capability::CapImplOrder;

  /** Set of unique CapabilityImpl. */
  typedef std::set<CapabilityImpl::Ptr,CapImplOrder> USet;

  /** Set to unify created capabilities.
   *
   * This is to unify capabilities. Each CapabilityImpl created
   * by CapFactory, must be inserted into _uset, and the returned
   * CapabilityImpl::Ptr has to be uset to create the Capability.
  */
  USet _uset;

  /** Each CapabilityImpl created in CapFactory \b must be warpped.
   *
   * Immediately wrap \a allocated_r, and unified by inserting it into
   * \c _uset. Each CapabilityImpl created by CapFactory, \b must be
   * inserted into _uset, by calling usetInsert.
   *
   * \return CapabilityImpl_Ptr referencing \a allocated_r (or an
   * eqal representation, allocated is deleted then).
  */
  CapabilityImpl::Ptr usetInsert( CapabilityImpl * allocated_r )
  {
    return *(_uset.insert( CapabilityImpl::Ptr(allocated_r) ).first);
  }

  /** Collect USet statistics.
   * \ingroup DEBUG
  */
  struct USetStatsCollect : public std::unary_function<CapabilityImpl::constPtr, void>
  {
    struct Counter
    {
      unsigned _count;
      Counter() : _count( 0 ) {}
      void incr() { ++_count; }
    };

    Counter _caps;
    std::map<CapabilityImpl::Kind,Counter> _capKind;
    std::map<Resolvable::Kind,Counter>     _capRefers;

    void operator()( const CapabilityImpl::constPtr & cap_r )
    {
      //DBG << *cap_r << endl;
      _caps.incr();
      _capKind[cap_r->kind()].incr();
      _capRefers[cap_r->refers()].incr();
    }

    std::ostream & dumpOn( std::ostream & str ) const
    {
      str << "  Capabilities total: " << _caps._count << endl;
      str << "  Capability kinds:" << endl;
      for ( std::map<CapabilityImpl::Kind,Counter>::const_iterator it = _capKind.begin();
            it != _capKind.end(); ++it )
        {
          str << "    " << it->first << '\t' << it->second._count << endl;
        }
      str << "  Capability refers:" << endl;
      for ( std::map<Resolvable::Kind,Counter>::const_iterator it = _capRefers.begin();
            it != _capRefers.end(); ++it )
        {
          str << "    " << it->first << '\t' << it->second._count << endl;
        }
      return str;
    }
  };

  /////////////////////////////////////////////////////////////////
} // namespace
///////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////
namespace zypp
{ /////////////////////////////////////////////////////////////////

  ///////////////////////////////////////////////////////////////////
  //
  //	CLASS NAME : CapFactoryImpl
  //
  /** CapFactory implementation.
   *
   * Provides various functions doing checks and log and \c throw.
   * CapFactory::parse usually combines them, and if nothing fails,
   * finaly builds the Capability.
   *
   * \attention Each CapabilityImpl created by CapFactory, \b must
   * be inserted into ::_uset, by calling ::usetInsert, \b before
   * the Capability is created.
   *
   * \li \c file:    /absolute/path
   * \li \c split:   name:/absolute/path
   * \li \c name:    name
   * \li \c vers:    name op edition
  */
  struct CapFactory::Impl
  {
    /** Assert a valid Resolvable::Kind. */
    static void assertResKind( const Resolvable::Kind & refers_r )
    {
      if ( refers_r == Resolvable::Kind() )
        ZYPP_THROW( Exception("Missing or empty  Resolvable::Kind in Capability") );
    }

    /** Check whether \a op_r and \a edition_r make a valid edition spec.
     *
     * Rel::NONE is not usefull thus forbidden. Rel::ANY can be ignored,
     * so no VersionedCap is needed for this. Everything else requires
     * a VersionedCap.
     *
     * \return Whether to build a VersionedCap (i.e. \a op_r
     * is not Rel::ANY.
    */
    static bool isEditionSpec( Rel op_r, const Edition & edition_r )
    {
      switch ( op_r.inSwitch() )
        {
        case Rel::ANY_e:
          if ( edition_r != Edition::noedition )
            WAR << "Operator " << op_r << " causes Edition "
            << edition_r << " to be ignored." << endl;
          return false;
          break;

        case Rel::NONE_e:
          ZYPP_THROW( Exception("Operator NONE is not allowed in Capability") );
          break;

        case Rel::EQ_e:
        case Rel::NE_e:
        case Rel::LT_e:
        case Rel::LE_e:
        case Rel::GT_e:
        case Rel::GE_e:
          return true;
          break;
        }
      // SHOULD NOT GET HERE
      ZYPP_THROW( Exception("Unknow Operator NONE is not allowed in Capability") );
      return false; // not reached
    }

    /** Test for a FileCap. \a name_r starts with \c "/". */
    static bool isFileSpec( const std::string & name_r )
    {
      return *name_r.c_str() == '/';
    }

    /** Test for a SplitCap. \a name_r constains \c ":/". */
    static bool isSplitSpec( const std::string & name_r )
    {
      return name_r.find( ":/" ) != std::string::npos;
    }

    /** Try to build a non versioned cap from \a name_r .
     *
     * The CapabilityImpl is built here and inserted into _uset.
     * The final Capability must be created by CapFactory, as it
     * is a friend of Capability. Here we can't access the ctor.
    */
    static CapabilityImpl::Ptr buildNamed( const Resolvable::Kind & refers_r,
                                           const std::string & name_r )
    {
      // NullCap check first:
      if ( name_r.empty() )
        {
          // Singleton, so no need to put it into _uset !?
          return capability::NullCap::instance();
        }

      assertResKind( refers_r );

      // file:    /absolute/path
      if ( isFileSpec( name_r ) )
        return usetInsert
        ( new capability::FileCap( refers_r, name_r ) );

      //split:   name:/absolute/path
      str::regex  rx( "([^/]*):(/.*)" );
      str::smatch what;
      if( str::regex_match( name_r.begin(), name_r.end(), what, rx ) )
        {
          return usetInsert
          ( new capability::SplitCap( refers_r, what[1].str(), what[2].str() ) );
        }

      //name:    name
      return usetInsert
      ( new capability::NamedCap( refers_r, name_r ) );
    }

    /** Try to build a versioned cap from \a name_r .
     *
     * The CapabilityImpl is built here and inserted into _uset.
     * The final Capability must be created by CapFactory, as it
     * is a friend of Capability. Here we can't access the ctor.
     *
     * \todo Quick check for name not being filename or split.
    */
    static CapabilityImpl::Ptr buildVersioned( const Resolvable::Kind & refers_r,
                                               const std::string & name_r,
                                               Rel op_r,
                                               const Edition & edition_r )
    {
      if ( Impl::isEditionSpec( op_r, edition_r ) )
        {
          assertResKind( refers_r );

          // build a VersionedCap
          return usetInsert
          ( new capability::VersionedCap( refers_r, name_r, op_r, edition_r ) );
        }
      //else
      // build a NamedCap

      return buildNamed( refers_r, name_r );
    }
  };
  ///////////////////////////////////////////////////////////////////

  ///////////////////////////////////////////////////////////////////

  ///////////////////////////////////////////////////////////////////
  //
  //	CLASS NAME : CapFactory
  //
  ///////////////////////////////////////////////////////////////////

  ///////////////////////////////////////////////////////////////////
  //
  //	METHOD NAME : CapFactory::CapFactory
  //	METHOD TYPE : Ctor
  //
  CapFactory::CapFactory()
  {}

  ///////////////////////////////////////////////////////////////////
  //
  //	METHOD NAME : CapFactory::~CapFactory
  //	METHOD TYPE : Dtor
  //
  CapFactory::~CapFactory()
  {}

#if 0
  ///////////////////////////////////////////////////////////////////
  //
  //	METHOD NAME : CapFactory::parse
  //	METHOD TYPE : Capability
  //
  Capability CapFactory::parse( const std::string & strval_r ) const
  {
    return parse( strval_r, Resolvable::Kind() );
  }
#endif

  ///////////////////////////////////////////////////////////////////
  //
  //	METHOD NAME : CapFactory::parse
  //	METHOD TYPE : Capability
  //
  Capability CapFactory::parse( const Resolvable::Kind & refers_r,
                                const std::string & strval_r ) const

  try
    {
      // strval_r has at least two words which could make 'op edition'?
      // improve regex!
      str::regex  rx( "(.*[^ \t])([ \t]+)([^ \t]+)([ \t]+)([^ \t]+)" );
      str::smatch what;
      if( str::regex_match( strval_r.begin(), strval_r.end(),what, rx ) )
        {
          Rel op;
          Edition edition;
          try
            {
              op = Rel(what[3].str());
              edition = Edition(what[5].str());
            }
          catch ( Exception & excpt )
            {
              // So they don't make valid 'op edition'
              ZYPP_CAUGHT( excpt );
              DBG << "Trying named cap for: " << strval_r << endl;
              // See whether it makes a named cap.
              return Capability( Impl::buildNamed( refers_r, strval_r ) );
            }

          // Valid 'op edition'
          return Capability ( Impl::buildVersioned( refers_r,
                                                    what[1].str(), op, edition ) );
        }
      //else
      // not a VersionedCap

      return Capability( Impl::buildNamed( refers_r, strval_r ) );
    }
  catch ( Exception & excpt )
    {
      ZYPP_RETHROW( excpt );
      return Capability(); // not reached
    }


  ///////////////////////////////////////////////////////////////////
  //
  //	METHOD NAME : CapFactory::parse
  //	METHOD TYPE : Capability
  //
  Capability CapFactory::parse( const Resolvable::Kind & refers_r,
                                const std::string & name_r,
                                const std::string & op_r,
                                const std::string & edition_r ) const
  try
    {
      // Try creating Rel and Edition, then parse
      return parse( refers_r, name_r, Rel(op_r), Edition(edition_r) );
    }
  catch ( Exception & excpt )
    {
      ZYPP_RETHROW( excpt );
      return Capability(); // not reached
    }

  ///////////////////////////////////////////////////////////////////
  //
  //	METHOD NAME : CapFactory::parse
  //	METHOD TYPE : Capability
  //
  Capability CapFactory::parse( const Resolvable::Kind & refers_r,
                                const std::string & name_r,
                                Rel op_r,
                                const Edition & edition_r ) const
  try
    {
      return Capability
      ( Impl::buildVersioned( refers_r, name_r, op_r, edition_r ) );
    }
  catch ( Exception & excpt )
    {
      ZYPP_RETHROW( excpt );
      return Capability(); // not reached
    }

  /******************************************************************
  **
  **	FUNCTION NAME : operator<<
  **	FUNCTION TYPE : std::ostream &
  */
  std::ostream & operator<<( std::ostream & str, const CapFactory & obj )
  {
    str << "CapFactory stats:" << endl;

    return for_each( _uset.begin(), _uset.end(), USetStatsCollect() ).dumpOn( str );
  }

  /////////////////////////////////////////////////////////////////
} // namespace zypp
///////////////////////////////////////////////////////////////////

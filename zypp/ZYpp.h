/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file	zypp/ZYpp.h
 *
*/
#ifndef ZYPP_ZYPP_H
#define ZYPP_ZYPP_H

#include <iosfwd>

#include "zypp/base/ReferenceCounted.h"
#include "zypp/base/NonCopyable.h"
#include "zypp/base/PtrTypes.h"
#include "zypp/Target.h"
#include "zypp/Resolver.h"

///////////////////////////////////////////////////////////////////
namespace zypp
{ /////////////////////////////////////////////////////////////////

  namespace zypp_detail
  {
    class ZYppImpl;
  }

  class ZYppFactory;
  class ResPool;
  class ResPoolProxy;
  class SourceFeed_Ref;
  class ResStore;
  class Locale;

  ///////////////////////////////////////////////////////////////////
  //
  //	CLASS NAME : ZYpp
  //
  /**
   * \todo define Exceptions
  */
  class ZYpp : public base::ReferenceCounted, private base::NonCopyable
  {
  public:

    typedef intrusive_ptr<ZYpp>       Ptr;
    typedef intrusive_ptr<const ZYpp> constPtr;

  public:

    /** Pool of ResStatus for individual ResObjetcs. */
    ResPool pool() const;

    /** Pool of ui::Selectable.
     * Based on the ResPool, ui::Selectable groups ResObjetcs of
     * same kind and name.
    */
    ResPoolProxy poolProxy() const;

    /**  */
    //SourceFeed_Ref sourceFeed() const;

    void addResolvables (const ResStore& store, bool installed = false);

    void removeResolvables (const ResStore& store);

  public:
    /**
     * \throws Exception
     */
    Target_Ptr target() const;

    /**
     * \throws Exception
     * if commit_only == true, just init, don't populate store or pool
     */
    void initTarget(const Pathname & root, bool commit_only = false);

    /**
     * \throws Exception
     */
    void finishTarget();


  public:
    /** Result returned from ZYpp::commit. */
    struct CommitResult;

    /** Commit changes and transactions.
     * \param medianr 0 = all/any media
     *                 > 0 means only the given media number
     * \return \ref CommitResult
     * \throws Exception
    */
    CommitResult commit( int medianr_r );

  public:
    /** */
    Resolver_Ptr resolver() const;

  public:
    /** Set the preferd locale for translated labels, descriptions,
     *  etc. passed to the UI.
     */
    void setTextLocale( const Locale & textLocale_r );
    /** */
    Locale getTextLocale() const;

  public:
    typedef std::set<Locale> LocaleSet;
    /** Set the requested locales.
     * Languages to be supported by the system, e.g. language specific
     * packages to be installed.
    */
    void setRequestedLocales( const LocaleSet & locales_r );
    /** */
    LocaleSet getRequestedLocales() const;

  public:
    /** Get the system architecture.   */
    Arch architecture() const;
    /** Set the system architecture.
	This should be used for testing/debugging only since the Target backend
	won't be able to install incompatible packages ;-)   */
    void setArchitecture( const Arch & arch );

  protected:
    /** Dtor */
    virtual ~ZYpp();
    /** Stream output */
    virtual std::ostream & dumpOn( std::ostream & str ) const;
  private:
    /** Factory */
    friend class ZYppFactory;
    /** */
    typedef zypp_detail::ZYppImpl Impl;
    typedef shared_ptr<Impl>      Impl_Ptr;
    /** Factory ctor */
    explicit
    ZYpp( const Impl_Ptr & impl_r );
  private:
    /** Pointer to implementation */
    RW_pointer<Impl> _pimpl;
  };
  ///////////////////////////////////////////////////////////////////

  /////////////////////////////////////////////////////////////////
} // namespace zypp
///////////////////////////////////////////////////////////////////
#endif // ZYPP_ZYPP_H

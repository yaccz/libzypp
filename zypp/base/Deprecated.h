/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file	zypp/base/Deprecated.h
 *  \brief	Provides the ZYPP_DEPRECATED macro.
 */

/**
 * The ZYPP_DEPRECATED macro can be used to trigger compile-time warnings
 * with gcc >= 3.2 when deprecated functions are used.
 *
 * For non-inline functions, the macro is used at the very end of the
 * function declaration, right before the semicolon, unless it's pure
 * virtual:
 *
 * int deprecatedFunc() const ZYPP_DEPRECATED;
 * virtual int deprecatedPureVirtualFunc() const ZYPP_DEPRECATED = 0;
 *
 * Functions which are implemented inline are handled differently:
 * the ZYPP_DEPRECATED macro is used at the front, right before the
 * return type, but after "static" or "virtual":
 *
 * ZYPP_DEPRECATED void deprecatedFuncA() { .. }
 * virtual ZYPP_DEPRECATED int deprecatedFuncB() { .. }
 * static  ZYPP_DEPRECATED bool deprecatedFuncC() { .. }
 *
 * You can also mark whole structs or classes as deprecated, by inserting
 * the ZYPP_DEPRECATED macro after the struct/class keyword, but before
 * the name of the struct/class:
 *
 * class ZYPP_DEPRECATED DeprecatedClass { };
 * struct ZYPP_DEPRECATED DeprecatedStruct { };
 *
 * However, deprecating a struct/class doesn't create a warning for gcc
 * versions <= 3.3 (haven't tried 3.4 yet).  If you want to deprecate a class,
 * also deprecate all member functions as well (which will cause warnings).
 *
 */

#if __GNUC__ - 0 > 3 || (__GNUC__ - 0 == 3 && __GNUC_MINOR__ - 0 >= 2)
#  define ZYPP_DECL_DEPRECATED __attribute__ ((__deprecated__))
#else
#  define ZYPP_DECL_DEPRECATED
#endif
#ifndef ZYPP_DECL_VARIABLE_DEPRECATED
#  define ZYPP_DECL_VARIABLE_DEPRECATED ZYPP_DECL_DEPRECATED
#endif
#ifndef ZYPP_DECL_CONSTRUCTOR_DEPRECATED
#  if defined(ZYPP_MOC_RUN)
#    define ZYPP_DECL_CONSTRUCTOR_DEPRECATED ZYPP_DECL_CONSTRUCTOR_DEPRECATED
#  elif defined(ZYPP_NO_DEPRECATED_CONSTRUCTORS)
#    define ZYPP_DECL_CONSTRUCTOR_DEPRECATED
#  else
#    define ZYPP_DECL_CONSTRUCTOR_DEPRECATED ZYPP_DECL_DEPRECATED
#  endif
#endif

#if defined(ZYPP_NO_DEPRECATED)
/* disable Zypp9 support as well */
#  undef ZYPP9_SUPPORT_WARNINGS
#  undef ZYPP9_SUPPORT
#  undef ZYPP_DEPRECATED
#  undef ZYPP_DEPRECATED_VARIABLE
#  undef ZYPP_DEPRECATED_CONSTRUCTOR
#elif defined(ZYPP_DEPRECATED_WARNINGS)
#  ifdef ZYPP9_SUPPORT
/* enable Zypp9 support warnings as well */
#    undef ZYPP9_SUPPORT_WARNINGS
#    define ZYPP9_SUPPORT_WARNINGS
#  endif
#  undef ZYPP_DEPRECATED
#  define ZYPP_DEPRECATED ZYPP_DECL_DEPRECATED
#  undef ZYPP_DEPRECATED_VARIABLE
#  define ZYPP_DEPRECATED_VARIABLE ZYPP_DECL_VARIABLE_DEPRECATED
#  undef ZYPP_DEPRECATED_CONSTRUCTOR
#  define ZYPP_DEPRECATED_CONSTRUCTOR explicit ZYPP_DECL_CONSTRUCTOR_DEPRECATED
#else
#  undef ZYPP_DEPRECATED
#  define ZYPP_DEPRECATED
#  undef ZYPP_DEPRECATED_VARIABLE
#  define ZYPP_DEPRECATED_VARIABLE
#  undef ZYPP_DEPRECATED_CONSTRUCTOR
#  define ZYPP_DEPRECATED_CONSTRUCTOR
#endif

#if defined(ZYPP9_SUPPORT_WARNINGS)
#  if !defined(ZYPP_COMPAT_WARNINGS) /* also enable compat */
#    define ZYPP_COMPAT_WARNINGS
#  endif
#  undef ZYPP9_SUPPORT
#  define ZYPP9_SUPPORT ZYPP_DECL_DEPRECATED
#  undef ZYPP9_SUPPORT_VARIABLE
#  define ZYPP9_SUPPORT_VARIABLE ZYPP_DECL_VARIABLE_DEPRECATED
#  undef ZYPP9_SUPPORT_CONSTRUCTOR
#  define ZYPP9_SUPPORT_CONSTRUCTOR explicit ZYPP_DECL_CONSTRUCTOR_DEPRECATED
#elif defined(ZYPP9_SUPPORT) /* define back to nothing */
#  if !defined(ZYPP_COMPAT) /* also enable zypp9 support */
#    define ZYPP_COMPAT
#  endif
#  undef ZYPP9_SUPPORT
#  define ZYPP9_SUPPORT
#  undef ZYPP9_SUPPORT_VARIABLE
#  define ZYPP9_SUPPORT_VARIABLE
#  undef ZYPP9_SUPPORT_CONSTRUCTOR
#  define ZYPP9_SUPPORT_CONSTRUCTOR explicit
#endif






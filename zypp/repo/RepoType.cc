/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/

#include <iostream>
#include <map>
#include "zypp/base/String.h"
#include "zypp/repo/RepoException.h"
#include "RepoType.h"

namespace zypp
{
namespace repo
{

  const RepoType RepoType::RPMMD(RepoType::RPMMD_e);
  const RepoType RepoType::YAST2(RepoType::YAST2_e);
  const RepoType RepoType::RPMPLAINDIR(RepoType::RPMPLAINDIR_e);
  const RepoType RepoType::RUBYGEM(RepoType::RUBYGEM_e);
  const RepoType RepoType::NONE(RepoType::NONE_e);

  RepoType::RepoType(const std::string & strval_r)
    : _type(parse(strval_r))
  {}

  RepoType::Type RepoType::parse(const std::string & strval_r)
  {
    static std::map<std::string,RepoType::Type> _table;

    if (_table.empty())
    {
      // initialize it
      _table["repomd"]
      = _table["rpmmd"]
      = _table["rpm-md"]
      = _table["yum"]
      = _table["up2date"]
      = RepoType::RPMMD_e;

      _table["susetags"]
      = _table["yast"]
      = _table["yast2"]
      = RepoType::YAST2_e;

      _table["plaindir"]
      = RepoType::RPMPLAINDIR_e;

      _table["rubygem"]
      = RepoType::RUBYGEM_e;

      _table["none"]
      = RepoType::NONE_e;
    }

    std::map<std::string,RepoType::Type>::const_iterator it
      = _table.find(str::toLower(strval_r));
    if (it == _table.end())
    {
      ZYPP_THROW(RepoUnknownTypeException(
        "Unknown repository type '" + strval_r + "'"));
    }
    return it->second;
  }


  const std::string & RepoType::asString() const
  {
    static std::map<Type, std::string> _table;
    if ( _table.empty() )
    {
      // initialize it
      _table[RPMMD_e]		= "rpm-md";
      _table[YAST2_e]		= "yast2";
      _table[RPMPLAINDIR_e]	= "plaindir";
      _table[RUBYGEM_e]		= "rubygem";
      _table[NONE_e]		= "NONE";
    }
    return _table[_type];
  }


  } // ns repo
} // ns zypp

// vim: set ts=2 sts=2 sw=2 et ai:

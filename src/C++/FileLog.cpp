/****************************************************************************
** Copyright (c) 2001-2014
**
** This file is part of the QuickFIX FIX Engine
**
** This file may be distributed under the terms of the quickfixengine.org
** license as defined by quickfixengine.org and appearing in the file
** LICENSE included in the packaging of this file.
**
** This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
** WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
**
** See http://www.quickfixengine.org/LICENSE for licensing information.
**
** Contact ask@quickfixengine.org if any conditions of this licensing are
** not clear to you.
**
****************************************************************************/

#ifdef _MSC_VER
#include "stdafx.h"
#else
#include "config.h"
#endif

#include "FileLog.h"

namespace FIX
{
Log* FileLogFactory::create()
{
  if ( ++m_globalLogCount > 1 ) return m_globalLog;

  if ( m_path.size() ) return new FileLog(m_path, b_globalIncludeTimestamp);

  try
  {

    const Dictionary& settings = m_settings.get();
    std::string path = settings.getString(FILE_LOG_PATH);
    std::string backupPath = path;

    if ( settings.has( FILE_LOG_BACKUP_PATH ) )
      backupPath = settings.getString( FILE_LOG_BACKUP_PATH );

    if( settings.has( FILE_LOG_INCLUDE_TIMESTAMP ) ){
      std::string includeTimestampText = settings.getString( FILE_LOG_INCLUDE_TIMESTAMP );
      b_globalIncludeTimestamp = includeTimestampText != "N";
    }

    return m_globalLog = new FileLog( path, backupPath, b_globalIncludeTimestamp );
  }
  catch( ConfigError& )
  {
    m_globalLogCount--;
    throw;
  }
}

Log* FileLogFactory::create( const SessionID& s )
{
  if ( m_path.size() && m_backupPath.size() )
    return new FileLog( m_path, m_backupPath, s );
  if ( m_path.size() )
    return new FileLog( m_path, s );

  bool includeTimestamp = true;
  std::string path;
  std::string backupPath;
  Dictionary settings = m_settings.get( s );
  path = settings.getString( FILE_LOG_PATH );
  backupPath = path;
  if( settings.has( FILE_LOG_BACKUP_PATH ) )
    backupPath = settings.getString( FILE_LOG_BACKUP_PATH );

  if( settings.has( FILE_LOG_INCLUDE_TIMESTAMP ) ){
    std::string includeTimestampText = settings.getString( FILE_LOG_INCLUDE_TIMESTAMP );
    includeTimestamp = includeTimestampText != "N";
  }

  return new FileLog( path, backupPath, s, includeTimestamp );
}

void FileLogFactory::destroy( Log* pLog )
{
  if ( pLog != m_globalLog || --m_globalLogCount == 0 )
  {
    delete pLog;
  }
}

FileLog::FileLog( const std::string& path, const bool includeTimestamp )
{
  init( path, path, "GLOBAL", includeTimestamp );
}

FileLog::FileLog( const std::string& path, const std::string& backupPath, const bool includeTimestamp )
{
  init( path, backupPath, "GLOBAL", includeTimestamp );
}

FileLog::FileLog( const std::string& path, const SessionID& s, const bool includeTimestamp )
{
  init( path, path, generatePrefix(s), includeTimestamp );
}

FileLog::FileLog( const std::string& path, const std::string& backupPath, const SessionID& s, const bool includeTimestamp )
{
  init( path, backupPath, generatePrefix(s), includeTimestamp );
}

std::string FileLog::generatePrefix( const SessionID& s )
{
  const std::string& begin =
    s.getBeginString().getString();
  const std::string& sender =
    s.getSenderCompID().getString();
  const std::string& target =
    s.getTargetCompID().getString();
  const std::string& qualifier =
    s.getSessionQualifier();

  std::string prefix = begin + "-" + sender + "-" + target;
  if( qualifier.size() )
    prefix += "-" + qualifier;

  return prefix;
}

void FileLog::init( std::string path, std::string backupPath, const std::string& prefix, const bool includeTimestamp )
{
  file_mkdir( path.c_str() );
  file_mkdir( backupPath.c_str() );

  if ( path.empty() ) path = ".";
  if ( backupPath.empty() ) backupPath = path;

  m_fullPrefix
    = file_appendpath(path, prefix + ".");
  m_fullBackupPrefix
    = file_appendpath(backupPath, prefix + ".");

  b_includeTimestamp = includeTimestamp;
  m_messagesFileName = m_fullPrefix + "messages.current.log";
  m_eventFileName = m_fullPrefix + "event.current.log";

  m_messages.open( m_messagesFileName.c_str(), std::ios::out | std::ios::app );
  if ( !m_messages.is_open() ) throw ConfigError( "Could not open messages file: " + m_messagesFileName );
  m_event.open( m_eventFileName.c_str(), std::ios::out | std::ios::app );
  if ( !m_event.is_open() ) throw ConfigError( "Could not open event file: " + m_eventFileName );
}

FileLog::~FileLog()
{
  m_messages.close();
  m_event.close();
}

void FileLog::clear()
{
  m_messages.close();
  m_event.close();

  m_messages.open( m_messagesFileName.c_str(), std::ios::out | std::ios::trunc );
  m_event.open( m_eventFileName.c_str(), std::ios::out | std::ios::trunc );
}

void FileLog::backup()
{
  m_messages.close();
  m_event.close();

  int i = 0;
  while( true )
  {
    std::stringstream messagesFileName;
    std::stringstream eventFileName;

    messagesFileName << m_fullBackupPrefix << "messages.backup." << ++i << ".log";
    eventFileName << m_fullBackupPrefix << "event.backup." << i << ".log";
    FILE* messagesLogFile = file_fopen( messagesFileName.str().c_str(), "r" );
    FILE* eventLogFile = file_fopen( eventFileName.str().c_str(), "r" );

    if( messagesLogFile == NULL && eventLogFile == NULL )
    {
      file_rename( m_messagesFileName.c_str(), messagesFileName.str().c_str() );
      file_rename( m_eventFileName.c_str(), eventFileName.str().c_str() );
      m_messages.open( m_messagesFileName.c_str(), std::ios::out | std::ios::trunc );
      m_event.open( m_eventFileName.c_str(), std::ios::out | std::ios::trunc );
      return;
    }

    if( messagesLogFile != NULL ) file_fclose( messagesLogFile );
    if( eventLogFile != NULL ) file_fclose( eventLogFile );
  }
}

} //namespace FIX

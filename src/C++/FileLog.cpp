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
#include <thread>
#include <chrono>
#include <stdio.h>

namespace FIX
{
Log* FileLogFactory::create()
{
  if ( ++m_globalLogCount > 1 ) return m_globalLog;

  if ( m_path.size() ) return new FileLog(m_path, b_globalIncludeTimestamp, c_globalRollFileType);

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

    if( settings.has( FILE_LOG_ROLL_TYPE ) ){
      std::string rollFileTypeText = settings.getString( FILE_LOG_ROLL_TYPE );
      if(rollFileTypeText.empty()){
        c_globalRollFileType = 'N';
      } else {
        c_globalRollFileType = rollFileTypeText[0];
      }
    } else {
      c_globalRollFileType = 'N';
    }

    return m_globalLog = new FileLog( path, backupPath, b_globalIncludeTimestamp, c_globalRollFileType );
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
  char rollFileType;
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

  if( settings.has( FILE_LOG_ROLL_TYPE ) ){
    std::string rollFileTypeText = settings.getString( FILE_LOG_ROLL_TYPE );
    if(rollFileTypeText.empty()){
      rollFileType = 'N';
    } else {
      rollFileType = rollFileTypeText[0];
    }
  } else {
    rollFileType = 'N';
  }

  return new FileLog( path, backupPath, s, includeTimestamp, rollFileType );
}

void FileLogFactory::destroy( Log* pLog )
{
  if ( pLog != m_globalLog || --m_globalLogCount == 0 )
  {
    delete pLog;
  }
}

FileLog::FileLog( const std::string& path, const bool includeTimestamp, const char rollFileType )
{
  init( path, path, "GLOBAL", includeTimestamp, rollFileType );
}

FileLog::FileLog( const std::string& path, const std::string& backupPath, const bool includeTimestamp, const char rollFileType )
{
  init( path, backupPath, "GLOBAL", includeTimestamp, rollFileType );
}

FileLog::FileLog( const std::string& path, const SessionID& s, const bool includeTimestamp, const char rollFileType )
{
  init( path, path, generatePrefix(s), includeTimestamp, rollFileType );
}

FileLog::FileLog( const std::string& path, const std::string& backupPath, const SessionID& s, const bool includeTimestamp, const char rollFileType )
{
  init( path, backupPath, generatePrefix(s), includeTimestamp, rollFileType );
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

std::string FileLog::generateSuffix( const UtcTimeStamp& timestamp )
{
  if(c_rollFileType == 'N'){
    return "";
  } else if ( c_rollFileType == 'D'){
    UtcTimeStamp t (timestamp.getYear(), timestamp.getMonth(), timestamp.getDay());
    std::string timestampText = UtcTimeStampConvertor::convert( t, 1 );
    return timestampText;
  } else if ( c_rollFileType == 'H'){
    UtcTimeStamp t (timestamp.getYear(), timestamp.getMonth(), timestamp.getDay(),
      timestamp.getHour(), 0, 0);
    std::string timestampText = UtcTimeStampConvertor::convert( t, 1 );
    return timestampText;
  } else if ( c_rollFileType == 'm'){
    UtcTimeStamp t (timestamp.getYear(), timestamp.getMonth(), timestamp.getDay(),
      timestamp.getHour(), timestamp.getMinute(), 0);
    std::string timestampText = UtcTimeStampConvertor::convert( t, 1 );
    return timestampText;
  }

  return "";
}

std::ofstream& FileLog::getMessagesStream( const UtcTimeStamp& timestamp )
{
  std::string suffix = generateSuffix(timestamp);
  if(suffix == m_fileNameSuffix)
    return *m_messages;

  reloadFileStream(suffix);
  return *m_messages;
}

std::ofstream& FileLog::getEventsStream( const UtcTimeStamp& timestamp )
{
  std::string suffix = generateSuffix(timestamp);
  if(suffix == m_fileNameSuffix)
    return *m_event;

  reloadFileStream(suffix);
  return *m_event;
}

void FileLog::reloadFileStream( const std::string& suffix )
{
  const std::lock_guard<std::mutex> lock(m_fileRollingMutex);
  if(suffix == m_fileNameSuffix)
    return;

  std::ofstream* prevMessages = m_messages;
  std::ofstream* prevEvent = m_event;
  std::string prevMessagesFileName = m_messagesFileName;
  std::string prevEventFileName = m_eventFileName;
  std::string prevFullFileNameSuffix = m_fullFileNameSuffix;

  std::ofstream* messages = new std::ofstream();
  std::ofstream* event = new std::ofstream();
  std::string fullFileNameSuffix = "." + suffix + ".log";
  std::replace( fullFileNameSuffix.begin(), fullFileNameSuffix.end(), ':', '-');
  std::replace( fullFileNameSuffix.begin(), fullFileNameSuffix.end(), ' ', '-');
  std::string messagesFileName = m_fullPrefix + "messages.current" + fullFileNameSuffix;
  std::string eventFileName = m_fullPrefix + "event.current" + fullFileNameSuffix;

  messages->open( messagesFileName.c_str(), std::ios::out | std::ios::app );
  if ( !messages->is_open() ) throw ConfigError( "Could not open messages file: " + messagesFileName );
  event->open( eventFileName.c_str(), std::ios::out | std::ios::app );
  if ( !event->is_open() ) throw ConfigError( "Could not open event file: " + eventFileName );

  m_fileNameSuffix = suffix;
  m_fullFileNameSuffix = fullFileNameSuffix;
  m_messagesFileName = messagesFileName;
  m_eventFileName = eventFileName;
  m_messages = messages;
  m_event = event;

  std::thread cleanUpThread (&FileLog::cleanFileStream, this, prevMessages, prevEvent,
    prevMessagesFileName, prevEventFileName, prevFullFileNameSuffix );
  cleanUpThread.detach();
}

void FileLog::cleanFileStream( std::ofstream* messages, std::ofstream* event,
    const std::string& messagesFileName, const std::string& eventFileName, const std::string& fullFileNameSuffix )
{
  std::this_thread::sleep_for(std::chrono::seconds(2));

  backup( *messages, *event, messagesFileName, eventFileName, fullFileNameSuffix );
  delete messages;
  delete event;
}

void FileLog::init( std::string path, std::string backupPath, const std::string& prefix, const bool includeTimestamp, const char rollFileType )
{
  m_messages = new std::ofstream();
  m_event = new std::ofstream();

  file_mkdir( path.c_str() );
  file_mkdir( backupPath.c_str() );

  if ( path.empty() ) path = ".";
  if ( backupPath.empty() ) backupPath = path;

  m_fullPrefix
    = file_appendpath(path, prefix + ".");
  m_fullBackupPrefix
    = file_appendpath(backupPath, prefix + ".");

  b_includeTimestamp = includeTimestamp;
  c_rollFileType = rollFileType;

  UtcTimeStamp now;
  m_fileNameSuffix = generateSuffix(now);
  if (m_fileNameSuffix.empty()) {
      m_fullFileNameSuffix = ".log";
  } else {
      m_fullFileNameSuffix = "." + m_fileNameSuffix + ".log";
  }

  std::replace( m_fullFileNameSuffix.begin(), m_fullFileNameSuffix.end(), ':', '-');
  std::replace( m_fullFileNameSuffix.begin(), m_fullFileNameSuffix.end(), ' ', '-');
  m_messagesFileName = m_fullPrefix + "messages.current" + m_fullFileNameSuffix;
  m_eventFileName = m_fullPrefix + "event.current" + m_fullFileNameSuffix;

  m_messages->open( m_messagesFileName.c_str(), std::ios::out | std::ios::app );
  if ( !m_messages->is_open() ) throw ConfigError( "Could not open messages file: " + m_messagesFileName );
  m_event->open( m_eventFileName.c_str(), std::ios::out | std::ios::app );
  if ( !m_event->is_open() ) throw ConfigError( "Could not open event file: " + m_eventFileName );
}

FileLog::~FileLog()
{
  m_messages->close();
  m_event->close();
  delete m_messages;
  delete m_event;
}

void FileLog::clear()
{
  m_messages->close();
  m_event->close();

  m_messages->open( m_messagesFileName.c_str(), std::ios::out | std::ios::trunc );
  m_event->open( m_eventFileName.c_str(), std::ios::out | std::ios::trunc );
}

void FileLog::backup()
{
  backup(*m_messages, *m_event, m_messagesFileName, m_eventFileName, m_fullFileNameSuffix );
}

void FileLog::backup( std::ofstream& messages, std::ofstream& event,
    const std::string& messagesFileName, const std::string& eventFileName, const std::string& fullFileNameSuffix )
{
  if(messages.is_open())
    messages.close();

  if(event.is_open())
    event.close();

  int i = 0;
  while( true )
  {
    std::stringstream messagesFileNameStream;
    std::stringstream eventFileNameStream;

    std::string suffix;
    if ( c_rollFileType == 'D'
      || c_rollFileType == 'H'
      || c_rollFileType == 'm'){
      suffix = fullFileNameSuffix;
    } else{
      suffix = "." + std::to_string(++i) + ".log";
    }

    messagesFileNameStream << m_fullBackupPrefix << "messages.backup"<< suffix;
    eventFileNameStream << m_fullBackupPrefix << "event.backup"<< suffix;

    FILE* messagesLogFile = file_fopen( messagesFileNameStream.str().c_str(), "r" );
    FILE* eventLogFile = file_fopen( eventFileNameStream.str().c_str(), "r" );

    if( messagesLogFile == NULL && eventLogFile == NULL )
    {
      file_rename( messagesFileName.c_str(), messagesFileNameStream.str().c_str() );
      file_rename( eventFileName.c_str(), eventFileNameStream.str().c_str() );

      if ( c_rollFileType == 'D'
        || c_rollFileType == 'H'
        || c_rollFileType == 'm'){
        if ( messagesFileName.c_str() != messagesFileNameStream.str().c_str() )
            std::remove(messagesFileName.c_str());

        if ( eventFileName.c_str() != eventFileNameStream.str().c_str() )
            std::remove(eventFileName.c_str());
      } else{
        messages.open( messagesFileName.c_str(), std::ios::out | std::ios::trunc );
        event.open( eventFileName.c_str(), std::ios::out | std::ios::trunc );
      }

      return;
    }

    if( messagesLogFile != NULL ) file_fclose( messagesLogFile );
    if( eventLogFile != NULL ) file_fclose( eventLogFile );
  }
}

} //namespace FIX

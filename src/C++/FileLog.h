/* -*- C++ -*- */

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

#ifndef FIX_FILELOG_H
#define FIX_FILELOG_H

#ifdef _MSC_VER
#pragma warning( disable : 4503 4355 4786 4290 )
#endif

#include "Log.h"
#include "SessionSettings.h"
#include <fstream>
#include <mutex>

namespace FIX
{
/**
 * Creates a file based implementation of Log
 *
 * This stores all log events into flat files
 */
class FileLogFactory : public LogFactory
{
public:
  FileLogFactory( const SessionSettings& settings )
: m_settings( settings ), m_globalLog(0), m_globalLogCount(0) {};
  FileLogFactory( const std::string& path )
: m_path( path ), m_backupPath( path ), m_globalLog(0), m_globalLogCount(0) {};
  FileLogFactory( const std::string& path, const std::string& backupPath )
: m_path( path ), m_backupPath( backupPath ), m_globalLog(0), m_globalLogCount(0) {};

public:
  Log* create();
  Log* create( const SessionID& );
  void destroy( Log* log );

private:
  bool b_globalIncludeTimestamp = true;
  bool c_globalRollFileType = 'N';
  std::string m_path;
  std::string m_backupPath;
  SessionSettings m_settings;
  Log* m_globalLog;
  int m_globalLogCount;
};

/**
 * File based implementation of Log
 *
 * Two files are created by this implementation.  One for messages,
 * and one for events.
 *
 */
class FileLog : public Log
{
public:
  FileLog( const std::string& path, const bool includeTimestamp = true, const char rollFileType = 'N' );
  FileLog( const std::string& path, const std::string& backupPath, const bool includeTimestamp = true, const char rollFileType = 'N' );
  FileLog( const std::string& path, const SessionID& sessionID, const bool includeTimestamp = true, const char rollFileType = 'N' );
  FileLog( const std::string& path, const std::string& backupPath, const SessionID& sessionID, const bool includeTimestamp = true, const char rollFileType = 'N' );
  virtual ~FileLog();

  void clear();
  void backup();

  void onIncoming( const std::string& value )
  {
    UtcTimeStamp now;
    std::ofstream& messages = getMessagesStream(now);
    if( b_includeTimestamp )
        messages << UtcTimeStampConvertor::convert(now, 9) << " : " << value << std::endl;
    else
        messages << value << std::endl;
  }
  void onOutgoing( const std::string& value )
  {
    UtcTimeStamp now;
    std::ofstream& messages = getMessagesStream(now);
    if( b_includeTimestamp )
        messages << UtcTimeStampConvertor::convert(now, 9) << " : " << value << std::endl;
    else
        messages << value << std::endl;
  }
  void onEvent( const std::string& value )
  {
    UtcTimeStamp now;
    std::ofstream& events = getEventsStream(now);
    if( b_includeTimestamp ){
        events << UtcTimeStampConvertor::convert( now, 9 )
                << " : " << value << std::endl;
    }
    else{
        events << value << std::endl;
    }
  }

private:
  std::string generatePrefix( const SessionID& sessionID );

  std::string generateSuffix( const UtcTimeStamp& timestamp );
  std::ofstream& getMessagesStream( const UtcTimeStamp& timestamp );
  std::ofstream& getEventsStream( const UtcTimeStamp& timestamp );
  void reloadFileStream( const std::string& suffix );
  void cleanFileStream( std::ofstream* messages, std::ofstream* event,
    const std::string& messagesFileName, const std::string& eventFileName, const std::string& fullFileNameSuffix  );

  void init( std::string path, std::string backupPath, const std::string& prefix,
    const bool includeTimestamp, const char rollFileType );
  void backup( std::ofstream& messages, std::ofstream& event,
    const std::string& messagesFileName, const std::string& eventFileName, const std::string& fullFileNameSuffix );

  bool b_includeTimestamp;
  std::ofstream* m_messages;
  std::ofstream* m_event;
  std::string m_messagesFileName;
  std::string m_eventFileName;
  std::string m_fullPrefix;
  std::string m_fullBackupPrefix;

  char c_rollFileType;
  std::mutex m_fileRollingMutex;
  std::string m_fileNameSuffix;
  std::string m_fullFileNameSuffix;
};
}

#endif //FIX_LOG_H

#pragma once

#include "Utilities.h"

// Standard Library
#include <stdexcept>
#include <string>

// Qt
#include <qwidget.h>
#include <qsqlerror.h>
#include <qstring.h>
#include <qmessagebox.h>

namespace ex
{
  namespace app
  {
    class DbSyncFailed : public std::runtime_error
    {
    public:
      DbSyncFailed() : std::runtime_error("") {}
    };

    class NoSteamApps : public std::runtime_error
    {
    public:
      NoSteamApps() : std::runtime_error("") {}
    };
  }

  namespace db
  {
    class QueryFailed : public std::runtime_error
    {
    public:
      QueryFailed(const QSqlError& err) : std::runtime_error(formatMessage(err)) {}

      void Show(QWidget* parent = nullptr) const
      {
        utils::ShowMessageBox(parent, QMessageBox::Critical, msg);
      }

    private:
      std::string formatMessage(const QSqlError& err)
      {
        QString msg = QString("Database query execution failed.\nDriver: %1\nDatabase: %2")
          .arg(err.driverText())
          .arg(err.databaseText());
        return msg.toStdString();
      }

    private:
      QString msg;
    };
  }
}
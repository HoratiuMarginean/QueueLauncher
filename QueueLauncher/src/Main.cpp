#include "QueueLauncher.h"
#include "Constants.h"
#include "ErrorMessages.h"
#include "Exceptions.h"
#include "Utilities.h"

// Qt
#include <qsqldatabase.h>
#include <qapplication.h>
#include <qstring.h>

int main(int argc, char* argv[])
{
  QApplication app(argc, argv);

  QSqlDatabase db = QSqlDatabase::addDatabase(Consts::dbType);
  db.setDatabaseName(Consts::dbFileName);
  if (!db.open())
  {
    Err::Db::openFailed.Show();
    return 1;
  }

  try
  {
    bool openUi = false;
    if (Utils::CreateDbTables())
    {
      openUi = true;
    }

    // Check arguments, skipping first one (application name)
    for (int i = 1; i < argc; i++)
    {
      QString arg = argv[i];
      if (!openUi && (arg == "-c" || arg == "--config"))
      {
        openUi = true;
      }
    }

    if (!openUi && !Utils::LaunchApp())
    {
      openUi = true;
    }

    if (openUi)
    {
      QueueLauncher window;
      window.show();

      return app.exec();
    }
  }
  catch (Ex::Db::QueryFailed& e)
  {
    e.Show();
    return 1;
  }
  catch (Ex::App::DbSyncFailed&)
  {
    Err::App::dbSyncFailed.Show();
    return 1;
  }

  return 0;
}
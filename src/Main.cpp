#include "Constants.h"
#include "ErrorMessages.h"
#include "Exceptions.h"
#include "QueueLauncher.h"
#include "Utilities.h"

// Qt
#include <qapplication.h>
#include <qdir.h>
#include <qsqldatabase.h>
#include <qstring.h>

int main(int argc, char* argv[])
{
  QApplication app(argc, argv);

  const QString localAppDataPath = QString(std::getenv("LOCALAPPDATA")).replace("\\", "/");
  const QString projectDirPath = localAppDataPath + "/" + PROJECT_NAME;
  QDir().mkdir(projectDirPath);

  QSqlDatabase db = QSqlDatabase::addDatabase(consts::dbType);
  db.setDatabaseName(projectDirPath + "/" + consts::dbFileName);
  if (!db.open())
  {
    err::db::openFailed.Show();
    return 1;
  }

  try
  {
    utils::CreateDbTables();

    // Check arguments, skipping first one (application name)
    for (int i = 1; i < argc; i++)
    {
      QString arg = argv[i];
      if (arg == "-ql" || arg == "--quick-launch")
      {
        utils::LaunchApp();
        return 0;
      }
    }

    QueueLauncher window;
    window.show();

    return QApplication::exec();
  }
  catch (ex::db::QueryFailed& e)
  {
    e.Show();
    return 1;
  }
  catch (ex::app::DbSyncFailed&)
  {
    err::app::dbSyncFailed.Show();
    return 1;
  }
}

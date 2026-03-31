#include "Utilities.h"
#include "ErrorMessages.h"
#include "Exceptions.h"
#include "Queries.h"

// Standard Library
#include <vector>
#include <string>
#include <tuple>
#include <fstream>
#include <filesystem>
namespace fs = std::filesystem;

// Qt
#include <qwidget.h>
#include <qstring.h>
#include <qcontainerfwd.h>
#include <qprocess.h>
#include <qsqldatabase.h>
#include <qsqlquery.h>
#include <qfileinfo.h>
#include <qmessagebox.h>
#include <qtypes.h>

// VDF Parser
#include <vdf_parser.hpp>
namespace vdf = tyti::vdf;

namespace utils
{
  bool CreateDbTables()
  {
    bool anyTableCreated = false;
    QStringList dbTables = QSqlDatabase::database().tables();

    QSqlQuery query;
    if (!dbTables.contains("app"))
    {
      if (!query.exec(queries::createTableApp))
        throw ex::db::QueryFailed(query.lastError());
      anyTableCreated = true;
    }
    if (!dbTables.contains("local_app"))
    {
      if (!query.exec(queries::createTableLocalApp))
        throw ex::db::QueryFailed(query.lastError());
      anyTableCreated = true;
    }
    if (!dbTables.contains("store"))
    {
      if (!query.exec(queries::createTableStore))
        throw ex::db::QueryFailed(query.lastError());
      anyTableCreated = true;
    }
    if (!dbTables.contains("store_app"))
    {
      if (!query.exec(queries::createTableStoreApp))
        throw ex::db::QueryFailed(query.lastError());
      anyTableCreated = true;
    }
    /*if (!dbTables.contains("option"))
    {
      if (!query.exec(Queries::createTableOption)) throw ex::db::QueryFailed(query.lastError());
      anyTableCreated = true;
    }*/

    return anyTableCreated;
  }

  bool LaunchApp(const uint queueIndex)
  {
    QSqlQuery selectAppIdQuery(
      "SELECT id "
      "FROM app "
      "WHERE queue_index = ?"
    );
    selectAppIdQuery.addBindValue(queueIndex);
    if (!selectAppIdQuery.exec())
    {
      throw ex::db::QueryFailed(selectAppIdQuery.lastError());
    }
    if (!selectAppIdQuery.next())
    {
      info::app::queueEmpty.Show();
      return false;
    }
    int appId = selectAppIdQuery.value(0).toInt();

    QSqlQuery selectStoreAppQuery(
      "SELECT store_id, external_id "
      "FROM store_app "
      "WHERE app_id = ?"
    );
    selectStoreAppQuery.addBindValue(appId);
    if (!selectStoreAppQuery.exec())
    {
      throw ex::db::QueryFailed(selectStoreAppQuery.lastError());
    }

    QSqlQuery selectLocalAppQuery(
      "SELECT path "
      "FROM local_app "
      "WHERE app_id = ?"
    );
    selectLocalAppQuery.addBindValue(appId);
    if (!selectLocalAppQuery.exec())
    {
      throw ex::db::QueryFailed(selectLocalAppQuery.lastError());
    }

    QString runAppPath;
    QStringList runAppArguments;
    if (selectStoreAppQuery.next())
    {
      int storeId    = selectStoreAppQuery.value(0).toInt();
      int externalId = selectStoreAppQuery.value(1).toInt();

      QSqlQuery selectStoreQuery(
        "SELECT name, path "
        "FROM store "
        "WHERE id = ?"
      );
      selectStoreQuery.addBindValue(storeId);
      if (!selectStoreQuery.exec())
      {
        throw ex::db::QueryFailed(selectStoreQuery.lastError());
      }
      if (!selectStoreQuery.next())
      {
        return false;
      }
      QString storeName = selectStoreQuery.value(0).toString();
      QString storePath = selectStoreQuery.value(1).toString();

      if (storeName == "Steam")
      {
        if (storePath.isEmpty())
        {
          return false;
        }
        runAppPath      = storePath + "/steam.exe";
        runAppArguments = {"-nochatui", "-nofriendsui", "-silent", "steam://rungameid/" + QString::number(externalId)};
      }
      // Other stores
      else
      {
        return false;
      }
    }
    else if (selectLocalAppQuery.next())
    {
      runAppPath = selectLocalAppQuery.value(0).toString();
    }
    else
    {
      return false;
    }

    if (!QProcess::startDetached(runAppPath, runAppArguments))
    {
      err::app::launchFailed.Show();
      return false;
    }

    return true;
  }

  bool ValidSteamPath(const QString& steamPath)
  {
    if (steamPath.isEmpty())
    {
      return false;
    }

    if (!QFileInfo(steamPath).exists())
    {
      QMessageBox::warning(nullptr, "Warning", "Steam path \"" + steamPath + "\" does not exist.");
      return false;
    }

    QString steamAppsPath = steamPath + "/steamapps";
    if (!QFileInfo(steamAppsPath).exists())
    {
      QMessageBox::warning(nullptr, "Warning", "Directory \"" + steamAppsPath + "\" not found.");
      return false;
    }

    QString libraryFilePath = steamAppsPath + "/libraryfolders.vdf";
    if (!QFileInfo(libraryFilePath).exists())
    {
      QMessageBox::warning(nullptr, "Warning", "File \"" + libraryFilePath + "\" not found.");
      return false;
    }

    return true;
  }

  // TODO: Add custom exceptions
  std::vector<std::tuple<QString, int>> GetInstalledSteamAppsNameExternalId(const QString& libraryFoldersFilePath)
  {
    std::vector<fs::path> installPaths;

    std::ifstream libraryFoldersFile(libraryFoldersFilePath.toStdString());
    auto libraryFoldersFileRoot = vdf::read(libraryFoldersFile);
    for (auto& child : libraryFoldersFileRoot.childs)
    {
      installPaths.push_back(child.second.get()->attribs["path"] + "\\steamapps");
    }
    if (installPaths.empty())
    {
      throw ex::app::NoSteamApps();
    }

    std::vector<std::tuple<QString, int>> appsNameExternalId;
    for (const fs::path& installPath : installPaths)
    {
      for (const fs::directory_entry entry : fs::directory_iterator(installPath))
      {
        fs::path path = entry.path();
        if (path.extension() != ".acf")
        {
          continue;
        }

        std::ifstream manifestFile(path.string());
        auto manifestFileRoot = vdf::read(manifestFile);

        QString appName   = QString::fromStdString(manifestFileRoot.attribs["name"]);
        int appExternalId = std::stoi(manifestFileRoot.attribs["appid"]);

        appsNameExternalId.push_back({appName, appExternalId});
      }
    }
    if (appsNameExternalId.empty())
    {
      throw ex::app::NoSteamApps();
    }

    return appsNameExternalId;
  }

  void ShowMessageBox(QWidget* parent, const QMessageBox::Icon& icon, const QString& msg)
  {
    QMessageBox msgBox(parent);
    msgBox.setIcon(icon);
    switch (icon)
    {
      case QMessageBox::Information: msgBox.setWindowTitle("Info");
        break;
      case QMessageBox::Warning: msgBox.setWindowTitle("Warning");
        break;
      case QMessageBox::Critical: msgBox.setWindowTitle("Error");
        break;
      default: msgBox.setWindowTitle("");
    }
    msgBox.setText(msg);

    msgBox.exec();
  }
}

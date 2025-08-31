#include "Utilities.h"
#include "ErrorMessages.h"
#include "Exceptions.h"
#include "Queries.h"

// Standard Library
#include <stdexcept>
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

// VDF Parser
#include <vdf_parser.hpp>
namespace vdf = tyti::vdf;

namespace Utils
{
  bool CreateDbTables()
  {
    bool anyTableCreated = false;
    QStringList dbTables = QSqlDatabase::database().tables();

    QSqlQuery query;
    if (!dbTables.contains("app"))
    {
      if (!query.exec(Queries::createTableApp))
      {
        throw Ex::Db::QueryFailed(query.lastError());
      }
      anyTableCreated = true;
    }
    if (!dbTables.contains("local_app"))
    {
      if (!query.exec(Queries::createTableLocalApp))
      {
        throw Ex::Db::QueryFailed(query.lastError());
      }
      anyTableCreated = true;
    }
    if (!dbTables.contains("store"))
    {
      if (!query.exec(Queries::createTableStore))
      {
        throw Ex::Db::QueryFailed(query.lastError());
      }
      anyTableCreated = true;
    }
    if (!dbTables.contains("store_app"))
    {
      if (!query.exec(Queries::createTableStoreApp))
      {
        throw Ex::Db::QueryFailed(query.lastError());
      }
      anyTableCreated = true;
    }

    return anyTableCreated;
  }

  bool LaunchApp()
  {
    QSqlQuery selectAppIdQuery(
      "SELECT id "
      "FROM app "
      "WHERE queue_index = 0"
    );
    if (!selectAppIdQuery.exec())
    {
      throw Ex::Db::QueryFailed(selectAppIdQuery.lastError());
    }
    if (!selectAppIdQuery.next())
    {
      Info::App::queueEmpty.Show();
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
      throw Ex::Db::QueryFailed(selectStoreAppQuery.lastError());
    }

    QSqlQuery selectLocalAppQuery(
      "SELECT path "
      "FROM local_app "
      "WHERE app_id = ?"
    );
    selectLocalAppQuery.addBindValue(appId);
    if (!selectLocalAppQuery.exec())
    {
      throw Ex::Db::QueryFailed(selectLocalAppQuery.lastError());
    }

    QString runAppPath;
    QStringList runAppArguments;
    if (selectStoreAppQuery.next())
    {
      int storeId = selectStoreAppQuery.value(0).toInt();
      int externalId = selectStoreAppQuery.value(1).toInt();

      QSqlQuery selectStoreQuery(
        "SELECT name, path "
        "FROM store "
        "WHERE id = ?"
      );
      selectStoreQuery.addBindValue(storeId);
      if (!selectStoreQuery.exec())
      {
        throw Ex::Db::QueryFailed(selectStoreQuery.lastError());
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
        runAppPath = storePath + "/steam.exe";
        runAppArguments = { "-nochatui", "-nofriendsui", "-silent", "steam://rungameid/" + QString::number(externalId) };
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
      Err::App::launchFailed.Show();
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
      throw Ex::App::NoSteamApps();
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

        QString appName = QString::fromStdString(manifestFileRoot.attribs["name"]);
        int appExternalId = std::stoi(manifestFileRoot.attribs["appid"]);

        appsNameExternalId.push_back({ appName, appExternalId });
      }
    }
    if (appsNameExternalId.empty())
    {
      throw Ex::App::NoSteamApps();
    }

    return appsNameExternalId;
  }

  inline void ShowMessageBox(QWidget* parent, const QMessageBox::Icon& icon, const QString& msg)
  {
    QMessageBox msgBox(parent);
    msgBox.setIcon(icon);
    switch (icon)
    {
      case QMessageBox::Information: msgBox.setWindowTitle("Info"); break;
      case QMessageBox::Warning:     msgBox.setWindowTitle("Warning"); break;
      case QMessageBox::Critical:    msgBox.setWindowTitle("Error"); break;
      default:                       msgBox.setWindowTitle("");
    }
    msgBox.setText(msg);

    msgBox.exec();
  }
}
#pragma once

// Standard Library
#include <vector>
#include <tuple>

// Qt
#include <qstring.h>
#include <qwidget.h>
#include <qmessagebox.h>
#include <qtypes.h>

namespace utils
{
  bool CreateDbTables();

  bool LaunchApp(uint queueIndex = 0);

  bool ValidSteamPath(const QString& steamPath);
  std::vector<std::tuple<QString, int>> GetInstalledSteamAppsNameExternalId(const QString& libraryFoldersFilePath);

  void ShowMessageBox(QWidget* parent, const QMessageBox::Icon& icon, const QString& msg);
}

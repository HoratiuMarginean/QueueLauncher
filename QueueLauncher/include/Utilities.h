#pragma once

// Standard Library
#include <vector>
#include <tuple>

// Qt
#include <qstring.h>
#include <qwidget.h>
#include <qmessagebox.h>

namespace Utils
{
  bool CreateDbTables();

  bool LaunchApp();

  bool ValidSteamPath(const QString& steamPath);
  std::vector<std::tuple<QString, int>> GetInstalledSteamAppsNameExternalId(const QString& libraryFoldersFilePath);

  inline void ShowMessageBox(QWidget* parent, const QMessageBox::Icon& icon, const QString& msg);
}
#pragma once

#include "Utilities.h"

// Qt
#include <qwidget.h>
#include <qstring.h>
#include <qmessagebox.h>

struct ErrorMessage
{
  QMessageBox::Icon icon;
  QString msg;

  void Show(QWidget* parent = nullptr) const
  {
    Utils::ShowMessageBox(parent, icon, msg);
  }
};

namespace Err
{
  namespace App
  {
    inline const ErrorMessage launchFailed{ QMessageBox::Critical, "App launch failed." };
    inline const ErrorMessage dbSyncFailed{ QMessageBox::Critical, "App order sync failed." };
    inline const ErrorMessage moveFailed{ QMessageBox::Critical, "App could not be moved." };
    inline const ErrorMessage addFailed{ QMessageBox::Critical, "App could not be added." };
    inline const ErrorMessage renameFailed{ QMessageBox::Critical, "App could not be renamed." };
    inline const ErrorMessage removeFailed{ QMessageBox::Critical, "App could not be removed." };
    inline const ErrorMessage saveSteamPathFailed{ QMessageBox::Critical, "Steam path could not be saved." };
  }

  namespace Db
  {
    inline const ErrorMessage openFailed{ QMessageBox::Critical, "Could not open database." };
  }
}

namespace Warn
{
  namespace App
  {
    inline const ErrorMessage alreadyAdded{ QMessageBox::Warning, "App already added." };
    inline const ErrorMessage nameDuplicated{ QMessageBox::Warning, "App with the same name already exists." };
  }
}

namespace Info
{
  namespace App
  {
    inline const ErrorMessage queueEmpty{ QMessageBox::Information, "No apps found in queue." };
    inline const ErrorMessage noSteamApps{ QMessageBox::Information, "No Steam apps installed." };
  }
}
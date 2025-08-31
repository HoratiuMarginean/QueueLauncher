#include "QueueLauncher.h"
#include "Constants.h"
#include "ErrorMessages.h"
#include "Exceptions.h"
#include "Queries.h"
#include "Utilities.h"

// Standard Library
#include <exception>
#include <vector>
#include <tuple>

// Qt
#include <qapplication.h>
#include <qwidget.h>
#include <qmainwindow.h>
#include <qfiledialog.h>
#include <qinputdialog.h>
#include <qstring.h>
#include <qfileinfo.h>
#include <qsqldatabase.h>
#include <qsqlquery.h>
#include <qnamespace.h>

// Public Methods
QueueLauncher::QueueLauncher(QWidget* parent) : QMainWindow(parent)
{
  ui.setupUi(this);

  ConnectSlots();

  InitUi();
  ScanForApps();
}
QueueLauncher::~QueueLauncher() {}

// Private Methods
void QueueLauncher::ConnectSlots()
{
  // Apps Tab
  ui.queueList->connect(ui.queueList, &QListWidget::itemSelectionChanged, this, &QueueLauncher::onQueueListItemSelectionChanged);
  ui.ignoredList->connect(ui.ignoredList, &QListWidget::itemSelectionChanged, this, &QueueLauncher::onIgnoredListItemSelectionChanged);
  ui.addButton->connect(ui.addButton, &QPushButton::clicked, this, &QueueLauncher::onAddButtonClicked);
  ui.removeButton->connect(ui.removeButton, &QPushButton::clicked, this, &QueueLauncher::onRemoveButtonClicked);
  ui.renameButton->connect(ui.renameButton, &QPushButton::clicked, this, &QueueLauncher::onRenameButtonClicked);
  ui.moveUpButton->connect(ui.moveUpButton, &QPushButton::clicked, this, &QueueLauncher::onMoveUpButtonClicked);
  ui.moveDownButton->connect(ui.moveDownButton, &QPushButton::clicked, this, &QueueLauncher::onMoveDownButtonClicked);
  ui.moveButton->connect(ui.moveButton, &QPushButton::clicked, this, &QueueLauncher::onMoveButtonClicked);
  ui.moveAllButton->connect(ui.moveAllButton, &QPushButton::clicked, this, &QueueLauncher::onMoveAllButtonClicked);
  ui.scanButton->connect(ui.scanButton, &QPushButton::clicked, this, &QueueLauncher::onScanButtonClicked);
  ui.launchButton->connect(ui.launchButton, &QPushButton::clicked, this, &QueueLauncher::onLaunchButtonClicked);

  // Paths Tab
  ui.pickSteamPathButton->connect(ui.pickSteamPathButton, &QPushButton::clicked, this, &QueueLauncher::onPickSteamPathButtonClicked);
}

void QueueLauncher::InitUi()
{
  // Fill Queue and Ignored lists
  QSqlQuery selectAppsQuery(
    "SELECT name, queue_index "
    "FROM app "
    "ORDER BY queue_index"
  );
  if (!selectAppsQuery.exec())
  {
    throw Ex::Db::QueryFailed(selectAppsQuery.lastError());
  }
  while (selectAppsQuery.next())
  {
    QString appName = selectAppsQuery.value(0).toString();
    // Queued Apps
    if (!selectAppsQuery.value(1).isNull())
    {
      ui.queueList->addItem(appName);
    }
    // Ignored Apps
    else
    {
      ui.ignoredList->addItem(appName);
    }
  }

  // Fill Steam installation path
  QSqlQuery selectStorePathSteamQuery(Queries::selectStorePathSteam);
  if (!selectStorePathSteamQuery.exec())
  {
    throw Ex::Db::QueryFailed(selectStorePathSteamQuery.lastError());
  }
  if (selectStorePathSteamQuery.next())
  {
    steamPath = selectStorePathSteamQuery.value(0).toString();
    ui.steamPathLineEdit->setText(steamPath);
  }
}

void QueueLauncher::SyncAppDbQueueOrder()
{
  int queueLength = ui.queueList->count();
  QSqlQuery updateAppIndexQuery(
    "UPDATE app "
    "SET queue_index = ? "
    "WHERE name = ?"
  );
  for (int i = 0; i < queueLength; i++)
  {
    QString appName = ui.queueList->item(i)->text();

    updateAppIndexQuery.addBindValue(i);
    updateAppIndexQuery.addBindValue(appName);
    if (!updateAppIndexQuery.exec())
    {
      throw Ex::Db::QueryFailed(updateAppIndexQuery.lastError());
    }
    if (updateAppIndexQuery.numRowsAffected() == 0)
    {
      throw Ex::App::DbSyncFailed();
    }
  }
}

// TODO: Separate this growing monstrosity into multiple methods
void QueueLauncher::ScanForApps()
{
  QSqlDatabase db = QSqlDatabase::database();

  if (Utils::ValidSteamPath(steamPath))
  {
    try
    {
      QString libraryFoldersFilePath = steamPath + "/steamapps/libraryfolders.vdf";
      auto appsNameExternalId = Utils::GetInstalledSteamAppsNameExternalId(libraryFoldersFilePath);

      QSqlQuery selectStoreIdSteamQuery(Queries::selectStoreIdSteam);
      if (!selectStoreIdSteamQuery.exec())
      {
        throw Ex::Db::QueryFailed(selectStoreIdSteamQuery.lastError());
      }
      if (!selectStoreIdSteamQuery.next())
      {
        return;
      }
      int steamStoreId = selectStoreIdSteamQuery.value(0).toInt();

      storeAppNames.clear();
      QSqlQuery selectExternalIdsSteamQuery(
        "SELECT app_id, external_id "
        "FROM store_app "
        "WHERE store_id = ?"
      );
      selectExternalIdsSteamQuery.addBindValue(steamStoreId);
      if (!selectExternalIdsSteamQuery.exec())
      {
        throw Ex::Db::QueryFailed(selectExternalIdsSteamQuery.lastError());
      }
      while (selectExternalIdsSteamQuery.next())
      {
        int dbAppId = selectExternalIdsSteamQuery.value(0).toInt();
        int dbExternalId = selectExternalIdsSteamQuery.value(1).toInt();

        QSqlQuery selectAppNameIndexQuery(
          "SELECT name, queue_index "
          "FROM app "
          "WHERE id = ?"
        );
        selectAppNameIndexQuery.addBindValue(dbAppId);
        if (!selectAppNameIndexQuery.exec())
        {
          throw Ex::Db::QueryFailed(selectAppNameIndexQuery.lastError());
        }
        if (!selectAppNameIndexQuery.next())
        {
          return;
        }
        QString appName = selectAppNameIndexQuery.value(0).toString();
        storeAppNames.insert(appName);

        int erasedCount = (int)std::erase_if(appsNameExternalId,
          [&dbExternalId](const auto& appNameExternalId)
          {
            return std::get<1>(appNameExternalId) == dbExternalId;
          }
        );
        if (erasedCount > 0)
        {
          continue;
        }

        db.transaction();

        QSqlQuery deleteStoreAppQuery(
          "DELETE FROM store_app "
          "WHERE external_id = ?"
        );
        deleteStoreAppQuery.addBindValue(dbExternalId);
        if (!deleteStoreAppQuery.exec())
        {
          db.rollback();
          throw Ex::Db::QueryFailed(deleteStoreAppQuery.lastError());
        }
        if (deleteStoreAppQuery.numRowsAffected() == 0)
        {
          db.rollback();
          return;
        }

        QSqlQuery deleteAppQuery(
          "DELETE FROM app "
          "WHERE id = ?"
        );
        deleteAppQuery.addBindValue(dbAppId);
        if (!deleteAppQuery.exec())
        {
          db.rollback();
          throw Ex::Db::QueryFailed(deleteAppQuery.lastError());
        }
        if (deleteAppQuery.numRowsAffected() == 0)
        {
          db.rollback();
          return;
        }

        if (!selectAppNameIndexQuery.value(1).isNull())
        {
          int appIndex = selectAppNameIndexQuery.value(1).toInt();
          QListWidgetItem* queuedApp = ui.queueList->takeItem(appIndex);
          try
          {
            SyncAppDbQueueOrder();
          }
          catch (Ex::App::DbSyncFailed& e)
          {
            db.rollback();

            ui.queueList->insertItem(appIndex, queuedApp);

            throw e;
          }
        }
        else
        {
          QListWidgetItem* ignoredApp = ui.ignoredList->findItems(appName, Qt::MatchFlag::MatchExactly).first();
          ui.ignoredList->removeItemWidget(ignoredApp);
        }

        db.commit();
      }

      for (auto& appNameExternalId : appsNameExternalId)
      {
        QString appName = std::get<0>(appNameExternalId);
        int appExternalId = std::get<1>(appNameExternalId);

        int appNewIndex = ui.queueList->count();

        db.transaction();

        QSqlQuery insertAppQuery(
          "INSERT INTO app (name, queue_index) "
          "VALUES (?, ?)"
        );
        insertAppQuery.addBindValue(appName);
        insertAppQuery.addBindValue(appNewIndex);
        if (!insertAppQuery.exec())
        {
          db.rollback();
          throw Ex::Db::QueryFailed(insertAppQuery.lastError());
        }
        if (insertAppQuery.numRowsAffected() == 0)
        {
          db.rollback();
          return;
        }

        QSqlQuery insertStoreAppQuery(
          "INSERT INTO store_app (app_id, store_id, external_id) "
          "VALUES (last_insert_rowid(), ?, ?)"
        );
        insertStoreAppQuery.addBindValue(steamStoreId);
        insertStoreAppQuery.addBindValue(appExternalId);
        if (!insertStoreAppQuery.exec())
        {
          db.rollback();
          throw Ex::Db::QueryFailed(insertStoreAppQuery.lastError());
        }
        if (insertStoreAppQuery.numRowsAffected() == 0)
        {
          db.rollback();
          return;
        }

        ui.queueList->addItem(appName);

        db.commit();
      }
    }
    catch (Ex::App::NoSteamApps&)
    {
      Info::App::noSteamApps.Show();
    }
  }
}

void QueueLauncher::MoveApp(MoveDirection direction, QListWidgetItem* app)
{
  QSqlDatabase db = QSqlDatabase::database();

  switch (direction)
  {
    case toIgnored:
    {
      QString appName = app->text();
      int appIndex = ui.queueList->indexFromItem(app).row();

      QSqlQuery selectAppIdQuery(
        "SELECT id "
        "FROM app "
        "WHERE name = ? AND queue_index = ?"
      );
      selectAppIdQuery.addBindValue(appName);
      selectAppIdQuery.addBindValue(appIndex);
      if (!selectAppIdQuery.exec())
      {
        throw Ex::Db::QueryFailed(selectAppIdQuery.lastError());
      }
      if (!selectAppIdQuery.next())
      {
        Err::App::moveFailed.Show();
        return;
      }
      int appId = selectAppIdQuery.value(0).toInt();

      db.transaction();

      QSqlQuery updateAppIndexQuery(
        "UPDATE app "
        "SET queue_index = NULL "
        "WHERE id = ?"
      );
      updateAppIndexQuery.addBindValue(appId);
      if (!updateAppIndexQuery.exec())
      {
        db.rollback();
        throw Ex::Db::QueryFailed(updateAppIndexQuery.lastError());
      }
      if (updateAppIndexQuery.numRowsAffected() == 0)
      {
        db.rollback();
        Err::App::moveFailed.Show();
        return;
      }

      app = ui.queueList->takeItem(appIndex);
      try
      {
        SyncAppDbQueueOrder();
      }
      catch (Ex::App::DbSyncFailed& e)
      {
        db.rollback();

        ui.queueList->insertItem(appIndex, app);
        ui.queueList->setCurrentItem(app);

        throw e;
      }
      ui.ignoredList->addItem(app);
      ui.ignoredList->setCurrentItem(app);

      db.commit();
    }
    break;

    case toQueue:
    {
      QString appName = app->text();

      QSqlQuery selectAppIdQuery(
        "SELECT id "
        "FROM app "
        "WHERE name = ? AND queue_index IS NULL"
      );
      selectAppIdQuery.addBindValue(appName);
      if (!selectAppIdQuery.exec())
      {
        throw Ex::Db::QueryFailed(selectAppIdQuery.lastError());
      }
      if (!selectAppIdQuery.next())
      {
        Err::App::moveFailed.Show();
        return;
      }
      int appId = selectAppIdQuery.value(0).toInt();

      int appNewIndex = ui.queueList->count();

      db.transaction();

      QSqlQuery updateAppIndexQuery(
        "UPDATE app "
        "SET queue_index = ? "
        "WHERE id = ?"
      );
      updateAppIndexQuery.addBindValue(appNewIndex);
      updateAppIndexQuery.addBindValue(appId);
      if (!updateAppIndexQuery.exec())
      {
        db.rollback();
        throw Ex::Db::QueryFailed(updateAppIndexQuery.lastError());
      }
      if (updateAppIndexQuery.numRowsAffected() == 0)
      {
        db.rollback();
        Err::App::moveFailed.Show();
        return;
      }

      int appIndex = ui.ignoredList->indexFromItem(app).row();
      app = ui.ignoredList->takeItem(appIndex);
      ui.queueList->addItem(app);
      ui.queueList->setCurrentItem(app);

      db.commit();
    }
    break;

    case up:
    case down:
    {
      int appIndex = ui.queueList->indexFromItem(app).row();
      QString appName = app->text();

      int destAppIndex = appIndex + (direction == up ? -1 : 1);
      QString destAppName = ui.queueList->item(destAppIndex)->text();

      db.transaction();

      QSqlQuery updateAppIndexQuery(
        "UPDATE app "
        "SET queue_index = ? "
        "WHERE name = ?"
      );
      updateAppIndexQuery.addBindValue(destAppIndex);
      updateAppIndexQuery.addBindValue(appName);
      if (!updateAppIndexQuery.exec())
      {
        db.rollback();
        throw Ex::Db::QueryFailed(updateAppIndexQuery.lastError());
      }
      if (updateAppIndexQuery.numRowsAffected() == 0)
      {
        db.rollback();
        Err::App::moveFailed.Show();
        return;
      }

      updateAppIndexQuery.addBindValue(appIndex);
      updateAppIndexQuery.addBindValue(destAppName);
      if (!updateAppIndexQuery.exec())
      {
        db.rollback();
        throw Ex::Db::QueryFailed(updateAppIndexQuery.lastError());
      }
      if (updateAppIndexQuery.numRowsAffected() == 0)
      {
        db.rollback();
        Err::App::moveFailed.Show();
        return;
      }

      app = ui.queueList->takeItem(appIndex);
      ui.queueList->insertItem(destAppIndex, app);
      ui.queueList->setCurrentItem(app);

      db.commit();
    }
    break;
  }
}

// Slots
void QueueLauncher::onQueueListItemSelectionChanged()
{
  auto queueListSelectedItems = ui.queueList->selectedItems();
  if (queueListSelectedItems.length() > 0)
  {
    ui.ignoredList->clearSelection();

    ui.moveButton->setText(">");
    ui.moveAllButton->setText(">>");

    if (queueListSelectedItems.length() == 1)
    {
      QString appName = ui.queueList->currentItem()->text();
      if (!storeAppNames.contains(appName))
      {
        ui.renameButton->setEnabled(true);
        ui.removeButton->setEnabled(true);
      }
      else
      {
        ui.renameButton->setDisabled(true);
        ui.removeButton->setDisabled(true);
      }

      int index = ui.queueList->currentRow();
      if (index <= 0)
      {
        ui.moveUpButton->setDisabled(true);
      }
      else // if (index > 0)
      {
        ui.moveUpButton->setEnabled(true);
      }

      int itemCount = ui.queueList->count();
      if (index >= itemCount - 1)
      {
        ui.moveDownButton->setDisabled(true);
      }
      else // if (index < itemCount - 1)
      {
        ui.moveDownButton->setEnabled(true);
      }

      ui.moveButton->setEnabled(true);
      ui.moveAllButton->setDisabled(true);
    }
    else // if (queueListSelectedItems.length() > 1)
    {
      ui.renameButton->setDisabled(true);
      ui.removeButton->setDisabled(true);
      ui.moveUpButton->setDisabled(true);
      ui.moveDownButton->setDisabled(true);
      ui.moveButton->setDisabled(true);
      ui.moveAllButton->setEnabled(true);
    }
  }
  else // if (queueListSelectedItems.length() == 0)
  {
    ui.renameButton->setDisabled(true);
    ui.removeButton->setDisabled(true);
    ui.moveUpButton->setDisabled(true);
    ui.moveDownButton->setDisabled(true);
    ui.moveButton->setDisabled(true);
    ui.moveAllButton->setDisabled(true);
  }
}

void QueueLauncher::onIgnoredListItemSelectionChanged()
{
  auto ignoredListSelectedItems = ui.ignoredList->selectedItems();
  if (ignoredListSelectedItems.length() > 0)
  {
    ui.queueList->clearSelection();

    ui.moveButton->setText("<");
    ui.moveAllButton->setText("<<");

    if (ignoredListSelectedItems.length() == 1)
    {
      QString appName = ui.ignoredList->currentItem()->text();
      if (!storeAppNames.contains(appName))
      {
        ui.renameButton->setEnabled(true);
        ui.removeButton->setEnabled(true);
      }
      else
      {
        ui.renameButton->setDisabled(true);
        ui.removeButton->setDisabled(true);
      }

      ui.moveButton->setEnabled(true);
      ui.moveAllButton->setDisabled(true);
    }
    else // if (ignoredListSelectedItems.length() > 1)
    {
      ui.renameButton->setDisabled(true);
      ui.removeButton->setDisabled(true);
      ui.moveButton->setDisabled(true);
      ui.moveAllButton->setEnabled(true);
    }
  }
  else // if (ignoredListSelectedItems.length() == 0)
  {
    ui.renameButton->setDisabled(true);
    ui.removeButton->setDisabled(true);
    ui.moveUpButton->setDisabled(true);
    ui.moveDownButton->setDisabled(true);
    ui.moveButton->setDisabled(true);
    ui.moveAllButton->setDisabled(true);
  }
}

void QueueLauncher::onAddButtonClicked()
{
  QSqlDatabase db = QSqlDatabase::database();

  QString appPath = QFileDialog::getOpenFileName(this, tr("Pick an application to add"), "", tr("Executable Files (*.exe *.bat *.cmd);; All Files (*.*)"));
  if (appPath.isEmpty())
  {
    return;
  }

  QString appName = QFileInfo(appPath).baseName();
  int appIndex = ui.queueList->count();

  QSqlQuery selectDuplicatePathCheckQuery(
    "SELECT app_id "
    "FROM local_app "
    "WHERE path = ?"
  );
  selectDuplicatePathCheckQuery.addBindValue(appPath);
  if (!selectDuplicatePathCheckQuery.exec())
  {
    throw Ex::Db::QueryFailed(selectDuplicatePathCheckQuery.lastError());
  }
  if (selectDuplicatePathCheckQuery.next())
  {
    Warn::App::alreadyAdded.Show();
    return;
  }

  db.transaction();

  QSqlQuery insertAppQuery(
    "INSERT INTO app (name, queue_index) "
    "VALUES (?, ?)"
  );
  insertAppQuery.addBindValue(appName);
  insertAppQuery.addBindValue(appIndex);
  if (!insertAppQuery.exec())
  {
    db.rollback();
    throw Ex::Db::QueryFailed(insertAppQuery.lastError());
  }
  if (insertAppQuery.numRowsAffected() == 0)
  {
    db.rollback();
    Err::App::addFailed.Show();
    return;
  }

  QSqlQuery insertLocalAppQuery(
    "INSERT INTO local_app (app_id, path) "
    "VALUES (last_insert_rowid(), ?)"
  );
  insertLocalAppQuery.addBindValue(appPath);
  if (!insertLocalAppQuery.exec())
  {
    db.rollback();
    throw Ex::Db::QueryFailed(insertLocalAppQuery.lastError());
  }
  if (insertLocalAppQuery.numRowsAffected() == 0)
  {
    db.rollback();
    Err::App::addFailed.Show();
    return;
  }

  ui.queueList->addItem(appName);
  ui.queueList->setCurrentRow(appIndex);

  db.commit();
}

void QueueLauncher::onRenameButtonClicked()
{
  QListWidgetItem* app = ui.queueList->currentItem();
  QString appName = app->text();

  bool ok = false;
  QString appNewName = QInputDialog::getText(this, "Rename", "Enter a new name:", QLineEdit::Normal, appName, &ok);
  if (!ok || appNewName.isEmpty())
  {
    return;
  }

  // Check for duplicates
  QSqlQuery selectDuplicateNameCheckQuery(
    "SELECT name "
    "FROM app "
    "WHERE name = ?"
  );
  selectDuplicateNameCheckQuery.addBindValue(appNewName);
  if (!selectDuplicateNameCheckQuery.exec())
  {
    throw Ex::Db::QueryFailed(selectDuplicateNameCheckQuery.lastError());
  }
  if (selectDuplicateNameCheckQuery.next())
  {
    Warn::App::nameDuplicated.Show();
    return;
  }

  QSqlQuery updateNameQuery(
    "UPDATE app "
    "SET name = ? "
    "WHERE name = ?"
  );
  updateNameQuery.addBindValue(appNewName);
  updateNameQuery.addBindValue(appName);
  if (!updateNameQuery.exec())
  {
    throw Ex::Db::QueryFailed(updateNameQuery.lastError());
  }
  if (updateNameQuery.numRowsAffected() == 0)
  {
    Err::App::renameFailed.Show();
    return;
  }

  app->setText(appNewName);
}

void QueueLauncher::onRemoveButtonClicked()
{
  QSqlDatabase db = QSqlDatabase::database();

  QListWidgetItem* app = nullptr;
  int appIndex = -1;
  if (ui.queueList->currentRow() > -1)
  {
    app = ui.queueList->currentItem();
    appIndex = ui.queueList->currentRow();
  }
  else if (ui.ignoredList->currentRow() > -1)
  {
    app = ui.ignoredList->currentItem();
    appIndex = ui.ignoredList->currentRow();
  }
  else
  {
    return;
  }
  QString appName = app->text();

  QSqlQuery selectAppIdQuery(
    "SELECT id "
    "FROM app "
    "WHERE name = ?"
  );
  selectAppIdQuery.addBindValue(appName);
  if (!selectAppIdQuery.exec())
  {
    throw Ex::Db::QueryFailed(selectAppIdQuery.lastError());
  }
  if (!selectAppIdQuery.next())
  {
    Err::App::removeFailed.Show();
    return;
  }
  int appId = selectAppIdQuery.value(0).toInt();

  db.transaction();

  QSqlQuery deleteLocalAppQuery(
    "DELETE FROM local_app "
    "WHERE app_id = ?"
  );
  deleteLocalAppQuery.addBindValue(appId);
  if (!deleteLocalAppQuery.exec())
  {
    db.rollback();
    throw Ex::Db::QueryFailed(deleteLocalAppQuery.lastError());
  }
  if (deleteLocalAppQuery.numRowsAffected() == 0)
  {
    db.rollback();
    Err::App::removeFailed.Show();
    return;
  }

  QSqlQuery deleteAppQuery(
    "DELETE FROM app "
    "WHERE id = ?"
  );
  deleteAppQuery.addBindValue(appId);
  if (!deleteAppQuery.exec())
  {
    throw Ex::Db::QueryFailed(deleteAppQuery.lastError());
  }
  if (deleteAppQuery.numRowsAffected() == 0)
  {
    db.rollback();
    Err::App::removeFailed.Show();
    return;
  }

  // Remove from Queue list
  if (ui.queueList->currentRow() > -1)
  {
    app = ui.queueList->takeItem(appIndex);
    try
    {
      SyncAppDbQueueOrder();
    }
    catch (std::exception& e)
    {
      db.rollback();

      ui.queueList->insertItem(appIndex, app);
      ui.queueList->setCurrentItem(app);

      throw e;
    }
  }
  // Remove from Ignored list
  else if (ui.ignoredList->currentRow() > -1)
  {
    delete ui.ignoredList->takeItem(appIndex);
  }
  else
  {
    db.rollback();
    return;
  }
  
  db.commit();
}

void QueueLauncher::onMoveUpButtonClicked()
{
  QListWidgetItem* app = ui.queueList->currentItem();
  MoveApp(up, app);
}

void QueueLauncher::onMoveDownButtonClicked()
{
  QListWidgetItem* app = ui.queueList->currentItem();
  MoveApp(down, app);
}

void QueueLauncher::onMoveButtonClicked()
{
  // Queue to Ignored
  if (ui.moveButton->text() == ">")
  {
    QListWidgetItem* app = ui.queueList->currentItem();
    MoveApp(toIgnored, app);
  }
  // Ignored to Queue
  else // if (ui.moveButton->text() == "<")
  {
    QListWidgetItem* app = ui.ignoredList->currentItem();
    MoveApp(toQueue, app);
  }
}

void QueueLauncher::onMoveAllButtonClicked()
{
  // Queue to Ignored
  if (ui.moveAllButton->text() == ">>")
  {
    QList<QListWidgetItem*> apps = ui.queueList->selectedItems();
    for (QListWidgetItem* app : apps)
    {
      MoveApp(toIgnored, app);
    }
  }
  // Ignored to Queue
  else // if (ui.moveAllButton->text() == "<<")
  {
    QList<QListWidgetItem*> apps = ui.ignoredList->selectedItems();
    for (QListWidgetItem* app : apps)
    {
      MoveApp(toQueue, app);
    }
  }
}

void QueueLauncher::onScanButtonClicked()
{
  ScanForApps();
}

void QueueLauncher::onLaunchButtonClicked()
{
  if (!Utils::LaunchApp())
  {
    return;
  }

  QApplication::quit();
}

void QueueLauncher::onPickSteamPathButtonClicked()
{
  QString workingDirectory = Consts::defaultSteamInstallPath;
  if (!steamPath.isEmpty())
  {
    workingDirectory = steamPath;
  }

  QString steamPathTemp = QFileDialog::getExistingDirectory(this, tr("Pick Steam Installation Directory"), workingDirectory);
  if (!Utils::ValidSteamPath(steamPathTemp))
  {
    return;
  }

  QSqlQuery upsertQuery(
    "INSERT OR REPLACE INTO store (name, path) "
    "VALUES ('Steam', ?)"
  );
  upsertQuery.addBindValue(steamPathTemp);
  if (!upsertQuery.exec())
  {
    ui.steamPathLineEdit->clear();
    throw Ex::Db::QueryFailed(upsertQuery.lastError());
  }
  if (upsertQuery.numRowsAffected() == 0)
  {
    ui.steamPathLineEdit->clear();
    Err::App::saveSteamPathFailed.Show();
    return;
  }

  steamPath = steamPathTemp;
  ui.steamPathLineEdit->setText(steamPathTemp);
}
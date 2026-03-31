#include "QueueLauncher.h"
#include "Constants.h"
#include "ErrorMessages.h"
#include "Exceptions.h"
#include "Queries.h"
#include "Utilities.h"

// Standard Library
#include <exception>
#include <tuple>
#include <vector>

// Qt
#include <qapplication.h>
#include <qfiledialog.h>
#include <qfileinfo.h>
#include <qinputdialog.h>
#include <qmainwindow.h>
#include <qnamespace.h>
#include <qsqldatabase.h>
#include <qsqlquery.h>
#include <qstring.h>
#include <qtypes.h>
#include <qwidget.h>

// Public Methods
QueueLauncher::QueueLauncher(QWidget* parent)
  : QMainWindow(parent),
    ui_(std::make_unique<Ui::QueueLauncher>())
{
  InitUi();

  ScanForApps();
}

QueueLauncher::~QueueLauncher() = default;

// Private Methods
void QueueLauncher::ConnectSlots()
{
  // Apps Tab
  connect(ui_->queueList, &QListWidget::itemSelectionChanged, this, &QueueLauncher::onQueueListItemSelectionChanged);
  connect(ui_->ignoreList, &QListWidget::itemSelectionChanged, this, &QueueLauncher::onIgnoreListItemSelectionChanged);
  connect(ui_->addButton, &QPushButton::clicked, this, &QueueLauncher::onAddButtonClicked);
  connect(ui_->removeButton, &QPushButton::clicked, this, &QueueLauncher::onRemoveButtonClicked);
  connect(ui_->renameButton, &QPushButton::clicked, this, &QueueLauncher::onRenameButtonClicked);
  connect(ui_->moveUpButton, &QPushButton::clicked, this, &QueueLauncher::onMoveUpButtonClicked);
  connect(ui_->moveDownButton, &QPushButton::clicked, this, &QueueLauncher::onMoveDownButtonClicked);
  connect(ui_->moveButton, &QPushButton::clicked, this, &QueueLauncher::onMoveButtonClicked);
  connect(ui_->moveAllButton, &QPushButton::clicked, this, &QueueLauncher::onMoveAllButtonClicked);
  connect(ui_->scanButton, &QPushButton::clicked, this, &QueueLauncher::onScanButtonClicked);
  connect(ui_->launchButton, &QPushButton::clicked, this, &QueueLauncher::onLaunchButtonClicked);

  // Options Tab
  // connect(
  //   ui_->keepOpenCheckBox,
  //   &QCheckBox::checkStateChanged,
  //   this,
  //   &QueueLauncher::onKeepOpenCheckBoxCheckStateChanged
  // );
  connect(ui_->pickSteamPathButton, &QPushButton::clicked, this, &QueueLauncher::onPickSteamPathButtonClicked);
}

void QueueLauncher::InitUi()
{
  ui_->setupUi(this);

  // Fill Queue and Ignored lists
  QSqlQuery selectAppsQuery(
    "SELECT name, queue_index "
    "FROM app "
    "ORDER BY queue_index"
  );
  if (!selectAppsQuery.exec())
  {
    throw ex::db::QueryFailed(selectAppsQuery.lastError());
  }
  while (selectAppsQuery.next())
  {
    QString appName = selectAppsQuery.value(0).toString();
    // Queued Apps
    if (!selectAppsQuery.value(1).isNull())
    {
      ui_->queueList->addItem(appName);
    }
    // Ignored Apps
    else
    {
      ui_->ignoreList->addItem(appName);
    }
  }

  // Fill Steam installation path
  QSqlQuery selectStorePathSteamQuery(queries::selectStorePathSteam);
  if (!selectStorePathSteamQuery.exec())
  {
    throw ex::db::QueryFailed(selectStorePathSteamQuery.lastError());
  }
  if (selectStorePathSteamQuery.next())
  {
    steamPath_ = selectStorePathSteamQuery.value(0).toString();
    ui_->steamPathLineEdit->setText(steamPath_);
  }

  ConnectSlots();
}

void QueueLauncher::SyncAppDbQueueOrder() const
{
  const int queueLength = ui_->queueList->count();
  QSqlQuery updateAppIndexQuery(
    "UPDATE app "
    "SET queue_index = ? "
    "WHERE name = ?"
  );
  for (int i = 0; i < queueLength; i++)
  {
    QString appName = ui_->queueList->item(i)->text();

    updateAppIndexQuery.addBindValue(i);
    updateAppIndexQuery.addBindValue(appName);
    if (!updateAppIndexQuery.exec())
    {
      throw ex::db::QueryFailed(updateAppIndexQuery.lastError());
    }
    if (updateAppIndexQuery.numRowsAffected() == 0)
    {
      throw ex::app::DbSyncFailed();
    }
  }
}

// TODO: Separate this growing monstrosity into multiple methods
void QueueLauncher::ScanForApps()
{
  QSqlDatabase db = QSqlDatabase::database();

  if (utils::ValidSteamPath(steamPath_))
  {
    try
    {
      const QString libraryFoldersFilePath = steamPath_ + "/steamapps/libraryfolders.vdf";
      auto appsNameExternalId = utils::GetInstalledSteamAppsNameExternalId(libraryFoldersFilePath);

      QSqlQuery selectStoreIdSteamQuery(queries::selectStoreIdSteam);
      if (!selectStoreIdSteamQuery.exec())
      {
        throw ex::db::QueryFailed(selectStoreIdSteamQuery.lastError());
      }
      if (!selectStoreIdSteamQuery.next())
      {
        return;
      }
      const int steamStoreId = selectStoreIdSteamQuery.value(0).toInt();

      storeAppNames_.clear();
      QSqlQuery selectExternalIdsSteamQuery(
        "SELECT app_id, external_id "
        "FROM store_app "
        "WHERE store_id = ?"
      );
      selectExternalIdsSteamQuery.addBindValue(steamStoreId);
      if (!selectExternalIdsSteamQuery.exec())
      {
        throw ex::db::QueryFailed(selectExternalIdsSteamQuery.lastError());
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
          throw ex::db::QueryFailed(selectAppNameIndexQuery.lastError());
        }
        if (!selectAppNameIndexQuery.next())
        {
          return;
        }
        QString appName = selectAppNameIndexQuery.value(0).toString();
        storeAppNames_.insert(appName);

        const int erasedCount = static_cast<int>(std::erase_if(
          appsNameExternalId,
          [&dbExternalId](const auto& appNameExternalId)
          {
            return std::get<1>(appNameExternalId) == dbExternalId;
          }
        ));
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
          throw ex::db::QueryFailed(deleteStoreAppQuery.lastError());
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
          throw ex::db::QueryFailed(deleteAppQuery.lastError());
        }
        if (deleteAppQuery.numRowsAffected() == 0)
        {
          db.rollback();
          return;
        }

        if (!selectAppNameIndexQuery.value(1).isNull())
        {
          const int appIndex = selectAppNameIndexQuery.value(1).toInt();
          QListWidgetItem* queuedApp = ui_->queueList->takeItem(appIndex);
          try
          {
            SyncAppDbQueueOrder();
          }
          catch (ex::app::DbSyncFailed& e)
          {
            db.rollback();

            ui_->queueList->insertItem(appIndex, queuedApp);

            throw;
          }
        }
        else
        {
          QListWidgetItem* ignoredApp = ui_->ignoreList->findItems(appName, Qt::MatchFlag::MatchExactly).first();
          ui_->ignoreList->removeItemWidget(ignoredApp);
        }

        db.commit();
      }

      for (auto& appNameExternalId : appsNameExternalId)
      {
        QString appName = std::get<0>(appNameExternalId);
        const int appExternalId = std::get<1>(appNameExternalId);

        const int appNewIndex = ui_->queueList->count();

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
          throw ex::db::QueryFailed(insertAppQuery.lastError());
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
          throw ex::db::QueryFailed(insertStoreAppQuery.lastError());
        }
        if (insertStoreAppQuery.numRowsAffected() == 0)
        {
          db.rollback();
          return;
        }

        ui_->queueList->addItem(appName);

        db.commit();
      }
    }
    catch (ex::app::NoSteamApps&)
    {
      info::app::noSteamApps.Show();
    }
  }
}

void QueueLauncher::MoveApp(MoveDirection direction, QListWidgetItem* app) const
{
  QSqlDatabase db = QSqlDatabase::database();

  switch (direction)
  {
    case toIgnored:
    {
      const QString appName = app->text();
      const int appIndex = ui_->queueList->indexFromItem(app).row();

      QSqlQuery selectAppIdQuery(
        "SELECT id "
        "FROM app "
        "WHERE name = ? AND queue_index = ?"
      );
      selectAppIdQuery.addBindValue(appName);
      selectAppIdQuery.addBindValue(appIndex);
      if (!selectAppIdQuery.exec())
      {
        throw ex::db::QueryFailed(selectAppIdQuery.lastError());
      }
      if (!selectAppIdQuery.next())
      {
        err::app::moveFailed.Show();
        return;
      }
      const int appId = selectAppIdQuery.value(0).toInt();

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
        throw ex::db::QueryFailed(updateAppIndexQuery.lastError());
      }
      if (updateAppIndexQuery.numRowsAffected() == 0)
      {
        db.rollback();
        err::app::moveFailed.Show();
        return;
      }

      app = ui_->queueList->takeItem(appIndex);
      try
      {
        SyncAppDbQueueOrder();
      }
      catch (ex::app::DbSyncFailed& e)
      {
        db.rollback();

        ui_->queueList->insertItem(appIndex, app);
        ui_->queueList->setCurrentItem(app);

        throw;
      }
      ui_->ignoreList->addItem(app);
      ui_->ignoreList->setCurrentItem(app);

      db.commit();
    }
    break;

    case toQueue:
    {
      const QString appName = app->text();

      QSqlQuery selectAppIdQuery(
        "SELECT id "
        "FROM app "
        "WHERE name = ? AND queue_index IS NULL"
      );
      selectAppIdQuery.addBindValue(appName);
      if (!selectAppIdQuery.exec())
      {
        throw ex::db::QueryFailed(selectAppIdQuery.lastError());
      }
      if (!selectAppIdQuery.next())
      {
        err::app::moveFailed.Show();
        return;
      }
      const int appId = selectAppIdQuery.value(0).toInt();

      const int appNewIndex = ui_->queueList->count();

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
        throw ex::db::QueryFailed(updateAppIndexQuery.lastError());
      }
      if (updateAppIndexQuery.numRowsAffected() == 0)
      {
        db.rollback();
        err::app::moveFailed.Show();
        return;
      }

      const int appIndex = ui_->ignoreList->indexFromItem(app).row();
      app = ui_->ignoreList->takeItem(appIndex);
      ui_->queueList->addItem(app);
      ui_->queueList->setCurrentItem(app);

      db.commit();
    }
    break;

    case up:
    case down:
    {
      const int appIndex = ui_->queueList->indexFromItem(app).row();
      const QString appName = app->text();

      const int destAppIndex = appIndex + (direction == up ? -1 : 1);
      const QString destAppName = ui_->queueList->item(destAppIndex)->text();

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
        throw ex::db::QueryFailed(updateAppIndexQuery.lastError());
      }
      if (updateAppIndexQuery.numRowsAffected() == 0)
      {
        db.rollback();
        err::app::moveFailed.Show();
        return;
      }

      updateAppIndexQuery.addBindValue(appIndex);
      updateAppIndexQuery.addBindValue(destAppName);
      if (!updateAppIndexQuery.exec())
      {
        db.rollback();
        throw ex::db::QueryFailed(updateAppIndexQuery.lastError());
      }
      if (updateAppIndexQuery.numRowsAffected() == 0)
      {
        db.rollback();
        err::app::moveFailed.Show();
        return;
      }

      app = ui_->queueList->takeItem(appIndex);
      ui_->queueList->insertItem(destAppIndex, app);
      ui_->queueList->setCurrentItem(app);

      db.commit();
    }
    break;
  }
}

// Slots
void QueueLauncher::onQueueListItemSelectionChanged() const
{
  const auto queueListSelectedItems = ui_->queueList->selectedItems();
  if (queueListSelectedItems.length() > 0)
  {
    ui_->ignoreList->clearSelection();

    ui_->moveButton->setText(">");
    ui_->moveAllButton->setText(">>");

    if (queueListSelectedItems.length() == 1)
    {
      const QString appName = ui_->queueList->currentItem()->text();
      if (!storeAppNames_.contains(appName))
      {
        ui_->renameButton->setEnabled(true);
        ui_->removeButton->setEnabled(true);
      }
      else
      {
        ui_->renameButton->setDisabled(true);
        ui_->removeButton->setDisabled(true);
      }

      const int index = ui_->queueList->currentRow();
      if (index <= 0)
      {
        ui_->moveUpButton->setDisabled(true);
      }
      else // if (index > 0)
      {
        ui_->moveUpButton->setEnabled(true);
      }

      const int itemCount = ui_->queueList->count();
      if (index >= itemCount - 1)
      {
        ui_->moveDownButton->setDisabled(true);
      }
      else // if (index < itemCount - 1)
      {
        ui_->moveDownButton->setEnabled(true);
      }

      ui_->moveButton->setEnabled(true);
      ui_->moveAllButton->setDisabled(true);
    }
    else // if (queueListSelectedItems.length() > 1)
    {
      ui_->renameButton->setDisabled(true);
      ui_->removeButton->setDisabled(true);
      ui_->moveUpButton->setDisabled(true);
      ui_->moveDownButton->setDisabled(true);
      ui_->moveButton->setDisabled(true);
      ui_->moveAllButton->setEnabled(true);
    }
  }
  else // if (queueListSelectedItems.length() == 0)
  {
    ui_->renameButton->setDisabled(true);
    ui_->removeButton->setDisabled(true);
    ui_->moveUpButton->setDisabled(true);
    ui_->moveDownButton->setDisabled(true);
    ui_->moveButton->setDisabled(true);
    ui_->moveAllButton->setDisabled(true);
  }
}

void QueueLauncher::onIgnoreListItemSelectionChanged() const
{
  const auto ignoreListSelectedItems = ui_->ignoreList->selectedItems();
  if (ignoreListSelectedItems.length() > 0)
  {
    ui_->queueList->clearSelection();

    ui_->moveButton->setText("<");
    ui_->moveAllButton->setText("<<");

    if (ignoreListSelectedItems.length() == 1)
    {
      const QString appName = ui_->ignoreList->currentItem()->text();
      if (!storeAppNames_.contains(appName))
      {
        ui_->renameButton->setEnabled(true);
        ui_->removeButton->setEnabled(true);
      }
      else
      {
        ui_->renameButton->setDisabled(true);
        ui_->removeButton->setDisabled(true);
      }

      ui_->moveButton->setEnabled(true);
      ui_->moveAllButton->setDisabled(true);
    }
    else // if (ignoreListSelectedItems.length() > 1)
    {
      ui_->renameButton->setDisabled(true);
      ui_->removeButton->setDisabled(true);
      ui_->moveButton->setDisabled(true);
      ui_->moveAllButton->setEnabled(true);
    }
  }
  else // if (ignoreListSelectedItems.length() == 0)
  {
    ui_->renameButton->setDisabled(true);
    ui_->removeButton->setDisabled(true);
    ui_->moveUpButton->setDisabled(true);
    ui_->moveDownButton->setDisabled(true);
    ui_->moveButton->setDisabled(true);
    ui_->moveAllButton->setDisabled(true);
  }
}

void QueueLauncher::onAddButtonClicked()
{
  QSqlDatabase db = QSqlDatabase::database();

  const QString appPath = QFileDialog::getOpenFileName(
    this,
    tr("Pick an application to add"),
    "",
    tr("Executable Files (*.exe *.bat *.cmd);; All Files (*.*)")
  );
  if (appPath.isEmpty())
  {
    return;
  }

  const QString appName = QFileInfo(appPath).baseName();
  const int appIndex = ui_->queueList->count();

  QSqlQuery selectDuplicatePathCheckQuery(
    "SELECT app_id "
    "FROM local_app "
    "WHERE path = ?"
  );
  selectDuplicatePathCheckQuery.addBindValue(appPath);
  if (!selectDuplicatePathCheckQuery.exec())
  {
    throw ex::db::QueryFailed(selectDuplicatePathCheckQuery.lastError());
  }
  if (selectDuplicatePathCheckQuery.next())
  {
    warn::app::alreadyAdded.Show();
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
    throw ex::db::QueryFailed(insertAppQuery.lastError());
  }
  if (insertAppQuery.numRowsAffected() == 0)
  {
    db.rollback();
    err::app::addFailed.Show();
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
    throw ex::db::QueryFailed(insertLocalAppQuery.lastError());
  }
  if (insertLocalAppQuery.numRowsAffected() == 0)
  {
    db.rollback();
    err::app::addFailed.Show();
    return;
  }

  ui_->queueList->clearSelection();

  ui_->queueList->addItem(appName);
  ui_->queueList->setCurrentRow(appIndex);

  db.commit();
}

void QueueLauncher::onRenameButtonClicked()
{
  QListWidgetItem* app = ui_->queueList->currentItem();
  const QString appName = app->text();

  bool ok = false;
  const QString appNewName = QInputDialog::getText(
    this,
    "Rename",
    "Enter a new name:",
    QLineEdit::Normal,
    appName,
    &ok
  );
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
    throw ex::db::QueryFailed(selectDuplicateNameCheckQuery.lastError());
  }
  if (selectDuplicateNameCheckQuery.next())
  {
    warn::app::nameDuplicated.Show();
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
    throw ex::db::QueryFailed(updateNameQuery.lastError());
  }
  if (updateNameQuery.numRowsAffected() == 0)
  {
    err::app::renameFailed.Show();
    return;
  }

  app->setText(appNewName);
}

void QueueLauncher::onRemoveButtonClicked() const
{
  QSqlDatabase db = QSqlDatabase::database();

  QListWidgetItem* app = nullptr;
  int appIndex = -1;
  if (ui_->queueList->currentRow() > -1)
  {
    app = ui_->queueList->currentItem();
    appIndex = ui_->queueList->currentRow();
  }
  else if (ui_->ignoreList->currentRow() > -1)
  {
    app = ui_->ignoreList->currentItem();
    appIndex = ui_->ignoreList->currentRow();
  }
  else
  {
    return;
  }
  const QString appName = app->text();

  QSqlQuery selectAppIdQuery(
    "SELECT id "
    "FROM app "
    "WHERE name = ?"
  );
  selectAppIdQuery.addBindValue(appName);
  if (!selectAppIdQuery.exec())
  {
    throw ex::db::QueryFailed(selectAppIdQuery.lastError());
  }
  if (!selectAppIdQuery.next())
  {
    err::app::removeFailed.Show();
    return;
  }
  const int appId = selectAppIdQuery.value(0).toInt();

  db.transaction();

  QSqlQuery deleteLocalAppQuery(
    "DELETE FROM local_app "
    "WHERE app_id = ?"
  );
  deleteLocalAppQuery.addBindValue(appId);
  if (!deleteLocalAppQuery.exec())
  {
    db.rollback();
    throw ex::db::QueryFailed(deleteLocalAppQuery.lastError());
  }
  if (deleteLocalAppQuery.numRowsAffected() == 0)
  {
    db.rollback();
    err::app::removeFailed.Show();
    return;
  }

  QSqlQuery deleteAppQuery(
    "DELETE FROM app "
    "WHERE id = ?"
  );
  deleteAppQuery.addBindValue(appId);
  if (!deleteAppQuery.exec())
  {
    throw ex::db::QueryFailed(deleteAppQuery.lastError());
  }
  if (deleteAppQuery.numRowsAffected() == 0)
  {
    db.rollback();
    err::app::removeFailed.Show();
    return;
  }

  // Remove from Queue list
  if (ui_->queueList->currentRow() > -1)
  {
    app = ui_->queueList->takeItem(appIndex);
    try
    {
      SyncAppDbQueueOrder();
    }
    catch (std::exception& e)
    {
      db.rollback();

      ui_->queueList->insertItem(appIndex, app);
      ui_->queueList->setCurrentItem(app);

      throw;
    }
  }
  // Remove from Ignored list
  else if (ui_->ignoreList->currentRow() > -1)
  {
    delete ui_->ignoreList->takeItem(appIndex);
  }
  else
  {
    db.rollback();
    return;
  }

  db.commit();
}

void QueueLauncher::onMoveUpButtonClicked() const
{
  QListWidgetItem* app = ui_->queueList->currentItem();
  MoveApp(up, app);
}

void QueueLauncher::onMoveDownButtonClicked() const
{
  QListWidgetItem* app = ui_->queueList->currentItem();
  MoveApp(down, app);
}

void QueueLauncher::onMoveButtonClicked() const
{
  // Queue to Ignored
  if (ui_->moveButton->text() == ">")
  {
    QListWidgetItem* app = ui_->queueList->currentItem();
    MoveApp(toIgnored, app);
  }
  // Ignored to Queue
  else // if (ui.moveButton->text() == "<")
  {
    QListWidgetItem* app = ui_->ignoreList->currentItem();
    MoveApp(toQueue, app);
  }
}

void QueueLauncher::onMoveAllButtonClicked() const
{
  // Queue to Ignored
  if (ui_->moveAllButton->text() == ">>")
  {
    QList<QListWidgetItem*> apps = ui_->queueList->selectedItems();
    for (QListWidgetItem* app : apps)
    {
      MoveApp(toIgnored, app);
    }
  }
  // Ignored to Queue
  else // if (ui.moveAllButton->text() == "<<")
  {
    QList<QListWidgetItem*> apps = ui_->ignoreList->selectedItems();
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

void QueueLauncher::onLaunchButtonClicked() const
{
  uint queueIndex = 0;
  if (!ui_->queueList->selectedItems().isEmpty())
  {
    queueIndex = ui_->queueList->currentRow();
  }

  if (!utils::LaunchApp(queueIndex))
  {
    return;
  }

  QApplication::quit();
}

// void QueueLauncher::onKeepOpenCheckBoxCheckStateChanged()
// {
//   // TODO: Implement
//   keepOpen_ = ui_->keepOpenCheckBox->isChecked();
//   if (keepOpen_) {}
// }

void QueueLauncher::onPickSteamPathButtonClicked()
{
  QString workingDirectory = consts::defaultSteamInstallPath;
  if (!steamPath_.isEmpty())
  {
    workingDirectory = steamPath_;
  }

  const QString steamPathTemp = QFileDialog::getExistingDirectory(
    this,
    tr("Pick Steam Installation Directory"),
    workingDirectory
  );
  if (!utils::ValidSteamPath(steamPathTemp))
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
    ui_->steamPathLineEdit->clear();
    throw ex::db::QueryFailed(upsertQuery.lastError());
  }
  if (upsertQuery.numRowsAffected() == 0)
  {
    ui_->steamPathLineEdit->clear();
    err::app::saveSteamPathFailed.Show();
    return;
  }

  steamPath_ = steamPathTemp;
  ui_->steamPathLineEdit->setText(steamPathTemp);
}

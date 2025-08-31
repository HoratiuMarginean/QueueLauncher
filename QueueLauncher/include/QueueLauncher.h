#pragma once

#include "ui_QueueLauncher.h"

// Standard Library
#include <unordered_set>

// Qt
#include <qwidget.h>
#include <qmainwindow.h>
#include <qlistwidget.h>
#include <qstring.h>

class QueueLauncher : public QMainWindow
{
  Q_OBJECT

public:
  QueueLauncher(QWidget* parent = nullptr);
  ~QueueLauncher();

private:
  enum MoveDirection { toIgnored, toQueue, up, down };

private:
  void ConnectSlots();
  void InitUi();

  void SyncAppDbQueueOrder();
  void ScanForApps();
  void MoveApp(MoveDirection dir, QListWidgetItem* app);

private slots:
  void onQueueListItemSelectionChanged();
  void onIgnoredListItemSelectionChanged();
  void onAddButtonClicked();
  void onRenameButtonClicked();
  void onRemoveButtonClicked();
  void onMoveUpButtonClicked();
  void onMoveDownButtonClicked();
  void onMoveButtonClicked();
  void onMoveAllButtonClicked();
  void onScanButtonClicked();
  void onLaunchButtonClicked();

  void onPickSteamPathButtonClicked();

private:
  Ui::QueueLauncherClass ui;

  std::unordered_set<QString> storeAppNames;
  QString steamPath;
};
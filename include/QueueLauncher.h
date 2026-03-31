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
  explicit QueueLauncher(QWidget* parent = nullptr);
  ~QueueLauncher() override;

private:
  enum MoveDirection
  {
    toIgnored,
    toQueue,
    up,
    down
  };

private:
  void ConnectSlots();
  void InitUi();

  void SyncAppDbQueueOrder() const;
  void ScanForApps();
  void MoveApp(MoveDirection dir, QListWidgetItem* app) const;

private slots:
  void onQueueListItemSelectionChanged() const;
  void onIgnoreListItemSelectionChanged() const;
  void onAddButtonClicked();
  void onRenameButtonClicked();
  void onRemoveButtonClicked() const;
  void onMoveUpButtonClicked() const;
  void onMoveDownButtonClicked() const;
  void onMoveButtonClicked() const;
  void onMoveAllButtonClicked() const;
  void onScanButtonClicked();
  void onLaunchButtonClicked() const;

  // void onKeepOpenCheckBoxCheckStateChanged();
  void onPickSteamPathButtonClicked();

private:
  std::unique_ptr<Ui::QueueLauncher> ui_;

  std::unordered_set<QString> storeAppNames_;
  QString steamPath_;

  //bool keepOpen_{};
};

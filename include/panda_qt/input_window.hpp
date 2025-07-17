#pragma once

#include <QDialog>
#include <QKeySequence>
#include <QMap>
#include <QPushButton>

#include "input_mappings.hpp"

class InputWindow : public QDialog {
	Q_OBJECT

  public:
	InputWindow(QWidget* parent = nullptr);

	void loadFromMappings(const InputMappings& mappings);
	void applyToMappings(InputMappings& mappings) const;

  signals:
	void mappingsChanged();

  protected:
	bool eventFilter(QObject* obj, QEvent* event) override;

  private:
	QMap<QString, QPushButton*> buttonMap;
	QMap<QString, QKeySequence> keyMappings;

	QString waitingForAction;

	void startKeyCapture(const QString& action);
};

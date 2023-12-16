#pragma once

#include <QApplication>
#include <QDialog>
#include <QWidget>
#include <string>

#include "zep.h"
#include "zep/mode_repl.h"
#include "zep/regress.h"

class TextEditorWindow : public QDialog {
	Q_OBJECT

  private:
	Zep::ZepWidget_Qt zepWidget;
	Zep::IZepReplProvider replProvider;
	static constexpr float fontSize = 14.0f;

  public:
	TextEditorWindow(QWidget* parent, const std::string& filename, const std::string& initialText);
	void setText(const std::string& text) { zepWidget.GetEditor().GetMRUBuffer()->SetText(text); }
};
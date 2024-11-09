#pragma once

#include <QApplication>
#include <QDialog>
#include <QWidget>
#include <string>

#include "zep.h"
#include "zep/mode_repl.h"
#include "zep/regress.h"

class ShaderEditorWindow : public QDialog {
	Q_OBJECT

  private:
	Zep::ZepWidget_Qt zepWidget;
	Zep::IZepReplProvider replProvider;
	static constexpr float fontSize = 14.0f;

  public:
	// Whether this backend supports shader editor
	bool supported = true;

	ShaderEditorWindow(QWidget* parent, const std::string& filename, const std::string& initialText);
	void setText(const std::string& text) { zepWidget.GetEditor().GetMRUBuffer()->SetText(text); }
	void setEnable(bool enable);
};
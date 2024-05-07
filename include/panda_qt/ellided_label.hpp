#pragma once
#include <QFontMetrics>
#include <QLabel>
#include <QString>
#include <QWidget>

class EllidedLabel : public QLabel {
	Q_OBJECT
  public:
	explicit EllidedLabel(Qt::TextElideMode elideMode = Qt::ElideLeft, QWidget* parent = nullptr);
	explicit EllidedLabel(QString text, Qt::TextElideMode elideMode = Qt::ElideLeft, QWidget* parent = nullptr);
	void setText(QString text);

  protected:
	void resizeEvent(QResizeEvent* event);

  private:
	void updateText();
	QString m_text;
	Qt::TextElideMode m_elideMode;
};
#pragma once
#include <QFontMetrics>
#include <QLabel>
#include <QString>
#include <QWidget>

class ElidedLabel : public QLabel {
	Q_OBJECT
  public:
	explicit ElidedLabel(Qt::TextElideMode elideMode = Qt::ElideLeft, QWidget* parent = nullptr);
	explicit ElidedLabel(QString text, Qt::TextElideMode elideMode = Qt::ElideLeft, QWidget* parent = nullptr);
	void setText(QString text);

  protected:
	void resizeEvent(QResizeEvent* event);

  private:
	void updateText();
	QString m_text;
	Qt::TextElideMode m_elideMode;
};
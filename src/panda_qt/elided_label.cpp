#include "panda_qt/elided_label.hpp"

// Based on https://stackoverflow.com/questions/7381100/text-overflow-for-a-qlabel-s-text-rendering-in-qt
ElidedLabel::ElidedLabel(Qt::TextElideMode elideMode, QWidget* parent) : ElidedLabel("", elideMode, parent) {}

ElidedLabel::ElidedLabel(QString text, Qt::TextElideMode elideMode, QWidget* parent) : QLabel(parent) {
	m_elideMode = elideMode;
	setText(text);
}

void ElidedLabel::setText(QString text) {
	m_text = text;
	updateText();
}

void ElidedLabel::resizeEvent(QResizeEvent* event) {
	QLabel::resizeEvent(event);
	updateText();
}

void ElidedLabel::updateText() {
	QFontMetrics metrics(font());
	QString elided = metrics.elidedText(m_text, m_elideMode, width());
	QLabel::setText(elided);
}
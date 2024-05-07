#include "panda_qt/ellided_label.hpp"

// Based on https://stackoverflow.com/questions/7381100/text-overflow-for-a-qlabel-s-text-rendering-in-qt
EllidedLabel::EllidedLabel(Qt::TextElideMode elideMode, QWidget* parent) : EllidedLabel("", elideMode, parent) {}

EllidedLabel::EllidedLabel(QString text, Qt::TextElideMode elideMode, QWidget* parent) : QLabel(parent) {
	m_elideMode = elideMode;
	setText(text);
}

void EllidedLabel::setText(QString text) {
	m_text = text;
	updateText();
}

void EllidedLabel::resizeEvent(QResizeEvent* event) {
	QLabel::resizeEvent(event);
	updateText();
}

void EllidedLabel::updateText() {
	QFontMetrics metrics(font());
	QString elided = metrics.elidedText(m_text, m_elideMode, width());
	QLabel::setText(elided);
}
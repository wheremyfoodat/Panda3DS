#include "panda_qt/about_window.hpp"
#include "version.hpp"

#include <QLabel>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QtGlobal>

// Based on https://github.com/dolphin-emu/dolphin/blob/master/Source/Core/DolphinQt/AboutDialog.cpp

AboutWindow::AboutWindow(QWidget* parent) : QDialog(parent) {
	resize(200, 200);

	setWindowTitle(tr("About Panda3DS"));
	setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

	const QString text =
		QStringLiteral(R"(
<p style='font-size:38pt; font-weight:400;'>Panda3DS</p>

<p style='font-size:18pt;'>v%VERSION_STRING%</p>

<p>
%ABOUT_PANDA3DS%<br>
<a href='https://panda3ds.com/'>%SUPPORT%</a><br>
</p>

<p>
<a>%AUTHORS%</a>
</p>
)")
			.replace(QStringLiteral("%VERSION_STRING%"), PANDA3DS_VERSION)
			.replace(QStringLiteral("%ABOUT_PANDA3DS%"), tr("Panda3DS is a free and open source Nintendo 3DS emulator, for Windows, MacOS and Linux"))
			.replace(QStringLiteral("%SUPPORT%"), tr("Visit panda3ds.com for help with Panda3DS and links to our official support sites."))
			.replace(
				QStringLiteral("%AUTHORS%"), tr("Panda3DS is developed by volunteers in their spare time. Below is a list of some of these"
												" volunteers who've agreed to be listed here, in no particular order.<br>If you think you should be "
												"listed here too, please inform us<br><br>"
												"- Peach (wheremyfoodat)<br>"
												"- noumidev<br>"
												"- liuk707<br>"
												"- Wunk<br>"
												"- marysaka<br>"
												"- Sky<br>"
												"- merryhime<br>"
												"- TGP17<br>"
												"- Shadow<br>")
			);

	QLabel* textLabel = new QLabel(text);
	textLabel->setTextInteractionFlags(Qt::TextBrowserInteraction);
	textLabel->setOpenExternalLinks(true);

	QLabel* logo = new QLabel();
	logo->setPixmap(QPixmap(":/docs/img/rstarstruck_icon.png"));
	logo->setContentsMargins(30, 0, 30, 0);

	QVBoxLayout* mainLayout = new QVBoxLayout;
	QHBoxLayout* hLayout = new QHBoxLayout;

	setLayout(mainLayout);
	mainLayout->addLayout(hLayout);

	hLayout->setAlignment(Qt::AlignLeft);
	hLayout->addWidget(logo);
	hLayout->addWidget(textLabel);
}
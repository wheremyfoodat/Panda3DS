#include <QFile>
#include <QTranslator>
#include <array>
#include <cstdio>

#include "panda_qt/config_window.hpp"
#include "panda_qt/main_window.hpp"

void MainWindow::loadTranslation() {
	// TODO: This should become a member variable when we allow changing language at runtime.
	QTranslator* translator = nullptr;

	// Fetch the .qm file for our language and load it
	auto language = QString::fromStdString(emu->getConfig().frontendSettings.language);
	const QString baseDir = QStringLiteral(":/translations");
	const QString basePath = QStringLiteral("%1/%2.qm").arg(baseDir).arg(language);

	if (QFile::exists(basePath)) {
		if (translator != nullptr) {
			qApp->removeTranslator(translator);
		}

		translator = new QTranslator(qApp);
		if (!translator->load(basePath)) {
			QMessageBox::warning(
				nullptr, QStringLiteral("Translation Error"),
				QStringLiteral("Failed to find load translation file for '%1':\n%2").arg(language).arg(basePath)
			);
			delete translator;
		} else {
			qApp->installTranslator(translator);
		}
	} else {
		printf("Language file %s does not exist. Defaulting to English\n", basePath.toStdString().c_str());
	}
}

struct LanguageInfo {
	QString name;      // Full name of the language (for example "English (US)")
	const char* code;  // ISO 639 language code (for example "en_us")

	explicit LanguageInfo(const QString& name, const char* code) : name(name), code(code) {}
};

// List of languages in the order they should appear in the menu
// Please keep this list mostly in alphabetical order.
// Also, for Unicode characters in language names, use Unicode keycodes instead of writing out the name,
// as some compilers/toolchains may not enjoy Unicode in source files.
static std::array<LanguageInfo, 6> languages = {
	LanguageInfo(QStringLiteral(u"English"), "en"),                                           // English
	LanguageInfo(QStringLiteral(u"\u0395\u03BB\u03BB\u03B7\u03BD\u03B9\u03BA\u03AC"), "el"),  // Greek
	LanguageInfo(QStringLiteral(u"Espa\u00F1ol"), "es"),                                      // Spanish
	LanguageInfo(QStringLiteral(u"Nederlands"), "nl"),                                        // Dutch
	LanguageInfo(QStringLiteral(u"Portugu\u00EAs (Brasil)"), "pt_br"),                        // Portuguese (Brazilian)
	LanguageInfo(QStringLiteral(u"Svenska"), "sv"),                                           // Swedish
};

QComboBox* ConfigWindow::createLanguageSelect() {
	QComboBox* select = new QComboBox();

	for (usize i = 0; i < languages.size(); i++) {
		const auto& lang = languages[i];
		select->addItem(lang.name);

		if (config.frontendSettings.language == lang.code) {
			select->setCurrentIndex(i);
		}
	}

	connect(select, &QComboBox::currentIndexChanged, this, [&](int index) {
		const QString baseDir = QStringLiteral(":/translations");
		const QString basePath = QStringLiteral("%1/%2.qm").arg(baseDir).arg(languages[index].code);

		if (QFile::exists(basePath)) {
			config.frontendSettings.language = languages[index].code;
			updateConfig();

			QMessageBox messageBox(
				QMessageBox::Icon::Information, tr("Language change successful"),
				tr("Restart Panda3DS for the new language to be used.")
			);

			messageBox.exec();
		} else {
			QMessageBox messageBox(
				QMessageBox::Icon::Warning, tr("Language change failed"),
				tr("The language you selected is not included in Panda3DS. If you're seeing this, someone messed up the language UI code...")
			);

			messageBox.exec();
		}
	});

	return select;
}

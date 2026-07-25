QT       += core gui widgets

CONFIG   += c++17

TARGET   = booktitlecase
TEMPLATE = app

SOURCES += \
    main.cpp \
    MainWindow.cpp \
    SettingsDialog.cpp \
    Dictionary.cpp \
    WordSplitter.cpp \
    TitleCaser.cpp \
    FileScanner.cpp \
    MetadataExtractor.cpp

HEADERS += \
    MainWindow.h \
    SettingsDialog.h \
    Dictionary.h \
    WordSplitter.h \
    TitleCaser.h \
    FileScanner.h \
    BookGroup.h \
    MetadataExtractor.h

FORMS   += \
    MainWindow.ui \
    SettingsDialog.ui

DISTFILES += \
    tech_dict.txt

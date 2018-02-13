QT      -= core gui
CONFIG  -= c++11

QMAKE_CXXFLAGS += -std=c++14
QMAKE_LFLAGS   += -std=c++14
LIBS           += -lsfml-graphics -lsfml-window -lsfml-system \
                  -L/usr/lib/x86_64-linux-gnu -L$$PWD/../ksg -L$$PWD/../util-common

linux {
    QMAKE_CXXFLAGS += -DMACRO_PLATFORM_LINUX
    contains(QT_ARCH, i386) {
        LIBS += -L../../bin/linux/g++-x86
    } else:contains(QT_ARCH, x86_64) {
        LIBS += -L../../bin/linux/g++-x86_64 \
                -L/usr/lib/x86_64-linux-gnu
    }
}

debug {
    TARGET  = ksg-te-d
    LIBS   += -lcommon-d -lksg-d
} else:release {
    TARGET  = ksg-te
    #LIBS   += -lcommon -lksg
}

SOURCES += \
    ../src/main.cpp \
    ../src/TextLines.cpp \
    ../src/TextLine.cpp \
    ../src/TextGrid.cpp \
    ../src/UserTextSelection.cpp \
    ../src/TargetTextGrid.cpp

HEADERS += \
    ../src/TextLines.hpp \
    ../src/TextLine.hpp \
    ../src/TextGrid.hpp \
    ../src/UserTextSelection.hpp \
    ../src/Cursor.hpp \
    ../src/TargetTextGrid.hpp

INCLUDEPATH += \
    ../ksg/inc      \
    ../util-common/inc

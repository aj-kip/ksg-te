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

release {
    TARGET  = ksg-te
    #LIBS   += -lcommon -lksg
}

debug {
    TARGET   = ksg-te-d
    LIBS    += -lcommon-d -lksg-d
    QMAKE_CXXFLAGS += -DMACRO_DEBUG
}

message($$CONFIG)

SOURCES += \
    ../src/main.cpp \
    ../src/TextLines.cpp \
    ../src/TextLine.cpp \
    ../src/UserTextSelection.cpp \
    ../src/TargetTextGrid.cpp \
    ../src/KsgTextGrid.cpp \
    ../src/LuaCodeModeler.cpp \
    ../src/TextLineImage.cpp

HEADERS += \
    ../src/TextLines.hpp \
    ../src/TextLine.hpp \
    ../src/UserTextSelection.hpp \
    ../src/Cursor.hpp \
    ../src/TargetTextGrid.hpp \
    ../src/KsgTextGrid.hpp \
    ../src/IteratorPair.hpp \
    ../src/LuaCodeModeler.hpp \
    ../src/TextLineImage.hpp

INCLUDEPATH += \
    ../ksg/inc      \
    ../util-common/inc

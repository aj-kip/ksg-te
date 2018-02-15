CXX = g++
LD = g++
CXXFLAGS = -std=c++14 -O3 -I./inc -Iutil-common/inc -Iksg/inc -Wall -pedantic -Werror -DMACRO_PLATFORM_LINUX
SOURCES  = $(shell find src | grep '[.]cpp$$')
OBJECTS_DIR = .debug-build
OBJECTS = $(addprefix $(OBJECTS_DIR)/,$(SOURCES:%.cpp=%.o))

$(OBJECTS_DIR)/%.o: | $(OBJECTS_DIR)/src
	$(CXX) $(CXXFLAGS) -c $*.cpp -o $@

.PHONY: default
default: $(OBJECTS)
	@echo $(SOURCES)
	g++ $(OBJECTS) -Lksg -Lutil-common  -lsfml-graphics -lsfml-window -lsfml-system -lksg-d -lcommon-d -O3 -o ksg-te-d

$(OBJECTS_DIR)/src:
	mkdir -p $(OBJECTS_DIR)/src
.PHONY: clean
clean:
	rm -rf $(OBJECTS_DIR)

DEMO_OPTIONS = -g -L/usr/lib/ -L./demos -lsfml-system -lsfml-graphics -lsfml-window -lcommon-d -lksg-d
.PHONY: demos

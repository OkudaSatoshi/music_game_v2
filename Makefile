CXX = g++
CXXFLAGS = -std=c++11 -Wall -Ilibs/midifile/include -Ilibs/json -finput-charset=UTF-8 -fexec-charset=UTF-8
LDLIBS = -lsfml-graphics -lsfml-window -lsfml-system -lsfml-audio
TARGET = soundgame.exe
SRC = src/main.cpp src/file_utils.cpp
LIB_SRC = $(wildcard libs/midifile/src/*.cpp)
OBJS = $(SRC:.cpp=.o) $(LIB_SRC:.cpp=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) -o $(TARGET) $(OBJS) $(LDLIBS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET) $(OBJS)
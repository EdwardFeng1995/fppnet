CXXFLAGS = -O0 -g  -Wall -I ../.. -pthread
LDFLAGS = -lpthread -lmuduo -L.
BASE_SRC = ../../src/base/logging/Logging.cpp ../../src/base/logging/LogStream.cpp ../../src/base/thread/Thread.cpp ../../src/base/datetime/Timestamp.cpp
MUDUO_SRC = $(notdir $(LIB_SRC) $(BASE_SRC))
OBJS = $(patsubst %.cpp,%.o,$(MUDUO_SRC))

libmuduo.a: $(BASE_SRC) $(LIB_SRC)
	g++ $(CXXFLAGS) -c $^
	ar rcs $@ $(OBJS)

$(BINARIES): libmuduo.a
	g++ $(CXXFLAGS) -o $@ $(filter %.cc,$^) $(LDFLAGS)

clean:
	rm -f $(BINARIES) *.o *.a core


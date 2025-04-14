# location of make.config
SWATCONFIGDIR = ./
# defines SWATDBDIR
include $(SWATCONFIGDIR)/make.config

# path to SwatDB library files
LIBDIR = $(SWATDBDIR)lib/

# paths to include directories
INCLUDES = -I. -I$(SWATDBDIR)include/

# compiler
CC = g++

# compiler flags for test code build
CFLAGS =  -g -Wall #-pthread

# lflags for linking
LFLAGS = -L$(LIBDIR)

# swatdb and other libraries to link in 
LIBS = $(LFLAGS) -lswatdb -lm -pthread -lcrypto -lssl -lUnitTest++

SRCS=  heapfile.cpp heapfilescanner.cpp


# suffix replacement rule
OBJS = $(SRCS:.cpp=.o)

# be very careful to not add any spaces to ends of these
TARGET1 = checkpt
TARGET2 = fileunittests
TARGET3 = checksaved
TARGET4 = filescannerunittests

# generic makefile
.PHONY: clean 

all: $(TARGET1) $(TARGET2) $(TARGET3) $(TARGET4) 

$(TARGET1): $(OBJS) $(TARGET1).cpp *.h *.sh
	$(CC) $(CFLAGS) $(INCLUDES) -o $(TARGET1) $(TARGET1).cpp $(OBJS) $(LIBS)

$(TARGET2): $(OBJS) $(TARGET2).cpp *.h *.sh
	$(CC) $(CFLAGS) $(INCLUDES) -o $(TARGET2) $(TARGET2).cpp $(OBJS) $(LIBS)

$(TARGET3): $(OBJS) $(TARGET3).cpp *.h *.sh
	$(CC) $(CFLAGS) $(INCLUDES) -o $(TARGET3) $(TARGET3).cpp $(OBJS) $(LIBS)

$(TARGET4): $(OBJS) $(TARGET4).cpp *.h *.sh
	$(CC) $(CFLAGS) $(INCLUDES) -o $(TARGET4) $(TARGET4).cpp $(OBJS) $(LIBS)


# suffix replacement rule using autmatic variables:
# automatic variables: $< is the name of the prerequiste of the rule
# (.cpp file),  and $@ is name of target of the rule (.o file)
.cpp.o: $(SRCS) *.h *.sh
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

runtests: 
	./$(TARGET1)
	sleep 2
	./$(TARGET2)
	sleep 2
	./$(TARGET3)
	sleep 2
	./$(TARGET4)
	sleep 2

clean:
	./cleanup.sh
	$(RM) *.o $(TARGET1) $(TARGET2)  $(TARGET3) $(TARGET4)


CXX		= g++ -std=c++11
ICC		= icc -std=c++11

CXXFLAGS  	= -Wall -DBLOCKING_MODE -DNO_DEFAULT_MAPPING 

LDFLAGS 	=  -lpthread
OPTFLAGS	=  -O3 -finline-functions -DNDEBUG 
OTHERFLAGS	=  -L/usr/X11R6/lib -lm
XFLAG		=  -lX11

FF_ROOT		= .

INCLUDES	= -I $(FF_ROOT) 


TARGETS		=  \
	Filter \
	IFilter


.PHONY: Filter IFilter clean cleanall
.SUFFIXES: .cpp 


Filter : Filter.cpp
	$(CXX) $< $(CXXFLAGS) $(INCLUDES) $(OPTFLAGS) $(OTHERFLAGS) $(LDFLAGS) $(XFLAG) -o Filter

IFilter : Filter.cpp
	$(ICC) $< $(CXXFLAGS) $(INCLUDES) $(OPTFLAGS) $(OTHERFLAGS) $(LDFLAGS) $(XFLAG) -o IFilter

clean		: 
	rm -f $(TARGETS) *.bmp
cleanall	: clean
	\rm -f *.o *~

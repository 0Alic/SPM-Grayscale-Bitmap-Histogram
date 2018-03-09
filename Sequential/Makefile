
CXX		= g++ -std=c++11
ICC		= icc -std=c++11

CXXFLAGS  	= -Wall

LDFLAGS 	=  -lpthread
OPTFLAGS	=  -O3 -finline-functions
OTHERFLAGS	=  -L/usr/X11R6/lib -lm
XFLAG		=  -lX11

TARGETS = \
	Filter \
	IFilter 

.PHONY: Filter IFilter clean cleanall
.SUFFIXES: .cpp 


Filter : Filter.cpp
	$(CXX) $< $(CXXFLAGS) $(OPTFLAGS) $(OTHERFLAGS) $(LDFLAGS) $(XFLAG) -o Filter

IFilter : Filter.cpp
	$(ICC) $< $(CXXFLAGS) $(OPTFLAGS) $(OTHERFLAGS) $(LDFLAGS) $(XFLAG) -o IFilter

clean		: 
	rm -f $(TARGETS) *.bmp
cleanall	: clean
	\rm -f *.o *~

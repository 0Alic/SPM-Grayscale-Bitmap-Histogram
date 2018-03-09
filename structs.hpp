#include "CImg.h"
#include <vector>

using namespace cimg_library;
using namespace std;

/* Data exanghed within stages */

// Input data
typedef struct in {

	in(){}
	in(bool le) : last_element(le) {}
	in(CImg<unsigned char> img, double sig, bool le) : image(img), sigma(sig), last_element(le) {}

	CImg<unsigned char> image;
	double 				sigma=0;
	bool 				last_element;

} inputData;

// Data between two stages
typedef struct mid {

	mid(){}
	mid(bool le) : last_element(le) {}
	mid(vector<pair<int, int>> histo, CImg<unsigned char> img, double sig, bool le) : 
		histogram(histo), image(img), sigma(sig), last_element(le) {}

	vector<pair<int, int>>	histogram;
	CImg<unsigned char> 	image;
	double 					sigma=0;
	bool 					last_element;

} midData;

// Output data
typedef struct out {

	out(){}
	out(bool le) : last_element(le) {}
	out(CImg<unsigned char> img, bool le) : image(img), last_element(le) {}

	CImg<unsigned char> image;
	bool 				last_element;

} outputData;
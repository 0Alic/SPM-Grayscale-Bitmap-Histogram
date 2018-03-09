#include "../CImg.h"
#include "../functions.hpp"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <iostream>
#include <chrono>
#include <vector>
#include <math.h>

using namespace cimg_library;
using namespace std;
using namespace std::chrono;


/*
	Implement BW Filter
	
	Here are present the probes to compute the times spent by each component of the application

	./Filter -streamlen -definition -sigma

	-streamlen=1			the stream length
	-definition=hd 			the definition of the image: ld/md/hd
	-sigma=0.1				the sigma parameter of the filter

*/
int main(int argc, char const *argv[]) {

	string path = "./../Data/";
	string filename;

	int stream_len 	= 1;
	string definition	= "hd";
	double SIGMA 	= 0.1;

	if(argc >= 2) stream_len	= stoi(argv[1]);
	if(argc >= 3) definition 	= argv[2];
	if(argc >= 4) SIGMA 		= stod(argv[3]); 
	
	if(SIGMA > 1) SIGMA = 1;
	else if(SIGMA < 0) SIGMA = 0;

	filename = "test" + definition + ".bmp";
	// Processing time variables

	high_resolution_clock::time_point 	t_init, t_end,
										t_load, t_save,	
										t_init_h, t_end_h,
										t_init_f, t_end_f;

	duration<double, std::milli> 	elapse,
									elapse_load, elapse_save,
									elapse_h, elapse_f;
	double avg_h = 0, avg_f = 0;




	/************************************************************************/
	/*						Serial fraction: Load							*/
	/************************************************************************/

	// Helpers variables
	int width;
	int height;


	t_init = high_resolution_clock::now();	/* ld; tot */

	CImg<unsigned char> src;
	src.assign((path + filename).data()); 	// param as const char*

	t_load = high_resolution_clock::now();	/* ld */
	elapse_load = t_load - t_init;			/* ld */	

	width = src.width();
	height = src.height();

	/************************************************************************/
	/*		Parallilizable Frac:Compute total and partial times				*/
	/************************************************************************/

	for(int i=0; i<stream_len; ++i) {

		t_init_h = high_resolution_clock::now();	/* h */


			// Each element of the histogram, at position i, is composed by:
			// #pixels with color i;
			// sum(histogram(0)[i+1 .. 255]): the partial sul of the pixels brighter than i
		vector<pair<int, int>> histogram(256);


		buildHistogram(src, &histogram, 0, height);
		computePartialSums(&histogram);


		t_end_h = high_resolution_clock::now();		/* h */
		elapse_h = t_end_h - t_init_h;				/* h */
		avg_h += elapse_h.count();					/* h */
	

		t_init_f = high_resolution_clock::now();	/* f */

		filterBitmap(&src, histogram, 0, height, SIGMA);

		t_end_f = high_resolution_clock::now(); 	/* f */
		elapse_f = t_end_f - t_init_f;				/* f */
		avg_f += elapse_f.count();					/* f */

	}

	/************************************************************************/
	/*						Serial Fraction: Save image						*/
	/************************************************************************/

	t_save = high_resolution_clock::now();		/* sv */

	string n = "bw_" + filename;
	src.save(n.data());

	t_end = high_resolution_clock::now();		/* sv; tot */
	elapse_save = t_end - t_save;				/* sv */
	elapse = t_end - t_init;					/* tot */
	t_end = high_resolution_clock::now();		/* tot */
	elapse = t_end - t_init;					/* tot */

	/************************************************************************/
	/*								Print Info								*/
	/************************************************************************/

	cout << 
	"n 1" << 
	" s " << stream_len <<
	" t " << elapse.count() << 
	" ld " << elapse_load.count() <<
	" h " << (avg_h / stream_len) <<
	" f " << (avg_f / stream_len) <<
	" sv " << elapse_save.count() <<
	" w " << width <<
	" h " << height <<
	"\n";

	return 0;
}
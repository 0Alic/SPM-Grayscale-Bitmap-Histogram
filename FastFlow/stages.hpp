#include "../CImg.h"
#include "../structs.hpp"
#include "../functions.hpp"
#include "ff/map.hpp"
#include <iostream>	
#include <vector>
#include <atomic>
#include <math.h> // ceil, floor
#include <chrono>

using namespace cimg_library;
using namespace std;
using namespace std::chrono;
using namespace ff;



/* Service time and Latecy, computed by the stages*/
double serviceTime = 0;
atomic<long> latencyH(0);
atomic<long> latencyF(0);
atomic<long> latencyComp(0);

			/**********************************************************************/
			/*						The program's stages						  */
			/**********************************************************************/


/*
 * Stage 1: load an image from the disk and send it
 *
 * Constructor's params:
 *	filen: 		the file name of the stream data
 *	sig:		sigma parameter of the filter
 *	stream_len: length of the stream to send
 *
 *	
*/
struct Generator : ff_monode_t<inputData> {

	string filename;
	double sigma;
	int stream_len;

	Generator(string filen, double sig, int n) : filename(filen), sigma(sig), stream_len(n) { }


	inputData* svc(inputData* e){

		CImg<unsigned char> img(filename.data()); // load image

		// Generate stream
 		for(int i=0; i<stream_len; i++){
			ff_send_out(new inputData(img, sigma, false));
 		}
		return EOS;
	}
};







/*
 * Stage 2: compute the histogram
 * The internal computation may be parallelized
 *
 * Constructor's params:
 *	nWorkers:	the number of partitions the image has to be split into during the "histogram computation" phase
 *	
*/
struct HistogramPar : ff_Map<inputData, midData>{


	high_resolution_clock::time_point 	t_init, t_end;
	duration<double, std::milli> elapse;

	int nWorkers = 1;

	HistogramPar(int nW) : ff_Map<inputData, midData>(nW, true) { nWorkers = nW; }


	midData* svc(inputData* el){


		t_init = high_resolution_clock::now();								/* latH: init */

		// Histogram of 256 positions, one for each color value
			// first: at [i] -> #pixels of color i
			// second: at [i] -> sum j: i+1, 256 (histogram[j].first), ie partial sums of the elements on its right portion
		vector<pair<int, int>> histogram;
		// Build as many partial histograms as many threads we have
		vector< vector< pair<int, int>>>  partials(nWorkers, vector< pair<int, int>>(COLORMAXVAL));


		int width = el->image.width();
		int height = el->image.height();


		// Compute the chunks of image, given by the number of workers
		int chunk = ceil(float(height) / float(nWorkers));

			// Static step computation, I don't have load balancing issues				
		ff_Map<inputData, midData>::parallel_for(0, height, [&partials, el, width, chunk](int y){

			int portion = floor(y / chunk);

			for(int x=0; x<width; x++){				
				int color = el->image(x, y, 0, 0);
				partials[portion][color].first++;
			}
		}, nWorkers);


		// Merge partial results in final histogram
		histogram = sumUpVectors(partials);

		// Compute partial sums
		computePartialSums(&histogram);

		// New element to send out
		midData* out = new midData(histogram, el->image, el->sigma, false);
		delete el;

		t_end = high_resolution_clock::now();								/* latH: end */
		elapse = t_end - t_init;		
		latencyH += long(elapse.count());									/* latH: update*/

		return out;
	}
};






/*
 * Stage 3: compute the filter
 * The internal computation may be parallelized
 *
 * Constructor's params:
 *	nWorkers:	the number of partitions the image has to be split into during the "filter computation" phase
 *	
*/
struct FilterPar : ff_Map<midData, outputData>{


	high_resolution_clock::time_point 	t_init, t_end;
	duration<double, std::milli> elapse;

	int nWorkers = 1;

		// call constructor super, spinWait = true
	FilterPar(int nW) : ff_Map<midData, outputData>(nW, true) { nWorkers = nW; }


	outputData* svc(midData* el) {

		t_init = high_resolution_clock::now();									/* latF: init */

		int width = el->image.width();
		int height = el->image.height();
		int area = width*height;

			// Static step computation, I don't have load balancing issues
		ff_Map<midData, outputData>::parallel_for(0, height, [el, width, area](int y){

			for(int x=0; x<width; x++){				

				int pixel_color = el->image(x, y, 0, 0);
				double perc = el->histogram[pixel_color].second / double(area);

				setColor(&(el->image), x, y, white*(perc <= el->sigma));
			}
		}, nWorkers);


		outputData* out = new outputData(el->image, false);
		delete el;

		t_end = high_resolution_clock::now();									/* latF: end */
		elapse = t_end - t_init;
		latencyF += long(elapse.count());										/* latF: update */

		return out;
	}
};



/*
 * Stage 4: visualize the images
 * The first time it get the output image save it and store in the disk at the really end
 *
 * Constructor's params:
 *	filen: the file name of the stream data
 *
*/
struct Visualizer : ff_minode_t<outputData>{

	// Time Processing variables
		// Measure the time spent from the end of a svc call till the begin of a new one,
		// that means: a new element is incoming
	high_resolution_clock::time_point 	t_init, t_end;
	duration<double, std::milli> elapse;
	bool skippedFirstEl = false;	// Skip first element, since it "accumulates" the time spent to load the image from disk

	string filename;
	CImg<unsigned char> img;
	bool took = false;


	Visualizer(string filen) : filename(filen) {}


	outputData* svc(outputData* el){
		

		if(skippedFirstEl) {
			t_end = high_resolution_clock::now();							/* serv time: end */ 	// In this version to compute the service time
			elapse = t_end - t_init;																// is taken the time elapsed between 2 svc calls
			serviceTime += long(elapse.count());							/* serv time: update */
		}

		if(!took){
			took = true;
			img = el->image;
		}

		delete el;

		if(!skippedFirstEl)
			skippedFirstEl = true;

		t_init = high_resolution_clock::now();								/* serv time: init */

		return GO_ON;
	}

	void svc_end(){
		string out = "bw_"+filename;
		img.save(out.data());
	}
};



			/**********************************************************************/
			/*					Sequential versions of inner stages				  */
			/**********************************************************************/

/*
 * Stage 2: Sequential version
 *	
*/
struct HistogramSeq : ff_node_t<inputData, midData>{

	// Time processing variables
	high_resolution_clock::time_point 	t_init, t_end;
	duration<double, std::milli> elapse;


	midData* svc(inputData* el){

		t_init = high_resolution_clock::now();					/* lat_H : init */


		vector<pair<int, int>> histogram(COLORMAXVAL);
		int height = el->image.height();
		
		buildHistogram(el->image, &histogram, 0, height);		
		computePartialSums(&histogram);

		midData* out = new midData(histogram, el->image, el->sigma, false);
		delete el;


		t_end = high_resolution_clock::now();					/* lat_H : end */
		elapse = t_end - t_init;								
		latencyComp += long(elapse.count());					/* lat_H : update */

		return out;
	}	
};





/*
 * Stage 3: Sequential version
 *	
*/
struct FilterSeq : ff_node_t<midData, outputData>{

	// Time Processing variables
	high_resolution_clock::time_point 	t_init, t_end;
	duration<double, std::milli> elapse;

	outputData* svc(midData* el) {

		t_init = high_resolution_clock::now();					/* lat_F : init */


		filterBitmap(&el->image, el->histogram, 0, el->image.height(), el->sigma);

		outputData* out = new outputData(el->image, false);
		delete el;


		t_end = high_resolution_clock::now();					/* lat_F : end */
		elapse = t_end - t_init;								
		latencyComp += long(elapse.count());					/* lat_F : update */

		return out;
	}
};
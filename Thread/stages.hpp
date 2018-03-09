#include "MyQueue.hpp"
#include "../functions.hpp"
#include "../CImg.h"
#include "../structs.hpp"
#include <fstream>
#include <vector>
#include <string>
#include <thread>
#include <atomic>
#include <chrono>

using namespace std::chrono;
using namespace cimg_library;
using namespace std;


/* Service time and Latecy, computed by the stages*/
double serviceTime = 0;
atomic<long> latencyH(0);
atomic<long> latencyF(0);
atomic<long> latencyComp(0);


			/**********************************************************************/
			/*				The Queue's for data generation/collection			  */
			/**********************************************************************/

MyQueue<inputData> 		inQueue;
MyQueue<outputData> 	outQueue;




			/**********************************************************************/
			/*						The program's stages						  */
			/**********************************************************************/

/*
 * Stage 1: loads an image from the disk and generates a stream of copies.
 * At the end, sends a number of "last_element" token equal to the number of workers at the next stage
 *
 * Params:
 *	filename: 	the file name of the stream data
 *	sigma:		sigma parameter of the filter
 *	stream_len: length of the stream to send
 *	nWorkers:	number of workers in the farm
 *	
*/
void Generator(string 	filename, 
			   double 	sigma, 
			   int 		stream_len, 
			   int 		nWorkers
			   ) {

	CImg<unsigned char> img(filename.data());

	// Produce stream
	for(int i=0; i<stream_len; i++){

		// Create new stream element
		inputData* el = new inputData(img, sigma, false);
		// Put into the queue
		inQueue.push(el);
	}

	// send last element
	for(int i=0; i<nWorkers; i++){
		inputData* lastEl = new inputData(true);
		inQueue.push(lastEl);
	}
	return;
}





/*
 * Stage 2: compute the histogram
 * The internal computation may be parallelized
 *
 * Constructor's params:
 *	nWorkers:	the number of partitions the image has to be split into during the "histogram computation" phase
 *	midQueue:	a pointer to the queue for outgoing data
 *	
*/
void Histogram(int 				 nWorkers, 
			   MyQueue<midData>* midQueue
			  ) {

	high_resolution_clock::time_point 	t_init, t_end;
	duration<double, std::milli> elapse;


	vector<thread> workers;

	// Get element from input queue
	inputData* el = inQueue.get();

	while(!el->last_element){

		t_init = high_resolution_clock::now();							/* latH: init */

		// Histogram of 256 positions, one for each color value
			// first: at [i] -> #pixels of color i
			// second: at [i] -> sum j: i+1, 256 (histogram[j].first), ie partial sums of the elements on its right portion
		vector<pair<int, int>> histogram(COLORMAXVAL);

		int height = el->image.height();


		// Prepare Data Parallel partitions
			// Build as many partial histograms as many threads we have
		vector< vector< pair<int, int>>>  partials(nWorkers, vector< pair<int, int>>(COLORMAXVAL));

		// Partition's offset
		int off = height / nWorkers;

		for(int i=0; i<nWorkers; i++){

			int yStart 	= i*off;
			int yEnd 	= min(height, (i+1)*off);

			// Spawn parallel workers to compute the histogram
			workers.push_back(thread(buildHistogram, el->image, &(partials[i]), yStart, yEnd));
		}

		for(int i=0; i<nWorkers; i++){
			workers.back().join();
			workers.pop_back();
		}

		// Merge partial results in final histogram
		histogram = sumUpVectors(partials);

		computePartialSums(&histogram);

		midData* out = new midData(histogram, el->image, el->sigma, false);
		delete el;

		midQueue->push(out);

		t_end = high_resolution_clock::now();							/* latH: end */
		elapse = t_end - t_init;
		latencyH += long(elapse.count());								/* latH: update */


		el = inQueue.get();
	}

	midData* lastEl = new midData(true);
	midQueue->push(lastEl);
	delete el;

	return;
}






/*
 * Stage 3: filter the image
 * The internal computation may be parallelized
 *
 * Constructor's params:
 *	nWorkers:	the number of partitions the image has to be split into during the "filter computation" phase
 *	midQueue:	a pointer to the queue for incoming data
*/
void Filter(int 				nWorkers, 
			MyQueue<midData>* 	midQueue
		   ) {

	high_resolution_clock::time_point 	t_init, t_end;
	duration<double, std::milli> elapse;


	vector<thread> workers;

	// Get element from parameter queue
	midData* el = midQueue->get();

	while(!el->last_element){

		t_init = high_resolution_clock::now();								/* latF: init */

		int height = el->image.height();

		// Partition's offset
		int off = height / nWorkers;

		for(int i=0; i<nWorkers; i++){

			int yStart 	= i*off;
			int yEnd 	= min(height, (i+1)*off);

			// Spawn parallel workers to filter the image
			workers.push_back(thread(filterBitmap, &el->image, el->histogram, yStart, yEnd, el->sigma));
		}

		for(int i=0; i<nWorkers; i++){
			workers.back().join();
			workers.pop_back();
		}

		outputData* out = new outputData(el->image, false);

		outQueue.push(out);
		delete el;

		t_end = high_resolution_clock::now();								/* latF: end */
		elapse = t_end - t_init;
		latencyF += long(elapse.count());									/* latF: update */

		el = midQueue->get();		
	}

	outputData* lastEl = new outputData(true);
	outQueue.push(lastEl);
	delete el;

	return;
}





/*
 * Stage 4: visualize the images
 * It as to wait a number of 'last_element' token equal to the number of the farm's workers before terminating
 * The first time it get the output image save it and store in the disk at the really end
 *
 * Params:
 *	filename: the file name of the stream data
 *	nWorkers: the number of workers in the farm
 *
*/
void Visualizer(string 	filename, 
				int 	nWorkers
			   ) {

	high_resolution_clock::time_point 	t_init, t_end;
	duration<double, std::milli> elapse;

	int nLastGet = 0;
	bool took = false;
	CImg<unsigned char> img;

	outputData* el;

	// This step is done in order to "skip" the first received element in the stream for the
	//	purposes of computing the service time. This first elements "accumulates" the time spent
	//	loading the image
	el = outQueue.get();

	if(!el->last_element) {
		if(!took){
			// Store the image variable the first time I get it 
			took = true;
			img = el->image;
		}
	}
	else {
		// Remember how many "last element" tokens I get
		nLastGet++;
	}

	// While I don't get nWorkers times "last_element" token
	while(nLastGet < nWorkers){

		t_init = high_resolution_clock::now();							/* service time: init */

		el = outQueue.get();

		t_end = high_resolution_clock::now();							/* service time: end */
		elapse = t_end - t_init;
		serviceTime += elapse.count();									/* service time: update */


		if(!el->last_element) {

			if(!took){
				// Store the image variable the first time I get it 
				took = true;
				img = el->image;
			}
		}
		else {
			// Remember how many "last element" tokens I get
			nLastGet++;
		}

		delete el;
	}

	// Store image in the disk
	string out = "bw_"+filename;
	img.save(out.data());

	return;
}




/*
 * Stages 2-3 merged: compute the histogram and filter the image
 * Both are sequential
 *
 * Constructor's params:
 *	nWorkers:	the number of partitions the image has to be split into during the "filter computation" phase
 *	midQueue:	a pointer to the queue for incoming data
*/
void Comp(){

	high_resolution_clock::time_point 	t_init, t_end;
	duration<double, std::milli> elapse;


	inputData* el = inQueue.get();

	while(!el->last_element) {

		t_init = high_resolution_clock::now();								/* latH+F: init */


		vector<pair<int, int>> histogram(COLORMAXVAL);
		int height = el->image.height();

		buildHistogram(el->image, &histogram, 0, height);
		computePartialSums(&histogram);
		filterBitmap(&el->image, histogram, 0, height, el->sigma);

		outputData* out = new outputData(el->image, false);
		outQueue.push(out);
		delete el;


		t_end = high_resolution_clock::now();								/* latH+F: end */
		elapse = t_end - t_init;
		latencyComp += long(elapse.count());								/* latH+F: update */

		el = inQueue.get();
	}

	outputData* lastEl = new outputData(true);

	outQueue.push(lastEl);
	delete el;
	return;
}

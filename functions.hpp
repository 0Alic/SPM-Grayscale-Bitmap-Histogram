#include "CImg.h"
#include <vector>
#include <string>
#include <thread>
#include <iostream>
#include <mutex>

using namespace cimg_library;
using namespace std;

/* Constants */
const int white = 255;
const int black = 0;

#define COLORMAXVAL 256



			/**********************************************************************/
			/*							Helper Functions						  */
			/**********************************************************************/


// Return the min of two integers
int min(int p1, int p2){

	if(p1 < p2)	return p1;
	else return p2;
}



/* 
 * Set the the color at (x,y) to 'color' of image 'src'
 *
 * Params:
 *  src: 	greyscale image
 *  x: 		x-coordinate
 *  y:		y-coordinate
 *  color:	the target color
 *
*/
void setColor(CImg<unsigned char> 	*src, 
			  int 					x, 
			  int 					y, 
			  int 					color
			 ) {

	(*src)(x, y, 0, 0) = color;
	(*src)(x, y, 0, 1) = color;
	(*src)(x, y, 0, 2) = color;
}



/*
 * Fill the histogram with the partial sums
 * The sum of the pixels brighter than i is the sum of:
 *	pixels of bright i+i and pixels brighter than i+1
 *
 * Params:
 *	histogram: pointer to the histogram to modify
 *
*/
void computePartialSums(vector<pair<int, int>>* histogram) {

	for(int i=histogram->size()-2; i >= 0; --i)
		(*histogram)[i].second = (*histogram)[i+1].first + (*histogram)[i+1].second;
}


/*
 * Merge the partial histograms into one 
 *
 * Params:
 *	partials: series of partil histograms
 *
 * Ret:
 *	The sum up of the partial histograms
 *
*/
vector <pair<int, int>> sumUpVectors(vector< vector <pair<int, int>>> partials){

	int nPortions = partials.size(); 

	vector<pair<int, int>> histogram(COLORMAXVAL);

	// h[col] = partial sum of each partial[col]
	for(int col=0; col < COLORMAXVAL; col++) {
		for(int i=0; i < nPortions; i++) {
			histogram[col].first += partials[i][col].first;
		}
	}
	return histogram;
}


/* 
 * Build the histogram from the image
 * Sequential version 
 *
 * Params:
 *  src: 	grayscale image
 *  histo: 	pointer to a histogram
 *  start:	first x-coordinate
 *  end:	last x-coordinate
 *
*/
void buildHistogram(CImg<unsigned char> 	src, 
					vector<pair<int, int>>* histo, 
					int 					start,
					int 					end){

	for(int x=0; x<src.width(); ++x){
		for(int y=start; y<end; ++y){
			// Grayscale image has all channels of equal value
			int color = src(x, y, 0, 0);
			(*histo)[color].first++;				
		}
	}
}


/*
 * Filter (part of) the grayscale image building the (part of) the b/w image, according to a brightness parameter
 * Process the image along verctical strips
 *
 * This function is the target for a Data Parallel
 *
 * Params:
 *  src: 	pointer to grayscale image
 *  histo: 	src image's histogram
 *  start:	first x-coordinate
 *  end:	last x-coordinate
 *  sigma:	brightness parameter
 *
*/
void filterBitmap(CImg<unsigned char> 	 *src, 
				  vector<pair<int, int>> histo, 
				  int 					 start, 
				  int 					 end, 
				  double 				 sigma
				 ) {

	int width = src->width();
	int height = src->height();

	int area = width*height;

	for(int x=0; x<width; ++x){
		for(int y=start; y<end; ++y){

			int 	pixel_color = (*src)(x, y, 0, 0);
			double 	perc = histo[pixel_color].second / double(area);

			setColor(src, x, y, white*(perc <= sigma));
		}
	}
}

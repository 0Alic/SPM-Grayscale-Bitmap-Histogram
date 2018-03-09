#include "stages.hpp"
#include <chrono>
#include <iostream>
#include <string> // stoi()
#include <fstream> // file
#include "ff/pipeline.hpp"
#include "ff/farm.hpp"
#include "ff/map.hpp"
#include "ff_comp.hpp"

using namespace std;
using namespace std::chrono;
using namespace ff;


/*
 * 1) G: generator				: load images and generate streams;
 * 2) h: histogram builder		: compute histogram
 * 3) f: filter 				: compute b/w bitmap
 * 4) V: visualizer				: visualize b/w bitmap
 *
 * Two different skeleton composition are possible, according to certain parameters:
 *	Farm of sequentials
 *
 * 		G --> inQueue --> Farm(Comp( h, f )) --> outQueue --> V
 * 
 *
 *
 *	Farm of Pipeline of Parallel stages
 *
 * 		G --> inQueue --> Farm(Pipe( Map(h), Map(f) )) --> outQueue --> V
 *
*/


/*
	Implement BW Filter by using FastFlow framework

	./Filter -streamlen -nWorkersFarm -nWorkersStageH -nWorkersStageF 

	-streamlen=1			the stream length
	-nWorkersFarm=1, 		workers in the farm
	-nWorkersStageH=1, 		parallelize the farm's H stage
	-nWorkersStageF=1, 		parallelize the farm's F stage

*/
int main(int argc, char const *argv[]) {

	// Processing time variables
	high_resolution_clock::time_point 	t_init, t_end,
										t_init_setup, t_end_setup;

	duration<double, std::milli> elapse,
								 elapse_setup;


	// Prepare default configuration
	string path = "./../Data/";
	string filename;

	int stream_len = 	1;
	int nWorkersFarm = 	1;
	int nWorkersH = 	1;
	int nWorkersF = 	1;
	int totalWorkers =	0; // It depends on the skeleton composition, to be set afterwards
	double sigma = 		0.1;

	string definition = "hd";
	string skeleton = "sf";

	// Measurements
	double tc = 0;			// Completion Time
	double latency = 0;		// Stage's latency, total
	double ts = 0;			// Service Time


	// Read main parameters
	if(argc >= 2) stream_len 	= stoi(argv[1]);
	if(argc >= 3) nWorkersFarm 	= stoi(argv[2]);
	if(argc >= 4) nWorkersH 	= stoi(argv[3]);
	if(argc >= 5) nWorkersF 	= stoi(argv[4]);

	// Read configuration file inputs
	ifstream config("../input.config");
	string line;

	while(getline(config, line)) {

		if(line == "-skeleton"){
			getline(config, skeleton);
		}
		else if(line == "-definition") {
			getline(config, definition);
		}
		else if(line == "-sigma") {
			getline(config, line);
			sigma = stod(line);

			// Clamp sigma value in [0, 1]
			if(sigma > 1) sigma = 1;
			else if(sigma < 0) sigma = 0;
		}
	}
	config.close();

	filename = "test" + definition + ".bmp";

	Generator g(path+filename, sigma, stream_len);
	Visualizer v(filename);

	// Setup composition according to our input -skeleton
	if(skeleton == "sf") {
		/**********************************************************************/
		/*						Farm of sequentials							  */
		/**********************************************************************/

		// Save total workers involved
		totalWorkers = nWorkersFarm;

		t_init_setup = high_resolution_clock::now();			/* setup: init */


		ff_Farm<> farm( [nWorkersFarm] () {

			vector<unique_ptr<ff_node>> workers;
		
			for(int i=0; i<nWorkersFarm; i++){

				HistogramSeq* h = new HistogramSeq();
				FilterSeq* f = new FilterSeq();

				workers.push_back(make_unique<ff_comp>(h, f));
			}

			return workers;

		}() // Call immediately
		, g, v); // Set g,v as emitter/collector


		t_end_setup = high_resolution_clock::now();				/* setup: end */
		elapse_setup = t_end_setup - t_init_setup;				/* stp */

		t_init = high_resolution_clock::now();					/* run: init */

		farm.run_and_wait_end();

		t_end = high_resolution_clock::now();					/* run: end */
		elapse = t_end - t_init;								/* f */

		// Get latency spent by workers
		latency = long(latencyComp / stream_len);
	}


	else if(skeleton == "pf") {
		/**********************************************************************/
		/*				Farm of Pipeline of Parallel stages					  */
		/**********************************************************************/

		// Save total woekers involved
		totalWorkers = nWorkersFarm*(nWorkersH + nWorkersF);

		t_init_setup = high_resolution_clock::now();			/* setup: init */


		ff_Farm<> farm( [nWorkersFarm, nWorkersH, nWorkersF] () {

			vector<unique_ptr<ff_node>> workers;
		
			for(int i=0; i<nWorkersFarm; i++){

				workers.push_back(make_unique<ff_Pipe<>>(make_unique<HistogramPar>(nWorkersH),
														 make_unique<FilterPar>(nWorkersF) ));
			}

			return workers;

		}() // Call immediately
		, g, v); // Set g,v as emitter/collector


		t_end_setup = high_resolution_clock::now();				/* setup: end */
		elapse_setup = t_end_setup - t_init_setup;				/* stp */

		t_init = high_resolution_clock::now();					/* run: init */

		farm.run_and_wait_end();

		t_end = high_resolution_clock::now();					/* run: end */
		elapse = t_end - t_init;								/* run */

		// Get latency spent by workers
		latency = long((latencyH + latencyF) / stream_len);
	}

	else {
		cout << "This skeleton is not supported; Write \"pf\" or \"sf\"\n";
		exit(1);
	}


	// Print informations
	tc = elapse.count() + elapse_setup.count();
	ts = (stream_len == 1) ? latency : serviceTime/(stream_len-1);


	cout << 
	"n " << totalWorkers <<	
	" s " << stream_len <<
	" tpar " << elapse.count() << 
	" stp " << elapse_setup.count() <<
	" tc " << tc <<
	" lcy " << latency << 
	" ts " << ts << 
	"\n";

	return 0;
}
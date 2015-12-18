/*
    Copyright (C) 2012 Carl Hetherington <cth@carlh.net>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#include <iostream>
#include <cstdlib>
#include <unistd.h>
#include <stdio.h>
#include <xmmintrin.h>
#include <pmmintrin.h>
#include <getopt.h>
#include <sstream>
#include <list>
#include <stdexcept>
#include <signal.h>
#include <time.h>
#include "ladspa_plugin.h"
#include "lv2_plugin.h"
#include "log.h"
#include "input_buffers.h"
#include "tests.h"
#include "input_profile.h"

#define DEFAULT_N 1024

using namespace std;

static bool abort_on_sigfpe = false;
int sampling_rate;

void
fp_exception_handler (int)
{
	warning ("FP exception");
	if (abort_on_sigfpe) {
		abort ();
	}

	/* return code 2: SIGFPE was raised (probably due to a denormal) */
	exit (2);
}

double
run_tests (list<Test*> const & tests, bool evil, Plugin* p, int N)
{
    clock_t start, end;
    start = clock();

	for (list<Test*>::const_iterator i = tests.begin(); i != tests.end(); ++i) {
		if (evil || !(*i)->evil ()) {
			log ((*i)->name ());
			(*i)->run (p, N);
		}
	}

    end = clock();
    return ((double) (end - start)) / CLOCKS_PER_SEC;
}

static void
syntax (char* name)
{
	cerr << name << ": usage: " << name << " [-e] [-d] [-a] [-s|--ladspa] [-i,--index <n>] [-l,--lv2] [-g|--profile <input-profile] -p|--plugin <plugin.{so,ttl}>\n"
	     << "\t-e run particularly evil tests\n"
	     << "\t-d set CPU to raise SIGFPE on encountering a denormal, and catch it\n"
	     << "\t-a abort on SIGFPE; otherwise return with exit code 2\n"
	     << "\t-s|--ladspa plugin is LADSPA (specify the .so)\n"
	     << "\t-i|--index index of plugin in LADSPA .so (defaults to 0)\n"
	     << "\t-l|--lv2 plugin is LV2 (specify the .ttl, must be on LV2_PATH)\n"
	     << "\t-g|--profile <input-profile> input settings to use\n"
	     << "\t-p|--plugin <plugin.{so,ttl}> plugin to torture\n"
	     << "\t-n|--buffer-size buffer size (default " << DEFAULT_N << ")\n";
	exit (EXIT_FAILURE);
}
	

int
main (int argc, char* argv[])
{
	signal (SIGFPE, fp_exception_handler);
	sampling_rate = 44100;

	/* Make a list of tests */
	list<Test*> tests;
	tests.push_back (new Silence);
	tests.push_back (new Impulse);
	tests.push_back (new Pulse);
	tests.push_back (new Sine);
	tests.push_back (new ArdourDCBias);
	tests.push_back (new FltMin);
	tests.push_back (new Denormals);

	/* Parse the command line */

	string profile_file;
	string plugin_file;
	bool evil = false;
	bool detect_denormals = false;
	int ladspa_index = 0;
	int N = 1024;

	enum Type {
		LADSPA,
		LV2
	};

	Type type = LADSPA;
	
	if (argc == 1) {
		syntax (argv[0]);
	}

	while (1) {
		
		static struct option long_options[] = {
			{ "help", no_argument, 0, 'h' },
			{ "evil", no_argument, 0, 'e' },
			{ "denormals", no_argument, 0, 'd' },
			{ "abort", no_argument, 0, 'a' },
			{ "ladspa", no_argument, 0, 's' },
			{ "index", required_argument, 0, 'i'},
			{ "lv2", no_argument, 0, 'l' },
			{ "profile", required_argument, 0, 'g'},
			{ "plugin", required_argument, 0, 'p'},
			{ "buffer-size", required_argument, 0, 'n'},
			{ 0, 0, 0, 0 }
		};

		int i;
		int c = getopt_long (argc, argv, "hedasi:lg:p:n:", long_options, &i);
		if (c == -1) {
			break;
		}

		switch (c) {
		case 'h':
			syntax (argv[0]);
			break;
		case 'e':
			evil = true;
			break;
		case 'd':
			detect_denormals = true;
			break;
		case 'a':
			abort_on_sigfpe = true;
			break;
		case 's':
			type = LADSPA;
			break;
		case 'i':
			ladspa_index = atoi (optarg);
			break;
		case 'l':
			type = LV2;
			break;
		case 'g':
			profile_file = optarg;
			break;
		case 'p':
			plugin_file = optarg;
			break;
		case 'n':
			N = atoi (optarg);
            N = N<1 ? DEFAULT_N : N;
			break;
		}
	}
			

	if (detect_denormals) {
		log ("Turning on denormal detection.");
		int mxcsr = _mm_getcsr ();
		mxcsr &= ~(_MM_FLUSH_ZERO_ON | 0x8000);
		mxcsr &= ~_MM_MASK_DENORM;
		_mm_setcsr (mxcsr);
	}
	_MM_SET_FLUSH_ZERO_MODE( _MM_FLUSH_ZERO_ON );
	_MM_SET_DENORMALS_ZERO_MODE( _MM_DENORMALS_ZERO_ON );

	Plugin* plugin = 0;
	InputProfile* profile = 0;

	try {
		switch (type) {
		case LADSPA:
			plugin = new LadspaPlugin (plugin_file, ladspa_index);
			break;
		case LV2:
			plugin = new LV2Plugin (plugin_file);
			break;
		}
	
		if (!profile_file.empty ()) {
			profile = new InputProfile (profile_file);
		}
		
	} catch (runtime_error& e) {

		cerr << argv[0] << ": " << e.what() << "\n";
		exit (EXIT_FAILURE);

	}

	{
		stringstream s;
		s << "Running `" << plugin->name() << "' (" << plugin_file << ")";
		if (type == LADSPA) {
			s << " index " << ladspa_index;
		}
        s << ", buffer size " << N;
		log (s.str ());
	}

	plugin->instantiate (sampling_rate);
	plugin->activate ();
	plugin->prepare (N);

	int const control_inputs = plugin->control_inputs ();
	log ("Inputs:");
	for (int i = 0; i < control_inputs; ++i) {
		stringstream s;
		s << "\t" << i << " " << plugin->control_input_name (i) << " => default " << plugin->get_control_input (i);
		log (s.str ());
	}

	stringstream s;
	if (profile) {
		
		profile->begin_iteration ();
		
		while (1) {
			profile->setup (plugin);
			s << run_tests (tests, evil, plugin, N);
			if (!profile->step()) {
				break;
			}
		}
		
	} else {
		s << run_tests (tests, evil, plugin, N);
	
	}
    s << " seconds";
	log(s.str());
	
	plugin->deactivate ();
	delete plugin;

	return 0;
}


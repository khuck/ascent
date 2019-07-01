/*

*/

#include <vector>
#include <string>
#include <string.h>
#include <sstream>
#include <ostream>
#include <vector>
#include <algorithm>
#include "conduit.hpp"
#include "ascent.hpp"
#if USE_MPI
#include "mpi.h"
#endif
#include "perfstubs_api/Timer.h"

using namespace conduit;
using namespace ascent;

char * fix_timer_name(const char * timer_name) {
    // remove source information
    std::string tmp(timer_name);
    std::string delimiter(" [{");
    std::string token = tmp.substr(0, tmp.find(delimiter));
    // trim trailing whitespace - MPI events
    token.erase(token.find_last_not_of(" \t\n\r\f\v") + 1);
    // replace any other spaces with underscores
    std::transform(token.begin(), token.end(), token.begin(), [](char ch) {
        return ch == ' ' ? '_' : ch;
    });
    return strdup(token.c_str());
}

/******************************************/

int ascent_performance(int current_time, int current_cycle)
{
   int numRanks = 1;
   int myRank = 0;

#if USE_MPI
   MPI_Comm_size(MPI_COMM_WORLD, &numRanks) ;
   MPI_Comm_rank(MPI_COMM_WORLD, &myRank) ;
#endif

   //
   // setup Ascent In-situ rendering.
   //
    Ascent ascent;
    Node ascent_opts;

#if USE_MPI
    ascent_opts["mpi_comm"] = MPI_Comm_c2f(MPI_COMM_WORLD);
#endif

    ascent.open(ascent_opts);

   conduit::Node scenes;
   scenes["s2/plots/p1/type"]  = "pseudocolor";
   scenes["s2/plots/p1/field"] = "0_MPI_Wait()_Inclusive_TIME";
   //double vec3[3];
   //vec3[0] = -0.6; vec3[1] = -0.6; vec3[2] = -0.8;
   //scenes["s2/renders/r1/camera/position"].set_float64_ptr(vec3,3);

   conduit::Node actions;
   conduit::Node &add_plots = actions.append();
   add_plots["action"] = "add_scenes";
   add_plots["scenes"] = scenes;
   conduit::Node &execute = actions.append();
   execute["action"] = "execute";

   conduit::Node &reset_action = actions.append();
   reset_action["action"] = "reset";

/* TAU ascent nodes */
   std::vector<double> x ;  /* coordinates */
   std::vector<double> y ;  /* coordinates */
   std::vector<double> z ;  /* coordinates */
   x.resize(numRanks);
   y.resize(numRanks);
   z.resize(numRanks);
   for (int i = 0 ; i < numRanks ; i++) {
     double tmp = ((double)(i)) / ((double)(numRanks));
     x[i] = tmp;
     y[i] = tmp;
     z[i] = tmp;
   }

   conduit::Node tau_node;
   tau_node["state/time"].set_external(&current_time);
   tau_node["state/cycle"].set_external(&current_cycle);
   tau_node["state/domain_id"] = myRank;
   tau_node["state/info"] = "In Situ Pseudocolor rendering of Pressure from <br> LULESH Shock-Hydro Proxy Simulation";
   tau_node["coordsets/coords/type"] = "explicit";
   tau_node["coordsets/coords/values/x"].set(x);
   tau_node["coordsets/coords/values/y"].set(y);
   tau_node["coordsets/coords/values/z"].set(z);

   tau_node["topologies/mesh/type"] = "structured";
   tau_node["topologies/mesh/coordset"] = "coords";

   tau_node["topologies/mesh/elements/dims/i"] = 1;
   tau_node["topologies/mesh/elements/dims/j"] = 1;
   tau_node["topologies/mesh/elements/dims/k"] = 1;

   // tau_node["fields/MPI_Wait()/association"] = "element";
   // tau_node["fields/MPI_Wait()/topology"]    = "mesh";
   // tau_node["fields/MPI_Wait()/values"].set(myRank+1);
   perftool_timer_data_t timer_data;
   external::profiler::Timer::GetTimerData(&timer_data);
   int index = 0;
    for (int i = 0; i < timer_data.num_timers; i++)
    {
        for (int k = 0; k < timer_data.num_threads; k++)
        {
            for (int j = 0; j < timer_data.num_metrics; j++)
            {
                std::stringstream ss;
                ss << "fields/" << k << "_";
                ss << fix_timer_name(timer_data.timer_names[i]);
                ss << "_" << timer_data.metric_names[j];
                std::string assoc(ss.str());
                std::string topo(ss.str());
                std::string val(ss.str());
                assoc.append("/association");
                topo.append("/topology");
                val.append("/values");
                tau_node[assoc] = "element";
                tau_node[topo] = "mesh";
                tau_node[val].set(timer_data.values[index]);
                index = index + 1;
            }
        }
    }
    external::profiler::Timer::FreeTimerData(&timer_data);

   perftool_counter_data_t counter_data;
   external::profiler::Timer::GetCounterData(&counter_data);
   index = 0;
   std::vector<std::string> metrics;
   metrics.resize(5);
   metrics.push_back("num_samples");
   metrics.push_back("value_total");
   metrics.push_back("value_min");
   metrics.push_back("value_max");
   metrics.push_back("value_sumsqr");
    for (int i = 0; i < counter_data.num_counters; i++)
    {
        for (int k = 0; k < counter_data.num_threads; k++)
        {
            for (size_t j = 0; j < metrics.size(); j++)
            {
                std::stringstream ss;
                ss << "fields/" << k << "_";
                ss << fix_timer_name(counter_data.counter_names[i]);
                ss << "_" << metrics[j];
                std::string assoc(ss.str());
                std::string topo(ss.str());
                std::string val(ss.str());
                assoc.append("/association");
                topo.append("/topology");
                val.append("/values");
                std::cout << val << std::endl;
                tau_node[assoc] = "element";
                tau_node[topo] = "mesh";
                switch(j) {
                    case 0:
                        tau_node[val].set(counter_data.num_samples[index]);
                        break;
                    case 1:
                        tau_node[val].set(counter_data.value_total[index]);
                        break;
                    case 2:
                        tau_node[val].set(counter_data.value_min[index]);
                        break;
                    case 3:
                        tau_node[val].set(counter_data.value_max[index]);
                        break;
                    case 4:
                    default:
                        tau_node[val].set(counter_data.value_sumsqr[index]);
                        break;
                }
                index = index + 1;
            }
        }
    }
    external::profiler::Timer::FreeCounterData(&counter_data);

   ascent.publish(tau_node);
   ascent.execute(actions);
   ascent.close();

   return 0 ;
}

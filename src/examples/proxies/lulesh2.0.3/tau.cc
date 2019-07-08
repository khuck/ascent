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

int ascent_performance(conduit::Node &node, int length, int current_time, int current_cycle)
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
    ascent_opts["actions_file"] = "disabled.json";

    ascent.open(ascent_opts);

   conduit::Node scenes;
   scenes["s2/plots/p2/type"]  = "pseudocolor";
   scenes["s2/plots/p2/field"] = "0_LULESH_MAIN_LOOP_Inclusive_TIME";
   scenes["s2/image_prefix"] = "LULESH_MAIN_LOOP_%04d";
   //double vec3[3];
   //vec3[0] = -1.0; vec3[1] = -1.0; vec3[2] = -1.0;
   //scenes["s2/renders/r1/camera/position"].set_float64_ptr(vec3,3);
   double vec3[3];
   vec3[0] = 0.5; vec3[1] = 0.5; vec3[2] = 0.5;
   scenes["s2/renders/r1/camera/look_at"].set_float64_ptr(vec3,3);
   vec3[0] = -1.0; vec3[1] = -0.75; vec3[2] = -1.25;
   scenes["s2/renders/r1/camera/position"].set_float64_ptr(vec3,3);
   //scenes["s2/renders/r1/camera/azimuth"] = 10.0;
   //scenes["s2/renders/r1/camera/elevation"] = -10.0;

   conduit::Node actions;
   conduit::Node &add_plots = actions.append();
   add_plots["action"] = "add_scenes";
   add_plots["scenes"] = scenes;
   conduit::Node &execute = actions.append();
   execute["action"] = "execute";

   conduit::Node &reset_action = actions.append();
   reset_action["action"] = "reset";

#if 0
/* TAU ascent nodes */
   std::vector<double> x ;  /* coordinates */
   std::vector<double> y ;  /* coordinates */
   std::vector<double> z ;  /* coordinates */
   x.resize(numRanks);
   y.resize(numRanks);
   z.resize(numRanks);
   for (int i = 0 ; i < numRanks ; i++) {
     double tmp = ((double)(i)) / ((double)(numRanks-1));
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
#endif

   perftool_timer_data_t timer_data;
   external::profiler::Timer::GetTimerData(&timer_data);
   int index = 0;
   int nElems = length * length * length;
    for (int i = 0; i < timer_data.num_timers; i++)
    {
        for (int k = 0; k < timer_data.num_threads; k++)
        //for (int k = 0; k < 1; k++)
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
#if 0
                tau_node[assoc] = "element";
                tau_node[topo] = "mesh";
                tau_node[val].set(timer_data.values[index]);
#else
                node[assoc] = "element";
                node[topo] = "mesh";
                std::vector<double> vec(nElems, timer_data.values[index]);
                node[val].set(vec);
#endif
                index = index + 1;
            }
        }
    }
    external::profiler::Timer::FreeTimerData(&timer_data);

#if 0
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

    Node verify_info;
    if(!conduit::blueprint::mesh::verify(tau_node,verify_info))
    {
        // verify failed, print error message
        ASCENT_INFO("Error: Mesh Blueprint Verify Failed!");
        // show details of what went awry
        verify_info.print();
    }

   ascent.publish(tau_node);
#else
   ascent.publish(node);
#endif
   ascent.execute(actions);
   ascent.close();

   return 0 ;
}

/*

*/

#include <vector>
#include "conduit.hpp"
#include "ascent.hpp"
#if USE_MPI
#include "mpi.h"
#endif

using namespace conduit;
using namespace ascent;

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
   scenes["s2/plots/p1/field"] = "LagrangeLeapFrog";
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

   tau_node["fields/LagrangeLeapFrog/association"] = "element";
   tau_node["fields/LagrangeLeapFrog/topology"]    = "mesh";
   tau_node["fields/LagrangeLeapFrog/values"].set(myRank+1);

   ascent.publish(tau_node);
   ascent.execute(actions);
   ascent.close();

   return 0 ;
}

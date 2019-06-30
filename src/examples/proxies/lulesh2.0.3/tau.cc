/*

*/

#include "conduit.hpp"
#include "ascent.hpp"

using namespace conduit;
using namespace ascent;

/******************************************/

int ascent_performance(void)
{
   Int_t numRanks = 1;
   Int_t myRank = 0;

#if USE_MPI
   MPI_Init(&argc, &argv) ;
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
   scenes["s1/plots/p1/type"]  = "pseudocolor";
   scenes["s1/plots/p1/field"] = "LagrangeLeapFrog";
   //double vec3[3];
   //vec3[0] = -0.6; vec3[1] = -0.6; vec3[2] = -0.8;
   //scenes["s1/renders/r1/camera/position"].set_float64_ptr(vec3,3);

   conduit::Node actions;
   conduit::Node &add_plots = actions.append();
   add_plots["action"] = "add_scenes";
   add_plots["scenes"] = scenes;
   conduit::Node &execute = actions.append();
   execute["action"] = "execute";

   conduit::Node &reset_action = actions.append();
   reset_action["action"] = "reset";

/* TAU ascent nodes */
   conduit::Node tau_node;
   tau_node["state/time"].set_external(&m_time);
   tau_node["state/cycle"].set_external(&m_cycle);
   tau_node["state/domain_id"] = myRank;
   tau_node["state/info"] = "In Situ Pseudocolor rendering of Pressure from <br> LULESH Shock-Hydro Proxy Simulation";
   tau_node["coordsets/coords/type"] = "explicit";
   tau_node["coordsets/coords/values/x"].set(numRanks);
   tau_node["coordsets/coords/values/y"].set(numRanks);
   tau_node["coordsets/coords/values/z"].set(numRanks);

   tau_node["topologies/mesh/type"] = "structured";
   tau_node["topologies/mesh/coordset"] = "coords";

   tau_node["topologies/mesh/elements/dims/i"] = myRank;
   tau_node["topologies/mesh/elements/dims/j"] = myRank;
   tau_node["topologies/mesh/elements/dims/k"] = myRank;

   tau_node["fields/LagrangeLeapFrog/association"] = "element";
   tau_node["fields/LagrangeLeapFrog/topology"]    = "mesh";
   tau_node["fields/LagrangeLeapFrog/values"].set(myRank);

   ascent.publish(locDom->visitNode());
   ascent.execute(actions);
   ascent.close();

   return 0 ;
}

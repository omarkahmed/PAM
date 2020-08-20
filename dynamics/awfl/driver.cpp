
#include "const.h"
#include "Spatial_euler3d_cons_expl_cart_fv_Agrid.h"
#include "Temporal_ader.h"

// Define the Spatial operator based on constants from the Temporal operator
typedef Spatial_euler3d_cons_expl_cart_fv_Agrid<nTimeDerivs,timeAvg,nAder> Spatial;
// Define the Temporal operator based on the Spatial operator
typedef Temporal_ader<Spatial> Temporal;

int main(int argc, char** argv) {
  yakl::init();
  {

    if (argc <= 1) { endrun("ERROR: Must pass the input YAML filename as a parameter"); }
    std::string inFile(argv[1]);
    YAML::Node config = YAML::LoadFile(inFile);
    if ( !config            ) { endrun("ERROR: Invalid YAML input file"); }
    if ( !config["simTime"] ) { endrun("ERROR: no simTime entry"); }
    if ( !config["outFreq"] ) { endrun("ERROR: no outFreq entry"); }
    real simTime = config["simTime"].as<real>();
    real outFreq = config["outFreq"].as<real>();
    int numOut = 0;

    Temporal model;

    model.init(inFile);

    Spatial::StateArr state = model.spaceOp.createStateArr();

    model.spaceOp.initState(state);

    real etime = 0;

    model.spaceOp.output( state , etime );
    
    while (etime < simTime) {
      real dt = model.spaceOp.computeTimeStep(0.8,state);
      if (etime + dt > simTime) { dt = simTime - etime; }
      model.timeStep( state , dt );
      etime += dt;
      if (etime / outFreq >= numOut+1) {
        std::cout << "Etime , dt: " << etime << " , " << dt << "\n";
        model.spaceOp.output( state , etime );
        numOut++;
      }
    }

    std::cout << "Elapsed Time: " << etime << "\n";

    model.finalize(state);

  }
  yakl::finalize();
}




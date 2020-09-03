
#pragma once

#include "const.h"
#include "DataManager.h"

int  constexpr nTimeDerivs = 1;
bool constexpr timeAvg     = true;
int  constexpr nAder       = ngll;

template <class Spatial> class Temporal_ader {
public:
  static_assert(nTimeDerivs <= ngll , "ERROR: nTimeDerivs must be <= ngll.");

  typedef typename Spatial::StateTendArr  StateTendArr;
  typedef typename Spatial::StateArr      StateArr;
  typedef typename Spatial::TracerTendArr TracerTendArr;
  typedef typename Spatial::TracerArr     TracerArr;
  typedef typename Spatial::Location      Location;

  StateArr      stateArr;
  TracerArr     tracerArr;
  StateTendArr  stateTendArr;
  TracerTendArr tracerTendArr;

  Spatial spaceOp;
  
  void init(std::string inFile, int num_tracers, DataManager &dm) {
    spaceOp.init(inFile, num_tracers, dm);
    stateArr      = spaceOp.createStateArr();
    tracerArr     = spaceOp.createTracerArr();
    stateTendArr  = spaceOp.createStateTendArr ();
    tracerTendArr = spaceOp.createTracerTendArr();
  }


  template <class PHYS>
  void timeStep( DataManager &dm , PHYS const &physics , real dt ) {
    spaceOp.construct_state_and_tracers( dm , stateArr , tracerArr );

    // Loop over different items in the spatial splitting
    for (int spl = 0 ; spl < spaceOp.numSplit() ; spl++) {
      real dtloc = dt;
      spaceOp.computeTendencies( stateArr , stateTendArr , tracerArr , tracerTendArr , physics , dtloc , spl );

      {
        auto &stateTendArr  = this->stateTendArr ;
        auto applySingleTendency = YAKL_LAMBDA (Location const &loc) {
          real &state      = spaceOp.getState     (stateArr     ,loc  ,spl);
          real &stateTend  = spaceOp.getStateTend (stateTendArr ,loc,0,spl);
          state  += dtloc * stateTend;
        };
        spaceOp.applyStateTendencies( applySingleTendency , spl );
      }

      {
        auto &tracerTendArr  = this->tracerTendArr ;
        auto applySingleTendency = YAKL_LAMBDA (Location const &loc) {
          real &tracer     = spaceOp.getTracer    (tracerArr    ,loc  ,spl);
          real &tracerTend = spaceOp.getTracerTend(tracerTendArr,loc,0,spl);
          tracer += dtloc * tracerTend;
        };
        spaceOp.applyTracerTendencies( applySingleTendency , spl );
      }
    }

    spaceOp.write_state_and_tracers( dm , stateArr , tracerArr );
  }


  void finalize(DataManager &dm) { }


  const char * getTemporalName() const { return "ADER-DT"; }

};


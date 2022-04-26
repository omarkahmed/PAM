
#include "pam_coupler.h"
#include "Dycore.h"
#include "Microphysics.h"
#include "SGS.h"
#include "DataManager.h"
#include "mpi.h"
#include "output.h"

int main(int argc, char** argv) {
  MPI_Init( &argc , &argv );
  yakl::init();
  {
    
    int ierr = MPI_Init( &argc , &argv );
    int myrank;
    bool masterproc;
    ierr = MPI_Comm_rank(MPI_COMM_WORLD,&myrank);
    // Determine if I'm the master process
    masterproc = myrank == 0;
    
    using yakl::intrinsics::abs;
    using yakl::intrinsics::maxval;
    yakl::timer_start("main");

    pam::PamCoupler coupler;

    if (argc <= 1) { endrun("ERROR: Must pass the input YAML filename as a parameter"); }
    std::string inFile(argv[1]);
    YAML::Node config = YAML::LoadFile(inFile);
    if ( !config            ) { endrun("ERROR: Invalid YAML input file"); }

    real        simTime      = config["simTime" ].as<real>(0.0_fp);
    real        simSteps     = config["simSteps"].as<int>(0);
    int         crm_nx       = config["crm_nx"  ].as<int>();
    int         crm_ny       = config["crm_ny"  ].as<int>();
    int         crm_nz       = config["crm_nz"  ].as<int>(0);
    int         nens         = config["nens"    ].as<int>();
    real        xlen         = config["xlen"    ].as<real>(-1.0_fp);
    real        ylen         = config["ylen"    ].as<real>(-1.0_fp);
    real        zlen         = config["ylen"    ].as<real>(-1.0_fp);
    real        dtphys_in    = config["dtphys"  ].as<real>();
    std::string vcoords_file = config["vcoords" ].as<std::string>("");
    bool        use_coupler_hydrostasis = config["use_coupler_hydrostasis"].as<bool>(false);
    auto out_freq                = config["out_freq"   ].as<real>();
    auto out_prefix              = config["out_prefix" ].as<std::string>();

    // Read vertical coordinates
    real1d zint_in;
//THIS IS BROKEN FOR PARALLEL IO CASE
if (crm_nz == 0)
{
    yakl::SimpleNetCDF nc;
    nc.open(vcoords_file);
    crm_nz = nc.getDimSize("num_interfaces") - 1;
    zint_in = real1d("zint_in",crm_nz+1);
    nc.read(zint_in,"vertical_interfaces");
    nc.close();
}

    // Create the dycore and the microphysics
    Dycore       dycore;
    Microphysics micro;
    SGS          sgs;
    
    //set xlen, ylen, zlen based on init cond if needed
    if (xlen < 0 or ylen < 0 or zlen < 0)
    {dycore.set_domain_sizes(config["initData"].as<std::string>(), xlen, ylen, zlen);}
    //std::cout << xlen << " " << ylen << "\n";

    if (not crm_nz == 0) //We are using a uniform vertical grid with crm_nz levels; in this case zlen must be set
    {
      zint_in = real1d("zint_in",crm_nz+1);
      real dz = zlen/crm_nz;
      for (int i=0;i<crm_nz+1;i++)
      {zint_in(i) = i*dz;}
    }
    
    // Use microphysics gas constants values in the coupler
    coupler.set_phys_constants( micro.R_d , micro.R_v , micro.cp_d , micro.cp_v , micro.grav , micro.p0 );

    // Allocate coupler state
    coupler.allocate_coupler_state( crm_nz , crm_ny , crm_nx , nens );
    
    // Set the horizontal domain lengths and the vertical grid in the coupler
    coupler.set_grid( xlen , ylen , zint_in );

    // This is for the dycore to pull out to determine how to do idealized test cases
    coupler.set_option<std::string>( "standalone_input_file" , inFile );

    micro .init( coupler );
    sgs   .init( coupler );
    dycore.init( coupler ); // Dycore should initialize its own state here

    #ifdef PAM_STANDALONE
    if (masterproc) {
      std::cout << "Dycore: " << dycore.dycore_name() << std::endl;
      std::cout << "Micro : " << micro .micro_name () << std::endl;
      std::cout << "SGS   : " << sgs   .sgs_name   () << std::endl;
      std::cout << "\n";
    }
    #endif

    // Now that we have an initial state, define hydrostasis for each ensemble member
    if (use_coupler_hydrostasis) coupler.update_hydrostasis( );

    real etime = 0;
    if (simTime == 0.0) {  simTime = simSteps * dtphys_in; }
    
    int  num_out = 0;
    // Output the initial state
    if (out_freq >= 0. ) output( coupler , out_prefix , etime );

    real dtphys = dtphys_in;
    while (etime < simTime) {
      if (dtphys_in == 0.) { dtphys = dycore.compute_time_step(coupler); }
      if (etime + dtphys > simTime) { dtphys = simTime - etime; }

      yakl::timer_start("sgs");
      sgs.timeStep( coupler , dtphys );
      yakl::timer_stop("sgs");

      yakl::timer_start("micro");
      micro.timeStep( coupler , dtphys );
      yakl::timer_stop("micro");

      yakl::timer_start("dycore");
      dycore.timeStep( coupler , dtphys );
      yakl::timer_stop("dycore");

      etime += dtphys;

      auto &dm = coupler.get_data_manager_readonly();
      real maxw = maxval(abs(dm.get_collapsed<real const>("wvel")));
      std::cout << "Etime , dtphys, maxw: " << etime  << " , " 
                                            << dtphys << " , "
                                            << std::setw(10) << maxw << "\n";
        if (out_freq >= 0. && etime / out_freq >= num_out+1) {
          yakl::timer_start("output");
          output( coupler , out_prefix , etime );
          yakl::timer_stop("output");
          num_out++;
        }

    }

    if (masterproc) {std::cout << "Elapsed Time: " << etime << "\n";}

    dycore.finalize( coupler );

    yakl::timer_stop("main");
  }
  
  yakl::finalize();

  int ierr = MPI_Finalize();

  MPI_Finalize();
}

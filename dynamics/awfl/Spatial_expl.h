
#pragma once

#include "awfl_const.h"
#include "TransformMatrices.h"
#include "TransformMatrices_variable.h"
#include "WenoLimiter.h"
#include "idealized_profiles.h"
#include "MultipleFields.h"
#include "DataManager.h"
#include "pam_coupler.h"

using pam::PamCoupler;


template <int nTimeDerivs, bool timeAvg, int nAder>
class Spatial_operator {
public:

  static_assert(nTimeDerivs == 1 , "ERROR: This Spatial class isn't setup to use nTimeDerivs > 1");

  int static constexpr hs = (ord-1)/2;
  int static constexpr num_state = 5;
  int static constexpr max_tracers = 50;

  real Rd   ;
  real cp   ;
  real gamma;
  real p0   ;
  real C0   ;
  real Rv   ;
  real grav ;

  int idWV;  // Tracer index for water vapor (set in add_tracer by testing the tracer name against "water_vapor"

  real hydrostasis_parameters_sum;

  typedef real5d StateArr;  // Array of state variables (rho, rho*u, rho*v, rho*w, and rho*theta)
  typedef real5d TracerArr; // Array of tracers (total tracer mass)

  typedef real5d StateTendArr;   // State tendencies
  typedef real5d TracerTendArr;  // Tracer tendencies

  // Hydrostatically balanced values for density, potential temperature, and pressure (cell-averages)
  real2d hyDensCells;
  real2d hyPressureCells;
  real2d hyThetaCells;
  real2d hyDensThetaCells;

  // Hydrostatically balanced values for density, potential temperature, and pressure (GLL points)
  real3d hyDensGLL;
  real3d hyPressureGLL;
  real3d hyThetaGLL;
  real3d hyDensThetaGLL;

  // Matrices to transform DOFs from one form to another
  SArray<real,2,ord,ngll> coefs_to_gll;
  SArray<real,2,ord,ngll> coefs_to_deriv_gll;
  SArray<real,2,ord,ngll> sten_to_gll;
  SArray<real,2,ord,ord > sten_to_coefs;
  SArray<real,2,ord,ngll> sten_to_deriv_gll;
  // WENO reconstruction matrices
  SArray<real,3,hs+1,hs+1,hs+1> weno_recon_lower;
  SArray<real,1,hs+2> idl;   // Ideal weights for WENO
  real sigma;                // WENO sigma parameter (handicap high-order TV estimate)
  // For ADER spatial derivative computation (ngll points -> coefs -> deriv -> ngll points)
  SArray<real,2,ngll,ngll> derivMatrix;
  // For quadrature
  SArray<real,1,ord> gllWts_ord;
  SArray<real,1,ord> gllPts_ord;
  SArray<real,1,ngll> gllWts_ngll;
  SArray<real,1,ngll> gllPts_ngll;

  real2d vert_interface;
  real2d vert_interface_ghost;
  real3d vert_locs_normalized;
  real2d dz;
  real2d dz_ghost;
  real4d vert_sten_to_gll;
  real4d vert_sten_to_coefs;
  real5d vert_weno_recon_lower;

  // For indexing into the state and state tendency arrays
  int static constexpr idR = 0;  // density perturbation
  int static constexpr idU = 1;  // u
  int static constexpr idV = 2;  // v
  int static constexpr idW = 3;  // w
  int static constexpr idT = 4;  // potential temperature perturbation

  // The two boundary condition options for each direction
  int static constexpr BC_PERIODIC = 0;
  int static constexpr BC_WALL     = 1;

  // Options for initializing the data
  int static constexpr DATA_SPEC_EXTERNAL      = 0;
  int static constexpr DATA_SPEC_THERMAL       = 1;
  int static constexpr DATA_SPEC_SUPERCELL     = 2;

  bool sim2d;  // Whether we're simulating in 2-D

  // Grid spacing in each dimension
  real dx;
  real dy;

  // Initial time step (used throughout the simulation)
  real dtInit;

  // Which direction we're passing through for a given time step (x,y,z)  or (z,y,x)
  // For Strang splitting
  bool dimSwitch;

  int num_tracers;  // Number of tracers
  std::vector<std::string> tracer_name;      // Name of each tracer
  std::vector<std::string> tracer_desc;      // Description of each tracer
  bool1d                   tracer_pos;       // Whether each tracer is positive-definite
  bool1d                   tracer_adds_mass; // Whether each tracer adds mass (otherwise it's passive)

  //////////////////////////////////////
  // Values read from input file
  //////////////////////////////////////
  // Number of ensembles to simulate at the same time
  int         nens;
  // Number of cells in each direction
  int         nx;
  int         ny;
  int         nz;
  // Length of the domain in each direction (m)
  real        xlen;
  real        ylen;
  real1d      zbot;
  real1d      ztop;
  // Boundary condition in each direction
  int         bc_x;
  int         bc_y;
  int         bc_z;
  // Whether to use WENO for scalars and also for winds
  bool        weno_scalars;
  bool        weno_winds;
  // Name of the output file
  std::string out_prefix;
  // How to initialize the data
  int         data_spec;
  // Whether to balance initial density to avoid acoustics at the start
  bool balance_initial_density;


  // When this class is created, initialize num_tracers to zero
  Spatial_operator() {
    num_tracers = 0;
    idWV = -1;
  }



  // Make sure it's odd-order-accurate
  static_assert(ord%2 == 1,"ERROR: ord must be an odd integer");



  void convert_dynamics_to_coupler_state( PamCoupler &coupler , realConst5d state , realConst5d tracers ) const {
    real4d dm_dens_dry = coupler.dm.get<real,4>( "density_dry"      );
    real4d dm_uvel     = coupler.dm.get<real,4>( "uvel"             );
    real4d dm_vvel     = coupler.dm.get<real,4>( "vvel"             );
    real4d dm_wvel     = coupler.dm.get<real,4>( "wvel"             );
    real4d dm_temp     = coupler.dm.get<real,4>( "temp"             );

    YAKL_SCOPE( hyDensCells      , this->hyDensCells      );
    YAKL_SCOPE( hyDensThetaCells , this->hyDensThetaCells );
    YAKL_SCOPE( C0               , this->C0               );
    YAKL_SCOPE( gamma            , this->gamma            );
    YAKL_SCOPE( num_tracers      , this->num_tracers      );
    YAKL_SCOPE( Rd               , this->Rd               );
    YAKL_SCOPE( Rv               , this->Rv               );
    YAKL_SCOPE( tracer_adds_mass , this->tracer_adds_mass );
    YAKL_SCOPE( idWV , this->idWV );

    MultipleFields<max_tracers,real4d> dm_tracers;
    for (int tr = 0; tr < num_tracers; tr++) {
      auto trac = coupler.dm.get<real,4>( tracer_name[tr] );
      dm_tracers.add_field( trac );
    }

    parallel_for( "Spatial.h d2c" , SimpleBounds<4>(nz,ny,nx,nens) , YAKL_LAMBDA (int k, int j, int i, int iens) {
      real dens  = state(idR,hs+k,hs+j,hs+i,iens) + hyDensCells(k,iens);
      real uvel  = state(idU,hs+k,hs+j,hs+i,iens) / dens;
      real vvel  = state(idV,hs+k,hs+j,hs+i,iens) / dens;
      real wvel  = state(idW,hs+k,hs+j,hs+i,iens) / dens;
      real theta = ( state(idT,hs+k,hs+j,hs+i,iens) + hyDensThetaCells(k,iens) ) / dens;
      real pressure = C0 * pow( dens*theta , gamma );
      real dens_vap = tracers(idWV,hs+k,hs+j,hs+i,iens);
      real dens_dry = dens;
      for (int tr=0; tr < num_tracers; tr++) {
        if (tracer_adds_mass(tr)) dens_dry -= tracers(tr,hs+k,hs+j,hs+i,iens);
      }
      real temp = pressure / ( dens_dry * Rd + dens_vap * Rv );
      dm_dens_dry(k,j,i,iens) = dens_dry;
      dm_uvel    (k,j,i,iens) = uvel;
      dm_vvel    (k,j,i,iens) = vvel;
      dm_wvel    (k,j,i,iens) = wvel;
      dm_temp    (k,j,i,iens) = temp;
      for (int tr=0; tr < num_tracers; tr++) {
        dm_tracers(tr,k,j,i,iens) = tracers(tr,hs+k,hs+j,hs+i,iens);
      }
    });
  }



  YAKL_INLINE static real hydrostatic_dens_theta( realConst2d hy_params , real z , real zbot , real ztop ,
                                                  int iens , real C0 , real gamma ) {
    real p = pam::hydrostatic_pressure( hy_params , z , zbot , ztop , iens );
    // p = C0*(rho*theta)^gamma
    return pow(p/C0,1._fp/gamma);
  }



  YAKL_INLINE static real hydrostatic_theta( realConst2d hy_params , real z , real zbot , real ztop ,
                                             int iens , real C0 , real gamma , real grav ) {
    real rt =      hydrostatic_dens_theta( hy_params , z , zbot , ztop , iens , C0 , gamma        );
    real r  = pam::hydrostatic_density   ( hy_params , z , zbot , ztop , iens              , grav );
    return rt/r;
  }



  void convert_coupler_state_to_dynamics( PamCoupler const &coupler , real5d const &state , real5d const &tracers ) {
    auto hy_params = coupler.dm.get<real const,2>("hydrostasis_parameters");

    YAKL_SCOPE( hyPressureCells  , this->hyPressureCells  );
    YAKL_SCOPE( hyThetaCells     , this->hyThetaCells     );
    YAKL_SCOPE( hyDensCells      , this->hyDensCells      );
    YAKL_SCOPE( hyDensThetaCells , this->hyDensThetaCells );
    YAKL_SCOPE( hyPressureGLL    , this->hyPressureGLL    );
    YAKL_SCOPE( hyThetaGLL       , this->hyThetaGLL       );
    YAKL_SCOPE( hyDensGLL        , this->hyDensGLL        );
    YAKL_SCOPE( hyDensThetaGLL   , this->hyDensThetaGLL   );
    YAKL_SCOPE( C0               , this->C0               );
    YAKL_SCOPE( gamma            , this->gamma            );
    YAKL_SCOPE( num_tracers      , this->num_tracers      );
    YAKL_SCOPE( Rd               , this->Rd               );
    YAKL_SCOPE( Rv               , this->Rv               );
    YAKL_SCOPE( tracer_adds_mass , this->tracer_adds_mass );
    YAKL_SCOPE( zbot             , this->zbot             );
    YAKL_SCOPE( ztop             , this->ztop             );
    YAKL_SCOPE( gllPts_ngll      , this->gllPts_ngll      );
    YAKL_SCOPE( dz               , this->dz               );
    YAKL_SCOPE( vert_interface   , this->vert_interface   );
    YAKL_SCOPE( idWV             , this->idWV             );
    YAKL_SCOPE( grav             , this->grav             );

    auto dm_dens_dry = coupler.dm.get<real const,4>( "density_dry"      );
    auto dm_uvel     = coupler.dm.get<real const,4>( "uvel"             );
    auto dm_vvel     = coupler.dm.get<real const,4>( "vvel"             );
    auto dm_wvel     = coupler.dm.get<real const,4>( "wvel"             );
    auto dm_temp     = coupler.dm.get<real const,4>( "temp"             );

    MultipleFields<max_tracers,realConst4d> dm_tracers;
    for (int tr = 0; tr < num_tracers; tr++) {
      auto trac = coupler.dm.get<real const,4>( tracer_name[tr] );
      dm_tracers.add_field( trac );
    }

    // If hydrostasis in the coupler has changed, then we need to re-compute
    // hydrostatically balanced cells and GLL points for the dycore's time step
    real tmp = yakl::intrinsics::sum(hy_params);
    if (tmp != hydrostasis_parameters_sum) {
      SArray<real,1,9> gll_pts, gll_wts;
      TransformMatrices::get_gll_points ( gll_pts );
      TransformMatrices::get_gll_weights( gll_wts );

      // Compute new cell averages and GLL point values for hydrostasis
      hydrostasis_parameters_sum = tmp;
      parallel_for( "Spatial.h new hydrostasis" , SimpleBounds<2>(nz,nens) , YAKL_LAMBDA (int k, int iens) {
        real p  = 0;
        real r  = 0;
        real rt = 0;
        real t  = 0;
        for (int kk=0; kk < 9; kk++) {
          real zloc = vert_interface(k,iens) + 0.5_fp*dz(k,iens) + gll_pts(kk)*dz(k,iens);
          real wt = gll_wts(kk);
          p  += pam::hydrostatic_pressure  ( hy_params , zloc , zbot(iens) , ztop(iens) , iens                     ) * wt;
          r  += pam::hydrostatic_density   ( hy_params , zloc , zbot(iens) , ztop(iens) , iens              , grav ) * wt;
          rt +=      hydrostatic_dens_theta( hy_params , zloc , zbot(iens) , ztop(iens) , iens , C0 , gamma        ) * wt;
          t  +=      hydrostatic_theta     ( hy_params , zloc , zbot(iens) , ztop(iens) , iens , C0 , gamma , grav ) * wt;
        }
        hyPressureCells (k,iens) = p;
        hyDensCells     (k,iens) = r;
        hyDensThetaCells(k,iens) = rt;
        hyThetaCells    (k,iens) = t;

        for (int kk=0; kk < ngll; kk++) {
          real zloc = vert_interface(k,iens) + 0.5_fp*dz(k,iens) + gllPts_ngll(kk)*dz(k,iens);
          hyPressureGLL (k,kk,iens) = pam::hydrostatic_pressure  ( hy_params , zloc , zbot(iens) , ztop(iens) ,
                                                                   iens                     );
          hyDensGLL     (k,kk,iens) = pam::hydrostatic_density   ( hy_params , zloc , zbot(iens) , ztop(iens) ,
                                                                   iens              , grav );
          hyDensThetaGLL(k,kk,iens) =      hydrostatic_dens_theta( hy_params , zloc , zbot(iens) , ztop(iens) ,
                                                                   iens , C0 , gamma        );
          hyThetaGLL    (k,kk,iens) =      hydrostatic_theta     ( hy_params , zloc , zbot(iens) , ztop(iens) ,
                                                                   iens , C0 , gamma , grav );
        }
      });
    }

    parallel_for(  "Spatial.h c2d" , SimpleBounds<4>(nz,ny,nx,nens) , YAKL_LAMBDA (int k, int j, int i, int iens) {
      for (int tr=0; tr < num_tracers; tr++) {
        tracers(tr,hs+k,hs+j,hs+i,iens) = dm_tracers(tr,k,j,i,iens);
      }
      real dens_dry = dm_dens_dry    (k,j,i,iens);
      real uvel     = dm_uvel        (k,j,i,iens);
      real vvel     = dm_vvel        (k,j,i,iens);
      real wvel     = dm_wvel        (k,j,i,iens);
      real temp     = dm_temp        (k,j,i,iens);
      real dens_vap = tracers(idWV,hs+k,hs+j,hs+i,iens);
      real dens     = dens_dry;
      for (int tr=0; tr < num_tracers; tr++) {
        if (tracer_adds_mass(tr)) dens += tracers(tr,hs+k,hs+j,hs+i,iens);
      }
      real pressure = dens_dry * Rd * temp + dens_vap * Rv * temp;
      real theta    = pow( pressure / C0 , 1._fp / gamma ) / dens;
      state(idR,hs+k,hs+j,hs+i,iens) = dens - hyDensCells(k,iens);
      state(idU,hs+k,hs+j,hs+i,iens) = dens * uvel;
      state(idV,hs+k,hs+j,hs+i,iens) = dens * vvel;
      state(idW,hs+k,hs+j,hs+i,iens) = dens * wvel;
      state(idT,hs+k,hs+j,hs+i,iens) = dens * theta - hyDensThetaCells(k,iens);
    });
  }



  real5d createStateArr() const {
    return real5d("stateArr",num_state,nz+2*hs,ny+2*hs,nx+2*hs,nens);
  }



  real5d createTracerArr() const {
    return real5d("tracerArr",num_tracers,nz+2*hs,ny+2*hs,nx+2*hs,nens);
  }



  real5d createStateTendArr() const {
    return real5d("stateTendArr",num_state,nz,ny,nx,nens);
  }



  real5d createTracerTendArr() const {
    return real5d("tracerTendArr",num_tracers,nz,ny,nx,nens);
  }



  // Number of operator splittinng steps to use
  // Normally this would be 3, but the z-directly CFL is reduced because of how the fluxes are
  // handled in the presence of a solid wall boundary condition. I'm looking into how to fix this
  int numSplit() const {
    if (sim2d) {
      return 2;
    } else {
      return 3;
    }
  }



  // Given the model data and CFL value, compute the maximum stable time step
  real compute_time_step(PamCoupler const &coupler, real cfl_in = -1) {
    real cfl = cfl_in;
    if (cfl < 0) cfl = 0.8;

    // If we've already computed the time step, then don't compute it again
    if (dtInit <= 0) {
      YAKL_SCOPE( dx                   , this->dx                  );
      YAKL_SCOPE( dy                   , this->dy                  );
      YAKL_SCOPE( dz                   , this->dz                  );
      YAKL_SCOPE( hyDensCells          , this->hyDensCells         );
      YAKL_SCOPE( hyDensThetaCells     , this->hyDensThetaCells    );
      YAKL_SCOPE( gamma                , this->gamma               );
      YAKL_SCOPE( C0                   , this->C0                  );
      YAKL_SCOPE( Rd                   , this->Rd                  );
      YAKL_SCOPE( Rv                   , this->Rv                  );

      // Convert data from DataManager to state and tracers array for convenience
      auto dm_dens_dry = coupler.dm.get<real const,4>( "density_dry" );
      auto dm_uvel     = coupler.dm.get<real const,4>( "uvel"        );
      auto dm_vvel     = coupler.dm.get<real const,4>( "vvel"        );
      auto dm_wvel     = coupler.dm.get<real const,4>( "wvel"        );
      auto dm_temp     = coupler.dm.get<real const,4>( "temp"        );
      auto dm_dens_vap = coupler.dm.get<real const,4>( "water_vapor" );

      // Allocate a 3-D array for the max stable time steps (we'll use this for a reduction later)
      real4d dt3d("dt3d",nz,ny,nx,nens);

      // Loop through the cells, calculate the max stable time step for each cell
      parallel_for( "Spatial.h compute_time_step" , SimpleBounds<4>(nz,ny,nx,nens) ,
                    YAKL_LAMBDA (int k, int j, int i, int iens) {
        real rho_d = dm_dens_dry(k,j,i,iens);
        real u     = dm_uvel    (k,j,i,iens);
        real v     = dm_vvel    (k,j,i,iens);
        real w     = dm_wvel    (k,j,i,iens);
        real temp  = dm_temp    (k,j,i,iens);
        real rho_v = dm_dens_vap(k,j,i,iens);
        real p = Rd * rho_d * temp + Rv * rho_v * temp;
        // This neglects non-wv mass-adding tracers, but these are small, and their lack only increases cs
        // Thus the resulting time step is conservative w/r to these missing masses, which is more stable
        real cs = sqrt(gamma*p/(rho_v+rho_d));

        // Compute the maximum stable time step in each direction
        real udt = cfl * dx         / max( abs(u-cs) , abs(u+cs) );
        real vdt = cfl * dy         / max( abs(v-cs) , abs(v+cs) );
        real wdt = cfl * dz(k,iens) / max( abs(w-cs) , abs(w+cs) );

        // Compute the min of the max stable time steps
        dt3d(k,j,i,iens) = min( min(udt,vdt) , wdt );
      });
      // Store to dtInit so we don't have to compute this again
      dtInit = yakl::intrinsics::minval( dt3d );
    }

    return dtInit;
  }



  // Initialize crap needed by recon()
  void init(PamCoupler const &coupler) {
    using yakl::intrinsics::matmul_cr;

    this->nens = coupler.get_nens();
    this->nx = coupler.get_nx();
    this->ny = coupler.get_ny();
    this->xlen = coupler.get_xlen();
    this->ylen = coupler.get_ylen();
    this->num_tracers = coupler.get_num_tracers();

    this->hydrostasis_parameters_sum = 0;

    this->Rd    = coupler.R_d;
    this->cp    = coupler.cp_d;
    this->p0    = coupler.p0;
    this->Rv    = coupler.R_v;
    this->grav  = coupler.grav;
    this->gamma = cp / (cp-Rd);
    real kappa = Rd/cp;
    this->C0 = pow( Rd * pow( p0 , -kappa ) , gamma );

    // Allocate device arrays for whether tracers are positive-definite or add mass
    tracer_pos       = bool1d("tracer_pos"      ,num_tracers);
    tracer_adds_mass = bool1d("tracer_adds_mass",num_tracers);
    boolHost1d tracer_pos_host      ("tracer_pos_host"      ,num_tracers);
    boolHost1d tracer_adds_mass_host("tracer_adds_mass_host",num_tracers);

    std::vector<std::string> tracer_names_loc = coupler.get_tracer_names();
    bool water_vapor_found = false;
    for (int tr=0; tr < num_tracers; tr++) {
      bool found, positive, adds_mass;
      std::string desc;
      coupler.get_tracer_info( tracer_names_loc[tr] , desc , found , positive , adds_mass );
      tracer_name.push_back(tracer_names_loc[tr]);
      tracer_desc.push_back(desc);
      tracer_pos_host      (tr) = positive ;
      tracer_adds_mass_host(tr) = adds_mass;
      if (tracer_names_loc[tr] == std::string("water_vapor")) {
        idWV = tr;
        water_vapor_found = true;
      }
    }
    if (! water_vapor_found) endrun("ERROR: processed registered tracers, and water_vapor was not found");

    tracer_pos_host      .deep_copy_to(tracer_pos      );
    tracer_adds_mass_host.deep_copy_to(tracer_adds_mass);
    fence();

    // Inialize time step to zero, and dimensional splitting switch
    dtInit = 0;
    dimSwitch = true;

    std::string inFile = coupler.get_option<std::string>( "standalone_input_file" );

    // If inFile is empty, then we aren't reading in an input file
    if (inFile == std::string("")) {
      weno_scalars            = true;
      weno_winds              = true;
      data_spec               = DATA_SPEC_EXTERNAL;
      bc_x                    = BC_PERIODIC;
      bc_y                    = BC_PERIODIC;
      bc_z                    = BC_WALL;
      out_prefix              = "test";
      balance_initial_density = false;
    } else {
      // Read the YAML input file
      YAML::Node config = YAML::LoadFile(inFile);

      // Read whether we're doing WENO limiting on scalars and winds
      weno_scalars = config["weno_scalars"].as<bool>();
      weno_winds   = config["weno_winds"].as<bool>();

      // Read the data initialization option
      std::string dataStr = config["initData"].as<std::string>();
      if        (dataStr == "thermal") {
        data_spec = DATA_SPEC_THERMAL;
      } else if (dataStr == "supercell") {
        data_spec = DATA_SPEC_SUPERCELL;
      } else if (dataStr == "external") {
        data_spec = DATA_SPEC_EXTERNAL;
      } else {
        endrun("ERROR: Invalid data_spec");
      }

      // Read the x-direction boundary condition option
      std::string bc_x_str = config["bc_x"].as<std::string>();
      if        (bc_x_str == "periodic" ) {
        bc_x = BC_PERIODIC;
      } else if (bc_x_str == "wall"     ) {
        bc_x = BC_WALL;
      } else {
        endrun("Invalid bc_x");
      }

      // Read the y-direction boundary condition option
      std::string bc_y_str = config["bc_y"].as<std::string>();
      if        (bc_y_str == "periodic" ) {
        bc_y = BC_PERIODIC;
      } else if (bc_y_str == "wall"     ) {
        bc_y = BC_WALL;
      } else {
        endrun("Invalid bc_y");
      }

      // Read the z-direction boundary condition option
      std::string bc_z_str = config["bc_z"].as<std::string>();
      if        (bc_z_str == "periodic" ) {
        bc_z = BC_PERIODIC;
      } else if (bc_z_str == "wall"     ) {
        bc_z = BC_WALL;
      } else {
        endrun("Invalid bc_z");
      }

      // Read the output filename
      out_prefix = config["out_prefix"].as<std::string>();

      if (config["balance_initial_density"]) {
        balance_initial_density = config["balance_initial_density"].as<bool>();
      } else {
        balance_initial_density = false;
      }
    }

    // Determine whether this is a 2-D simulation
    sim2d = ny == 1;

    // Store vertical cell interface heights in the data manager
    auto zint = coupler.dm.get<real const,2>("vertical_interface_height");

    nz = coupler.get_nz();

    // Get the height of the z-dimension
    zbot = real1d("zbot",nens);
    ztop = real1d("ztop",nens);
    YAKL_SCOPE( zbot , this->zbot );
    YAKL_SCOPE( ztop , this->ztop );
    YAKL_SCOPE( nz   , this->nz   );
    parallel_for( "Spatial.h init 1" , nens , YAKL_LAMBDA (int iens) {
      zbot(iens) = zint(0 ,iens);
      ztop(iens) = zint(nz,iens);
    });

    vert_interface        = real2d("vert_interface"      ,nz+1          ,nens);
    vert_interface_ghost  = real2d("vert_interface_ghost",nz+2*hs+1     ,nens);
    vert_locs_normalized  = real3d("vert_locs_normalized",nz,ord+1      ,nens);
    dz                    = real2d("dz"                  ,nz            ,nens);
    dz_ghost              = real2d("dz_ghost"            ,nz+2*hs       ,nens);
    vert_sten_to_gll      = real4d("vert_sten_to_gll"     ,nz,ord,ngll,nens);
    vert_sten_to_coefs    = real4d("vert_sten_to_coefs"   ,nz,ord,ord ,nens);
    vert_weno_recon_lower = real5d("vert_weno_recon_lower",nz,hs+1,hs+1,hs+1,nens);

    YAKL_SCOPE( vert_interface        , this->vert_interface        );
    YAKL_SCOPE( vert_interface_ghost  , this->vert_interface_ghost  );
    YAKL_SCOPE( vert_locs_normalized  , this->vert_locs_normalized  );
    YAKL_SCOPE( dz                    , this->dz                    );
    YAKL_SCOPE( dz_ghost              , this->dz_ghost              );
    YAKL_SCOPE( vert_sten_to_gll      , this->vert_sten_to_gll      );
    YAKL_SCOPE( vert_sten_to_coefs    , this->vert_sten_to_coefs    );
    YAKL_SCOPE( vert_weno_recon_lower , this->vert_weno_recon_lower );

    zint.deep_copy_to(vert_interface);

    parallel_for( "Spatial.h init 1" , SimpleBounds<2>(nz,nens) , YAKL_LAMBDA (int k, int iens) {
      dz(k,iens) = vert_interface(k+1,iens) - vert_interface(k,iens);
    });

    parallel_for( "Spatial.h init 2" , SimpleBounds<2>(nz+2*hs,nens) , YAKL_LAMBDA (int k, int iens) {
      if (k >= hs && k < hs+nz) {
        dz_ghost(k,iens) = dz(k-hs,iens);
      } else if (k < hs) {
        dz_ghost(k,iens) = dz(0,iens);
      } else if (k >= hs+nz) {
        dz_ghost(k,iens) = dz(nz-1,iens);
      }
    });

    parallel_for( "Spatial.h init 3" , nens , YAKL_LAMBDA (int iens) {
      vert_interface_ghost(0,iens) = vert_interface(0,iens) - hs*dz(0,iens);
      for (int k=1; k < nz+2*hs+1; k++) {
        vert_interface_ghost(k,iens) = vert_interface_ghost(k-1,iens) + dz_ghost(k-1,iens);
      }
    });

    auto vint_host      = vert_interface_ghost .createHostCopy();
    auto vert_s2g_host  = vert_sten_to_gll     .createHostCopy();
    auto vert_s2c_host  = vert_sten_to_coefs   .createHostCopy();
    auto vert_weno_host = vert_weno_recon_lower.createHostCopy();
    auto vert_locs_host = vert_locs_normalized .createHostCopy();

    SArray<real,2,ord,ngll> c2g;
    TransformMatrices::coefs_to_gll_lower(c2g);

    for (int k=0; k < nz; k++) {
      for (int iens = 0; iens < nens; iens++) {
        // Store stencil locations
        SArray<double,1,ord+1> locs;
        for (int kk=0; kk < ord+1; kk++) {
          locs(kk) = vint_host(k+kk,iens);
        }

        // Normalize stencil locations
        double zmid = ( locs(hs+1) + locs(hs) ) / 2;
        double dzmid = locs(hs+1) - locs(hs);
        for (int kk=0; kk < ord+1; kk++) {
          locs(kk) = ( locs(kk) - zmid ) / dzmid;
          vert_locs_host(k,kk,iens) = locs(kk);
        }

        // Compute reconstruction matrices
        SArray<double,2,ord,ord> s2c_var_in;
        SArray<double,3,hs+1,hs+1,hs+1> weno_recon_lower_var;
        TransformMatrices_variable::sten_to_coefs_variable<ord>(locs,s2c_var_in);
        TransformMatrices_variable::weno_lower_sten_to_coefs<ord>(locs,weno_recon_lower_var);
        SArray<real,2,ord,ord> s2c_var;
        for (int jj=0; jj < ord; jj++) {
          for (int ii=0; ii < ord; ii++) {
            s2c_var(jj,ii) = s2c_var_in(jj,ii);
          }
        }
        auto s2g_var = matmul_cr( c2g , s2c_var );

        // Store reconstruction matrices
        for (int jj=0; jj < ord; jj++) {
          for (int ii=0; ii < ord; ii++) {
            vert_s2c_host(k,jj,ii,iens) = s2c_var(jj,ii);
          }
        }

        for (int jj=0; jj < ord; jj++) {
          for (int ii=0; ii < ngll; ii++) {
            vert_s2g_host(k,jj,ii,iens) = s2g_var(jj,ii);
          }
        }

        for (int kk=0; kk < hs+1; kk++) {
          for (int jj=0; jj < hs+1; jj++) {
            for (int ii=0; ii < hs+1; ii++) {
              vert_weno_host(k,kk,jj,ii,iens) = weno_recon_lower_var(kk,jj,ii);
            }
          }
        }

      }
    }

    vert_s2g_host .deep_copy_to(vert_sten_to_gll     );
    vert_s2c_host .deep_copy_to(vert_sten_to_coefs   );
    vert_weno_host.deep_copy_to(vert_weno_recon_lower);
    vert_locs_host.deep_copy_to(vert_locs_normalized );

    // Compute the grid spacing in each dimension
    dx = xlen/nx;
    dy = ylen/ny;

    // Store the WENO reconstruction matrices
    TransformMatrices::weno_lower_sten_to_coefs(this->weno_recon_lower);

    // Block exists to avoid name mangling stufff
    {
      SArray<real,2,ord,ord>  s2c;        // Converts ord stencil cell averages to ord coefficients
      SArray<real,2,ord,ngll> c2g_lower;  // Converts ord coefficients to ngll GLL points
      SArray<real,2,ord,ord>  c2d;        // Converts ord coefficients to order differentiated coefficients

      TransformMatrices::sten_to_coefs     (s2c      );
      TransformMatrices::coefs_to_gll_lower(c2g_lower);
      TransformMatrices::coefs_to_deriv    (c2d      );

      this->coefs_to_gll       = c2g_lower;
      this->coefs_to_deriv_gll = matmul_cr( c2g_lower , c2d );
      this->sten_to_coefs      = s2c;
      this->sten_to_gll        = matmul_cr( c2g_lower , s2c );
      this->sten_to_deriv_gll  = matmul_cr( c2g_lower , matmul_cr( c2d , s2c ) );
    }
    // Store ader derivMatrix
    {
      SArray<real,2,ngll,ngll> g2c;  // Converts ngll GLL points to ngll coefficients
      SArray<real,2,ngll,ngll> c2d;  // Converts ngll coefficients to ngll differentiated coefficients
      SArray<real,2,ngll,ngll> c2g;  // Converts ngll coefficients to ngll GLL points

      TransformMatrices::gll_to_coefs  (g2c);
      TransformMatrices::coefs_to_deriv(c2d);
      TransformMatrices::coefs_to_gll  (c2g);

      this->derivMatrix = matmul_cr( c2g , matmul_cr( c2d , g2c ) );
    }
    // Store quadrature weights using ord GLL points
    TransformMatrices::get_gll_points (this->gllPts_ord);
    TransformMatrices::get_gll_weights(this->gllWts_ord);
    // Store quadrature weights using ngll GLL points
    TransformMatrices::get_gll_points (this->gllPts_ngll);
    TransformMatrices::get_gll_weights(this->gllWts_ngll);

    // Store WENO ideal weights and sigma value
    weno::wenoSetIdealSigma<ord>(this->idl,this->sigma);

    // Allocate data
    hyDensCells      = real2d("hyDensCells       ",nz,nens);
    hyPressureCells  = real2d("hyPressureCells   ",nz,nens);
    hyThetaCells     = real2d("hyThetaCells      ",nz,nens);
    hyDensThetaCells = real2d("hyDensThetaCells  ",nz,nens);
    hyDensGLL        = real3d("hyDensGLL         ",nz,ngll,nens);
    hyPressureGLL    = real3d("hyPressureGLL     ",nz,ngll,nens);
    hyThetaGLL       = real3d("hyThetaGLL        ",nz,ngll,nens);
    hyDensThetaGLL   = real3d("hyDensThetaGLL    ",nz,ngll,nens);

    #ifdef PAM_STANDALONE
      std::cout << "nx: " << nx << "\n";
      std::cout << "ny: " << ny << "\n";
      std::cout << "nz: " << nz << "\n";
      std::cout << "xlen (m): " << xlen << "\n";
      std::cout << "ylen (m): " << ylen << "\n";
      std::cout << "zbot (m): " << zbot.createHostCopy()(0) << "\n";
      std::cout << "ztop (m): " << ztop.createHostCopy()(0) << "\n";
      std::cout << "Vertical interface heights: ";
      auto zint_host = zint.createHostCopy();
      for (int k=0; k < nz+1; k++) {
        std::cout << zint_host(k,0) << "  ";
      }
      std::cout << "\n\n";
    #endif
  }



  // Initialize the state
  void init_idealized_state_and_tracers( PamCoupler &coupler ) {
    YAKL_SCOPE( nx                       , this->nx                      );
    YAKL_SCOPE( ny                       , this->ny                      );
    YAKL_SCOPE( nz                       , this->nz                      );
    YAKL_SCOPE( dx                       , this->dx                      );
    YAKL_SCOPE( dy                       , this->dy                      );
    YAKL_SCOPE( dz                       , this->dz                      );
    YAKL_SCOPE( gllPts_ord               , this->gllPts_ord              );
    YAKL_SCOPE( gllWts_ord               , this->gllWts_ord              );
    YAKL_SCOPE( gllPts_ngll              , this->gllPts_ngll             );
    YAKL_SCOPE( gllWts_ngll              , this->gllWts_ngll             );
    YAKL_SCOPE( hyDensCells              , this->hyDensCells             );
    YAKL_SCOPE( hyThetaCells             , this->hyThetaCells            );
    YAKL_SCOPE( hyPressureCells          , this->hyPressureCells         );
    YAKL_SCOPE( hyDensThetaCells         , this->hyDensThetaCells        );
    YAKL_SCOPE( hyDensGLL                , this->hyDensGLL               );
    YAKL_SCOPE( hyThetaGLL               , this->hyThetaGLL              );
    YAKL_SCOPE( hyPressureGLL            , this->hyPressureGLL           );
    YAKL_SCOPE( hyDensThetaGLL           , this->hyDensThetaGLL          );
    YAKL_SCOPE( data_spec                , this->data_spec               );
    YAKL_SCOPE( sim2d                    , this->sim2d                   );
    YAKL_SCOPE( xlen                     , this->xlen                    );
    YAKL_SCOPE( ylen                     , this->ylen                    );
    YAKL_SCOPE( Rd                       , this->Rd                      );
    YAKL_SCOPE( Rv                       , this->Rv                      );
    YAKL_SCOPE( cp                       , this->cp                      );
    YAKL_SCOPE( gamma                    , this->gamma                   );
    YAKL_SCOPE( p0                       , this->p0                      );
    YAKL_SCOPE( C0                       , this->C0                      );
    YAKL_SCOPE( balance_initial_density  , this->balance_initial_density );
    YAKL_SCOPE( vert_interface           , this->vert_interface          );
    YAKL_SCOPE( num_tracers              , this->num_tracers             );
    YAKL_SCOPE( tracer_adds_mass         , this->tracer_adds_mass        );
    YAKL_SCOPE( idWV                     , this->idWV                    );
    YAKL_SCOPE( grav                     , this->grav                    );

    // If data's being specified by the driver externally, then there's nothing to do here
    if (data_spec == DATA_SPEC_EXTERNAL) return;

    real5d state   = createStateArr();
    real5d tracers = createTracerArr();

    // If the data_spec is thermal or ..., then initialize the domain with Exner pressure-based hydrostasis
    // This is mostly to make plotting potential temperature perturbation easier for publications
    if (data_spec == DATA_SPEC_THERMAL) {

      // Setup hydrostatic background state
      parallel_for( "Spatial.h init_state 1" , SimpleBounds<2>(nz,nens) , YAKL_LAMBDA (int k, int iens) {
        // Compute cell averages
        hyDensCells     (k,iens) = 0;
        hyPressureCells (k,iens) = 0;
        hyThetaCells    (k,iens) = 0;
        hyDensThetaCells(k,iens) = 0;
        for (int kk=0; kk<ord; kk++) {
          real zloc = vert_interface(k,iens) + 0.5_fp*dz(k,iens) + gllPts_ord(kk)*dz(k,iens);
          if        (data_spec == DATA_SPEC_THERMAL) {
            // Compute constant theta hydrostatic background state
            real th  = 300;
            real rh = profiles::initConstTheta_density (th,zloc,Rd,cp,gamma,p0,C0,grav);
            real ph = profiles::initConstTheta_pressure(th,zloc,Rd,cp,gamma,p0,C0,grav);
            real wt = gllWts_ord(kk);
            hyDensCells     (k,iens) += rh    * wt;
            hyThetaCells    (k,iens) += th    * wt;
            hyDensThetaCells(k,iens) += rh*th * wt;
            hyPressureCells (k,iens) += ph    * wt;
          }
        }
      });

      parallel_for( "Spatial.h init_state 2" , SimpleBounds<2>(nz,nens) , YAKL_LAMBDA (int k, int iens) {
        // Compute ngll GLL points
        for (int kk=0; kk<ngll; kk++) {
          real zloc = vert_interface(k,iens) + 0.5_fp*dz(k,iens) + gllPts_ngll(kk)*dz(k,iens);
          if        (data_spec == DATA_SPEC_THERMAL) {
            // Compute constant theta hydrostatic background state
            real th = 300;
            real rh = profiles::initConstTheta_density (th,zloc,Rd,cp,gamma,p0,C0,grav);
            real ph = profiles::initConstTheta_pressure(th,zloc,Rd,cp,gamma,p0,C0,grav);
            hyDensGLL     (k,kk,iens) = rh;
            hyThetaGLL    (k,kk,iens) = th;
            hyDensThetaGLL(k,kk,iens) = rh*th;
            hyPressureGLL (k,kk,iens) = ph;
          }
        }
      });

      // Compute the state
      parallel_for( "Spatial.h init_state 3" , SimpleBounds<4>(nz,ny,nx,nens) ,
                    YAKL_LAMBDA (int k, int j, int i, int iens) {
        state(idR,hs+k,hs+j,hs+i,iens) = 0;
        state(idU,hs+k,hs+j,hs+i,iens) = 0;
        state(idV,hs+k,hs+j,hs+i,iens) = 0;
        state(idW,hs+k,hs+j,hs+i,iens) = 0;
        state(idT,hs+k,hs+j,hs+i,iens) = 0;
        for (int kk=0; kk<ord; kk++) {
          for (int jj=0; jj<ord; jj++) {
            for (int ii=0; ii<ord; ii++) {
              real zloc = vert_interface(k,iens) + 0.5_fp*dz(k,iens) + gllPts_ord(kk)*dz(k,iens);
              real yloc;
              if (sim2d) {
                yloc = ylen/2;
              } else {
                yloc = (j+0.5_fp)*dy + gllPts_ord(jj)*dy;
              }
              real xloc = (i+0.5_fp)*dx + gllPts_ord(ii)*dx;
              real wt = gllWts_ord(kk) * gllWts_ord(jj) * gllWts_ord(ii);
              if        (data_spec == DATA_SPEC_THERMAL) {
                // Compute constant theta hydrostatic background state
                real th = 300;
                real rh = profiles::initConstTheta_density(th,zloc,Rd,cp,gamma,p0,C0,grav);
                real tp = profiles::ellipsoid_linear(xloc, yloc, zloc, xlen/2, ylen/2, 2000, 2000, 2000, 2000, 2 );
                real t = th + tp;
                real r = rh;
                // Line below balances initial density to remove the acoustic wave
                if (balance_initial_density) r = rh*th/t;

                state(idR,hs+k,hs+j,hs+i,iens) += (r - rh)*wt;
                state(idT,hs+k,hs+j,hs+i,iens) += (r*t - rh*th) * wt;
              }
            }
          }
        }
      });

      parallel_for( "Spatial.h init_tracers" , SimpleBounds<4>(nz,ny,nx,nens) ,
                    YAKL_LAMBDA (int k, int j, int i, int iens) {
        tracers(idWV,hs+k,hs+j,hs+i,iens) = 0;
        // Loop over quadrature points
        for (int kk=0; kk<ord; kk++) {
          for (int jj=0; jj<ord; jj++) {
            for (int ii=0; ii<ord; ii++) {
              // Get the location
              real zloc = vert_interface(k,iens) + 0.5_fp*dz(k,iens) + gllPts_ord(kk)*dz(k,iens);
              real yloc;
              if (sim2d) {
                yloc = ylen/2;
              } else {
                yloc = (j+0.5_fp)*dy + gllPts_ord(jj)*dy;
              }
              real xloc = (i+0.5_fp)*dx + gllPts_ord(ii)*dx;

              // Get dry constants

              // Compute constant theta hydrostatic background state
              real th = 300;
              real rh = profiles::initConstTheta_density(th,zloc,Rd,cp,gamma,p0,C0,grav);

              // Initialize tracer mass based on dry state
              // Vapor perturbation profile
              real pert  = profiles::ellipsoid_linear(xloc,yloc,zloc  ,  xlen/2,ylen/2,2000  ,  2000,2000,2000  ,  0.8);
              real press = C0*pow(rh*th,gamma);                       // Dry pressure
              real temp  = press / Rd / rh;                           // Temperator (same for dry and moist)
              real svp   = profiles::saturation_vapor_pressure(temp); // Self-explanatory
              real p_v   = pert*svp;                                  // Multiply profile by saturation vapor pressure
              real r_v   = p_v / (Rv*temp);                           // Compute vapor density

              real wt = gllWts_ord(kk) * gllWts_ord(jj) * gllWts_ord(ii);
              tracers(idWV,hs+k,hs+j,hs+i,iens) += r_v / (rh+r_v) * rh * wt;
              for (int tr=0; tr < num_tracers; tr++) {
                if (tr != idWV) tracers(tr,hs+k,hs+j,hs+i,iens) = 0;
              }
            }
          }
        }
      });

      parallel_for( "Spatial.h adjust_moisture" , SimpleBounds<4>(nz,ny,nx,nens) ,
                    YAKL_LAMBDA (int k, int j, int i, int iens) {
        // Add tracer density to dry density if it adds mass
        real rho_dry = state(idR,hs+k,hs+j,hs+i,iens) + hyDensCells(k,iens);
        state(idR,hs+k,hs+j,hs+i,iens) += tracers(idWV,hs+k,hs+j,hs+i,iens);
        real rho_moist = state(idR,hs+k,hs+j,hs+i,iens) + hyDensCells(k,iens);

        // Adjust momenta for moist density
        state(idU,hs+k,hs+j,hs+i,iens) = state(idU,hs+k,hs+j,hs+i,iens) / rho_dry * rho_moist;
        state(idV,hs+k,hs+j,hs+i,iens) = state(idV,hs+k,hs+j,hs+i,iens) / rho_dry * rho_moist;
        state(idW,hs+k,hs+j,hs+i,iens) = state(idW,hs+k,hs+j,hs+i,iens) / rho_dry * rho_moist;

        // Compute the dry temperature (same as the moist temperature)
        real rho_theta_dry = state(idT,hs+k,hs+j,hs+i,iens) + hyDensThetaCells(k,iens);
        real press = C0*pow(rho_theta_dry,gamma);  // Dry pressure
        real temp  = press / Rd / rho_dry;         // Temp (same dry or moist)

        // Compute moist theta
        real rho_v = tracers(idWV,hs+k,hs+j,hs+i,iens);
        real R_moist = Rd * (rho_dry / rho_moist) + Rv * (rho_v / rho_moist);
        real press_moist = rho_moist * R_moist * temp;
        real rho_theta_moist = pow( press_moist / C0 , 1._fp/gamma );

        // Compute moist rho*theta
        state(idT,hs+k,hs+j,hs+i,iens) = rho_theta_moist - hyDensThetaCells(k,iens);

        for (int tr = 0 ; tr < num_tracers ; tr++) {
          tracers(tr,hs+k,hs+j,hs+i,iens) = tracers(tr,hs+k,hs+j,hs+i,iens) / rho_dry * rho_moist;
        }

        if (balance_initial_density) {
          real rh  = hyDensCells     (k,iens);
          real rth = hyDensThetaCells(k,iens);
          real rt = state(idT,hs+k,hs+j,hs+i,iens) + rth;
          real r  = state(idR,hs+k,hs+j,hs+i,iens) + rh;
          real t  = rt / r;
          r = rth/t;
          state(idR,hs+k,hs+j,hs+i,iens) = r - rh;
          state(idU,hs+k,hs+j,hs+i,iens) = state(idU,hs+k,hs+j,hs+i,iens) / rho_moist * r;
          state(idV,hs+k,hs+j,hs+i,iens) = state(idV,hs+k,hs+j,hs+i,iens) / rho_moist * r;
          state(idW,hs+k,hs+j,hs+i,iens) = state(idW,hs+k,hs+j,hs+i,iens) / rho_moist * r;
          state(idT,hs+k,hs+j,hs+i,iens) = r*t - rth;
          for (int tr = 0 ; tr < num_tracers ; tr++) {
            tracers(tr,hs+k,hs+j,hs+i,iens) = tracers(tr,hs+k,hs+j,hs+i,iens) / rho_moist * r;
          }
        }
      });

    } // if (data_spec == DATA_SPEC_THERMAL)


    if (data_spec == DATA_SPEC_SUPERCELL) {

      // This uses a piecewise linear profile for Temperature
      real constexpr z_0    = 0;
      real constexpr z_trop = 12000;
      real constexpr T_0    = 300;
      real constexpr T_trop = 213;
      real constexpr T_top  = 213;
      real constexpr p_0    = 100000;

      real4d quad_temp     ("quad_temp"     ,nz,ngll-1,ord,nens);
      real3d hyDensVapGLL  ("hyDensVapGLL"  ,nz,ngll,nens);
      real2d hyDensVapCells("hyDensVapCells",nz,nens);
      real2d z             ("z"             ,nz,nens);
      real2d temp_hy       ("temp_hy"       ,nz,nens);
      real2d tdew_hy       ("tdew_hy"       ,nz,nens);

      YAKL_SCOPE( ztop , this->ztop );

      // Compute full density at ord GLL points for the space between each cell
      parallel_for( "Spatial.h init_state 4" , SimpleBounds<4>(nz,ngll-1,ord,nens) ,
                    YAKL_LAMBDA (int k, int kk, int kkk, int iens) {
        // Middle of this cell
        real cellmid   = vert_interface(k,iens) + 0.5_fp*dz(k,iens);
        // Bottom, top, and middle of the space between these two ngll GLL points
        real ngll_b    = cellmid + gllPts_ngll(kk  )*dz(k,iens);
        real ngll_t    = cellmid + gllPts_ngll(kk+1)*dz(k,iens);
        real ngll_m    = 0.5_fp * (ngll_b + ngll_t);
        // Compute grid spacing between these ngll GLL points
        real ngll_dz   = dz(k,iens) * ( gllPts_ngll(kk+1) - gllPts_ngll(kk) );
        // Compute the locate of this ord GLL point within the ngll GLL points
        real zloc      = ngll_m + ngll_dz * gllPts_ord(kkk);
        // Compute full density at this location
        real temp      = profiles::init_supercell_temperature (zloc, z_0, z_trop, ztop(iens), T_0, T_trop, T_top);
        real press_dry = profiles::init_supercell_pressure_dry(zloc, z_0, z_trop, ztop(iens), T_0, T_trop, T_top,
                                                               p_0, Rd, grav);
        real dens_dry  = press_dry / (Rd*temp);
        real qvs       = profiles::init_supercell_sat_mix_dry(press_dry, temp);
        real relhum    = profiles::init_supercell_relhum(zloc, z_0, z_trop);
        if (relhum * qvs > 0.014_fp) relhum = 0.014_fp / qvs;
        real qv        = min( 0.014_fp , qvs*relhum );
        quad_temp(k,kk,kkk,iens) = -(1+qv)*grav/(Rd+qv*Rv)/temp;
      });

      parallel_for( "Spatial.h init_state 5" , nens , YAKL_LAMBDA (int iens) {
        hyPressureGLL(0,0,iens) = p_0;
        for (int k=0; k < nz; k++) {
          for (int kk=0; kk < ngll-1; kk++) {
            real tot = 0;
            for (int kkk=0; kkk < ord; kkk++) {
              tot += quad_temp(k,kk,kkk,iens) * gllWts_ord(kkk);
            }
            tot *= dz(k,iens) * ( gllPts_ngll(kk+1) - gllPts_ngll(kk) );
            hyPressureGLL(k,kk+1,iens) = hyPressureGLL(k,kk,iens) * exp( tot );
            if (kk == ngll-2 && k < nz-1) {
              hyPressureGLL(k+1,0,iens) = hyPressureGLL(k,ngll-1,iens);
            }
          }
        }
      });

      parallel_for( "Spatial.h init_state 6" , SimpleBounds<3>(nz,ngll,nens) , YAKL_LAMBDA (int k, int kk, int iens) {
        real zloc = vert_interface(k,iens) + 0.5_fp*dz(k,iens) + gllPts_ngll(kk)*dz(k,iens);
        real temp       = profiles::init_supercell_temperature (zloc, z_0, z_trop, ztop(iens), T_0, T_trop, T_top);
        real press_tmp  = profiles::init_supercell_pressure_dry(zloc, z_0, z_trop, ztop(iens), T_0, T_trop, T_top,
                                                                p_0, Rd, grav);
        real qvs        = profiles::init_supercell_sat_mix_dry(press_tmp, temp);
        real relhum     = profiles::init_supercell_relhum(zloc, z_0, z_trop);
        if (relhum * qvs > 0.014_fp) relhum = 0.014_fp / qvs;
        real qv         = min( 0.014_fp , qvs*relhum );
        real press      = hyPressureGLL(k,kk,iens);
        real dens_dry   = press / (Rd+qv*Rv) / temp;
        real dens_vap   = qv * dens_dry;
        real press_vap  = dens_vap * Rv * temp;
        real press_dry  = dens_dry * Rd * temp;
        real dens       = dens_dry + dens_vap;
        real dens_theta = pow( press / C0 , 1._fp / gamma );
        real theta      = dens_theta / dens;
        hyDensGLL     (k,kk,iens) = dens;
        hyDensThetaGLL(k,kk,iens) = dens_theta;
        hyThetaGLL    (k,kk,iens) = theta;
        hyDensVapGLL  (k,kk,iens) = dens_vap;
      });

      parallel_for( "Spatial.h init_state 7" , SimpleBounds<2>(nz,nens) , YAKL_LAMBDA (int k, int iens) {
        real press_tot      = 0;
        real dens_tot       = 0;
        real dens_vap_tot   = 0;
        real theta_tot      = 0;
        real dens_theta_tot = 0;
        for (int kk=0; kk < ngll; kk++) {
          press_tot      += hyPressureGLL (k,kk,iens) * gllWts_ngll(kk);
          dens_tot       += hyDensGLL     (k,kk,iens) * gllWts_ngll(kk);
          dens_vap_tot   += hyDensVapGLL  (k,kk,iens) * gllWts_ngll(kk);
          dens_theta_tot += hyDensThetaGLL(k,kk,iens) * gllWts_ngll(kk);
          theta_tot      += hyThetaGLL    (k,kk,iens) * gllWts_ngll(kk);
        }
        real press      = press_tot;
        real dens       = dens_tot;
        real dens_vap   = dens_vap_tot;
        real dens_theta = dens_theta_tot;
        real theta      = theta_tot;
        real dens_dry   = dens - dens_vap;
        real R          = dens_dry / dens * Rd + dens_vap / dens * Rv;
        real temp       = press / (dens * R);
        real qv         = dens_vap / dens_dry;
        real zloc       = vert_interface(k,iens) + 0.5_fp*dz(k,iens);
        real press_tmp  = profiles::init_supercell_pressure_dry(zloc, z_0, z_trop, ztop(iens), T_0, T_trop, T_top,
                                                                p_0, Rd, grav);
        real qvs        = profiles::init_supercell_sat_mix_dry(press_tmp, temp);
        real relhum     = qv / qvs;
        real T          = temp - 273;
        real a          = 17.27;
        real b          = 237.7;
        real tdew       = b * ( a*T / (b + T) + log(relhum) ) / ( a - ( a*T / (b+T) + log(relhum) ) );
        // The next three are just to confirm the skew-T diagram looks OK
        z               (k,iens) = zloc;
        temp_hy         (k,iens) = temp;
        tdew_hy         (k,iens) = tdew;
        // These are used in the rest of the model
        hyPressureCells (k,iens) = press;
        hyDensCells     (k,iens) = dens;
        hyDensThetaCells(k,iens) = dens_theta;
        hyThetaCells    (k,iens) = theta;
        hyDensVapCells  (k,iens) = dens_vap;
      });

      // Dump out data to plot a skew-T log-P diagram
      yakl::SimpleNetCDF nc;
      nc.create("skew.nc");
      real1d data("data",nz);
      for (int iens=0; iens < nens; iens++) {
        parallel_for( "Spatial.h init_state 8" , nz , YAKL_LAMBDA (int k) { data(k) = z(k,iens); });
        nc.write(data.createHostCopy(),"z"          ,{"z"});

        parallel_for( "Spatial.h init_state 9" , nz , YAKL_LAMBDA (int k) { data(k) = hyPressureCells(k,iens); });
        nc.write(data.createHostCopy(),"pressure"   ,{"z"});

        parallel_for( "Spatial.h init_state 10" , nz , YAKL_LAMBDA (int k) { data(k) = temp_hy(k,iens); });
        nc.write(data.createHostCopy(),"temperature",{"z"});

        parallel_for( "Spatial.h init_state 11" , nz , YAKL_LAMBDA (int k) { data(k) = tdew_hy(k,iens); });
        nc.write(data.createHostCopy(),"dew_point"  ,{"z"});
      }
      nc.close();

      parallel_for( "Spatial.h init_state 12" , SimpleBounds<4>(nz,ny,nx,nens) ,
                    YAKL_LAMBDA (int k, int j, int i, int iens) {
        state  (idR ,hs+k,hs+j,hs+i,iens) = 0;
        state  (idU ,hs+k,hs+j,hs+i,iens) = 0;
        state  (idV ,hs+k,hs+j,hs+i,iens) = 0;
        state  (idW ,hs+k,hs+j,hs+i,iens) = 0;
        state  (idT ,hs+k,hs+j,hs+i,iens) = 0;
        tracers(idWV,hs+k,hs+j,hs+i,iens) = 0;
        for (int kk=0; kk < ngll; kk++) {
          for (int jj=0; jj < ngll; jj++) {
            for (int ii=0; ii < ngll; ii++) {
              real xloc = (i+0.5_fp)*dx                              + gllPts_ngll(ii)*dx;
              real yloc = (j+0.5_fp)*dy                              + gllPts_ngll(jj)*dy;
              real zloc = vert_interface(k,iens) + 0.5_fp*dz(k,iens) + gllPts_ngll(kk)*dz(k,iens);

              if (sim2d) yloc = ylen/2;

              real dens = hyDensGLL(k,kk,iens);

              real uvel;
              real constexpr zs = 5000;
              real constexpr us = 30;
              real constexpr uc = 15;
              if (zloc < zs) {
                uvel = us * (zloc / zs) - uc;
              } else {
                uvel = us - uc;
              }

              real vvel       = 0;
              real wvel       = 0;
              real theta      = hyThetaGLL    (k,kk,iens);
              real dens_vap   = hyDensVapGLL  (k,kk,iens);
              real dens_theta = hyDensThetaGLL(k,kk,iens);

              real x0 = xlen / 2;
              real y0 = ylen / 2;
              real z0 = 1500;
              real radx = 10000;
              real rady = 10000;
              real radz = 1500;
              real amp  = 3;

              real xn = (xloc - x0) / radx;
              real yn = (yloc - y0) / rady;
              real zn = (zloc - z0) / radz;

              real rad = sqrt( xn*xn + yn*yn + zn*zn );

              real theta_pert = 0;
              if (rad < 1) {
                theta_pert = amp * pow( cos(M_PI*rad/2) , 2._fp );
              }

              // dens_theta += dens * theta_pert;

              real factor = gllWts_ngll(ii) * gllWts_ngll(jj) * gllWts_ngll(kk);
              state  (idR ,hs+k,hs+j,hs+i,iens) += (dens - hyDensGLL(k,kk,iens))            * factor;
              state  (idU ,hs+k,hs+j,hs+i,iens) += dens * uvel                              * factor;
              state  (idV ,hs+k,hs+j,hs+i,iens) += dens * vvel                              * factor;
              state  (idW ,hs+k,hs+j,hs+i,iens) += dens * wvel                              * factor;
              state  (idT ,hs+k,hs+j,hs+i,iens) += (dens_theta - hyDensThetaGLL(k,kk,iens)) * factor;
              tracers(idWV,hs+k,hs+j,hs+i,iens) += dens_vap                                 * factor;
            }
          }
        }
      });

    } // if (data_spec == DATA_SPEC_SUPERCELL)

    convert_dynamics_to_coupler_state( coupler , state , tracers );
  }



  // Compute state and tendency time derivatives from the state
  void computeTendencies( real5d const &state   , real5d const &stateTend  ,
                          real5d const &tracers , real5d const &tracerTend ,
                          real &dt , int splitIndex ) const {
    if (sim2d) {
      if (dimSwitch) {
        if        (splitIndex == 0) {
          computeTendenciesX( state , stateTend , tracers , tracerTend , dt );
        } else if (splitIndex == 1) {
          computeTendenciesZ( state , stateTend , tracers , tracerTend , dt );
        }
      } else {
        if        (splitIndex == 0) {
          computeTendenciesZ( state , stateTend , tracers , tracerTend , dt );
        } else if (splitIndex == 1) {
          computeTendenciesX( state , stateTend , tracers , tracerTend , dt );
        }
      }
    } else {
      if (dimSwitch) {
        if        (splitIndex == 0) {
          computeTendenciesX( state , stateTend , tracers , tracerTend , dt );
        } else if (splitIndex == 1) {
          computeTendenciesY( state , stateTend , tracers , tracerTend , dt );
        } else if (splitIndex == 2) {
          computeTendenciesZ( state , stateTend , tracers , tracerTend , dt );
        }
      } else {
        if        (splitIndex == 0) {
          computeTendenciesZ( state , stateTend , tracers , tracerTend , dt );
        } else if (splitIndex == 1) {
          computeTendenciesY( state , stateTend , tracers , tracerTend , dt );
        } else if (splitIndex == 2) {
          computeTendenciesX( state , stateTend , tracers , tracerTend , dt );
        }
      }
    }
  } // computeTendencies



  void switch_directions() {
    dimSwitch = ! dimSwitch;
  }



  void computeTendenciesX( real5d const &state   , real5d const &stateTend  ,
                           real5d const &tracers , real5d const &tracerTend ,
                           real &dt ) const {
    YAKL_SCOPE( nx                      , this->nx                     );
    YAKL_SCOPE( weno_scalars            , this->weno_scalars           );
    YAKL_SCOPE( weno_winds              , this->weno_winds             );
    YAKL_SCOPE( c2g                     , this->coefs_to_gll           );
    YAKL_SCOPE( s2g                     , this->sten_to_gll            );
    YAKL_SCOPE( s2c                     , this->sten_to_coefs          );
    YAKL_SCOPE( weno_recon_lower        , this->weno_recon_lower       );
    YAKL_SCOPE( idl                     , this->idl                    );
    YAKL_SCOPE( sigma                   , this->sigma                  );
    YAKL_SCOPE( hyDensCells             , this->hyDensCells            );
    YAKL_SCOPE( hyDensThetaCells        , this->hyDensThetaCells       );
    YAKL_SCOPE( sim2d                   , this->sim2d                  );
    YAKL_SCOPE( derivMatrix             , this->derivMatrix            );
    YAKL_SCOPE( dx                      , this->dx                     );
    YAKL_SCOPE( tracer_pos              , this->tracer_pos             );
    YAKL_SCOPE( num_tracers             , this->num_tracers            );
    YAKL_SCOPE( bc_x                    , this->bc_x                   );
    YAKL_SCOPE( gamma                   , this->gamma                  );
    YAKL_SCOPE( C0                      , this->C0                     );

    // Populate the halos
    if        (bc_x == BC_PERIODIC) {
      parallel_for( "Spatial.h X BCs periodic" , SimpleBounds<4>(nz,ny,hs,nens) ,
                    YAKL_LAMBDA(int k, int j, int ii, int iens) {
        for (int l=0; l < num_state; l++) {
          state  (l,hs+k,hs+j,      ii,iens) = state  (l,hs+k,hs+j,nx+ii,iens);
          state  (l,hs+k,hs+j,hs+nx+ii,iens) = state  (l,hs+k,hs+j,hs+ii,iens);
        }
        for (int l=0; l < num_tracers; l++) {
          tracers(l,hs+k,hs+j,      ii,iens) = tracers(l,hs+k,hs+j,nx+ii,iens);
          tracers(l,hs+k,hs+j,hs+nx+ii,iens) = tracers(l,hs+k,hs+j,hs+ii,iens);
        }
      });
    } else if (bc_x == BC_WALL) {
      parallel_for( "Spatial.h X BCs wall" , SimpleBounds<4>(nz,ny,hs,nens) ,
                    YAKL_LAMBDA(int k, int j, int ii, int iens) {
        for (int l=0; l < num_state; l++) {
          if (l == idU) {
            state(l,hs+k,hs+j,      ii,iens) = 0;
            state(l,hs+k,hs+j,hs+nx+ii,iens) = 0;
          } else {
            state(l,hs+k,hs+j,      ii,iens) = state(l,hs+k,hs+j,hs     ,iens);
            state(l,hs+k,hs+j,hs+nx+ii,iens) = state(l,hs+k,hs+j,hs+nx-1,iens);
          }
        }
        for (int l=0; l < num_tracers; l++) {
          tracers(l,hs+k,hs+j,      ii,iens) = tracers(l,hs+k,hs+j,hs     ,iens);
          tracers(l,hs+k,hs+j,hs+nx+ii,iens) = tracers(l,hs+k,hs+j,hs+nx-1,iens);
        }
      });
    }

    real6d state_gll  ("state_gll"  ,num_state  ,ngll,nz,ny,nx,nens);
    real6d tracers_gll("tracers_gll",num_tracers,ngll,nz,ny,nx,nens);

    // Loop through all cells, reconstruct in x-direction, compute centered tendencies, store cell-edge state estimates
    parallel_for( "Spatial.h X recon" , SimpleBounds<4>(nz,ny,nx,nens) , YAKL_LAMBDA (int k, int j, int i, int iens) {
      SArray<real,1,ord>  stencil;
      SArray<real,1,ngll> gll;

      // Density
      for (int ii=0; ii < ord; ii++) { stencil(ii) = state(idR,hs+k,hs+j,i+ii,iens); }
      reconstruct_gll_values( stencil , gll , c2g , s2g , s2c , weno_recon_lower , idl , sigma , weno_scalars );
      // Add hydrostasis back on
      for (int ii=0; ii < ngll; ii++) { state_gll(idR,ii,k,j,i,iens) = gll(ii) + hyDensCells(k,iens); }

      // u
      for (int ii=0; ii < ord; ii++) { stencil(ii) = state(idU,hs+k,hs+j,i+ii,iens); }
      reconstruct_gll_values( stencil , gll , c2g , s2g , s2c , weno_recon_lower , idl , sigma , weno_winds );
      for (int ii=0; ii < ngll; ii++) { state_gll(idU,ii,k,j,i,iens) = gll(ii); }

      // v
      for (int ii=0; ii < ord; ii++) { stencil(ii) = state(idV,hs+k,hs+j,i+ii,iens); }
      reconstruct_gll_values( stencil , gll , c2g , s2g , s2c , weno_recon_lower , idl , sigma , weno_winds );
      for (int ii=0; ii < ngll; ii++) { state_gll(idV,ii,k,j,i,iens) = gll(ii); }

      // w
      for (int ii=0; ii < ord; ii++) { stencil(ii) = state(idW,hs+k,hs+j,i+ii,iens); }
      reconstruct_gll_values( stencil , gll , c2g , s2g , s2c , weno_recon_lower , idl , sigma , weno_winds );
      for (int ii=0; ii < ngll; ii++) { state_gll(idW,ii,k,j,i,iens) = gll(ii); }

      // theta
      for (int ii=0; ii < ord; ii++) { stencil(ii) = state(idT,hs+k,hs+j,i+ii,iens); }
      reconstruct_gll_values( stencil , gll , c2g , s2g , s2c , weno_recon_lower , idl , sigma , weno_scalars );
      // Add hydrostasis back on
      for (int ii=0; ii < ngll; ii++) { state_gll(idT,ii,k,j,i,iens) = gll(ii) + hyDensThetaCells(k,iens); }

      for (int tr=0; tr < num_tracers; tr++) {
        for (int ii=0; ii < ord; ii++) { stencil(ii) = tracers(tr,hs+k,hs+j,i+ii,iens); }
        reconstruct_gll_values( stencil , gll , c2g , s2g , s2c , weno_recon_lower , idl , sigma , weno_scalars );
        if (tracer_pos(tr)) {
          for (int ii=0; ii < ngll; ii++) { gll(ii) = max( 0._fp , gll(ii) ); }
        }
        for (int ii=0; ii < ngll; ii++) { tracers_gll(tr,ii,k,j,i,iens) = gll(ii); }
      }
    });

    // Stores two estimates of the state and tracer values at each cell interface
    real6d stateLimits ("stateLimits" ,num_state  ,2,nz,ny,nx+1,nens);
    real6d tracerLimits("tracerLimits",num_tracers,2,nz,ny,nx+1,nens);

    // Loop through all cells, reconstruct in x-direction, compute centered tendencies, store cell-edge state estimates
    parallel_for( "Spatial.h X ader" , SimpleBounds<4>(nz,ny,nx,nens) , YAKL_LAMBDA (int k, int j, int i, int iens) {
      // We need density and momentum to evolve the tracers with ADER
      SArray<real,2,nAder,ngll> r_DTs , ru_DTs;

      { // BEGIN: Reconstruct, time-average, and store the state and fluxes
        SArray<real,2,nAder,ngll> rv_DTs , rw_DTs , rt_DTs;
        ///////////////////////////////////////////////////////////////
        // Compute other values needed for centered tendencies and DTs
        ///////////////////////////////////////////////////////////////
        SArray<real,2,nAder,ngll> ruu_DTs , ruv_DTs , ruw_DTs , rut_DTs , rt_gamma_DTs;
        for (int ii=0; ii < ngll; ii++) {
          real r = state_gll(idR,ii,k,j,i,iens);
          real u = state_gll(idU,ii,k,j,i,iens) / r;
          real v = state_gll(idV,ii,k,j,i,iens) / r;
          real w = state_gll(idW,ii,k,j,i,iens) / r;
          real t = state_gll(idT,ii,k,j,i,iens) / r;
          r_DTs       (0,ii) = r;
          ru_DTs      (0,ii) = r*u;
          rv_DTs      (0,ii) = r*v;
          rw_DTs      (0,ii) = r*w;
          rt_DTs      (0,ii) = r*t;
          ruu_DTs     (0,ii) = r*u*u;
          ruv_DTs     (0,ii) = r*u*v;
          ruw_DTs     (0,ii) = r*u*w;
          rut_DTs     (0,ii) = r*u*t;
          rt_gamma_DTs(0,ii) = pow(r*t,gamma);
        }

        //////////////////////////////////////////
        // Compute time derivatives if necessary
        //////////////////////////////////////////
        if (nAder > 1) {
          diffTransformEulerConsX( r_DTs , ru_DTs , rv_DTs , rw_DTs , rt_DTs , ruu_DTs , ruv_DTs , ruw_DTs ,
                                   rut_DTs , rt_gamma_DTs , derivMatrix , C0 , gamma , dx );
        }

        //////////////////////////////////////////
        // Time average if necessary
        //////////////////////////////////////////
        // density and momentum can't be overwritten because they will be used for tracers
        SArray<real,1,ngll> r_tavg, ru_tavg;
        if (timeAvg) {
          compute_timeAvg( r_DTs  , r_tavg  , dt );
          compute_timeAvg( ru_DTs , ru_tavg , dt );
          compute_timeAvg( rv_DTs           , dt );
          compute_timeAvg( rw_DTs           , dt );
          compute_timeAvg( rt_DTs           , dt );
        } else {
          for (int ii=0; ii < ngll; ii++) {
            r_tavg (ii) = r_DTs (0,ii);
            ru_tavg(ii) = ru_DTs(0,ii);
          }
        }

        //////////////////////////////////////////
        // Store cell edge estimates of the state
        //////////////////////////////////////////
        // Left interface
        stateLimits(idR,1,k,j,i  ,iens) = r_tavg  (0     );
        stateLimits(idU,1,k,j,i  ,iens) = ru_tavg (0     );
        stateLimits(idV,1,k,j,i  ,iens) = rv_DTs(0,0     );
        stateLimits(idW,1,k,j,i  ,iens) = rw_DTs(0,0     );
        stateLimits(idT,1,k,j,i  ,iens) = rt_DTs(0,0     );
        // Right interface
        stateLimits(idR,0,k,j,i+1,iens) = r_tavg  (ngll-1);
        stateLimits(idU,0,k,j,i+1,iens) = ru_tavg (ngll-1);
        stateLimits(idV,0,k,j,i+1,iens) = rv_DTs(0,ngll-1);
        stateLimits(idW,0,k,j,i+1,iens) = rw_DTs(0,ngll-1);
        stateLimits(idT,0,k,j,i+1,iens) = rt_DTs(0,ngll-1);
      } // END: Reconstruct, time-average, and store the state and fluxes

      // r_DTs and ru_DTs still exist and are computed
      { // BEGIN: Reconstruct, time-average, and store tracer fluxes
        // Only process one tracer at a time to save on local memory / register requirements
        for (int tr=0; tr < num_tracers; tr++) {
          SArray<real,2,nAder,ngll> rt_DTs;  // Density * tracer
          SArray<real,2,nAder,ngll> rut_DTs; // Density * uwind * tracer
          for (int ii=0; ii < ngll; ii++) {
            rt_DTs (0,ii) = tracers_gll(tr,ii,k,j,i,iens);
            rut_DTs(0,ii) = rt_DTs(0,ii) * ru_DTs(0,ii) / r_DTs(0,ii);
          }

          //////////////////////////////////////////
          // Compute time derivatives if necessary
          //////////////////////////////////////////
          if (nAder > 1) {
            diffTransformTracer( r_DTs , ru_DTs , rt_DTs , rut_DTs , derivMatrix , dx );
          }

          //////////////////////////////////////////
          // Time average if necessary
          //////////////////////////////////////////
          if (timeAvg) {
            compute_timeAvg( rt_DTs  , dt );
          }
          if (tracer_pos(tr)) {
            for (int ii=0; ii < ngll; ii++) { rt_DTs(0,ii) = max( 0._fp , rt_DTs(0,ii) ); }
          }

          ////////////////////////////////////////////////////////////
          // Store cell edge estimates of the tracer
          ////////////////////////////////////////////////////////////
          tracerLimits(tr,1,k,j,i  ,iens) = rt_DTs (0,0     ); // Left interface
          tracerLimits(tr,0,k,j,i+1,iens) = rt_DTs (0,ngll-1); // Right interface
        }
      } // END: Reconstruct, time-average, and store tracer fluxes

    });

    state_gll   = real6d();
    tracers_gll = real6d();

    ////////////////////////////////////////////////
    // BCs for the state edge estimates
    ////////////////////////////////////////////////
    parallel_for( "Spatial.h X BCs edge" , SimpleBounds<3>(nz,ny,nens) , YAKL_LAMBDA (int k, int j, int iens) {
      for (int l=0; l < num_state; l++) {
        if        (bc_x == BC_PERIODIC) {
          stateLimits(l,0,k,j,0 ,iens) = stateLimits(l,0,k,j,nx,iens);
          stateLimits(l,1,k,j,nx,iens) = stateLimits(l,1,k,j,0 ,iens);
        } else if (bc_x == BC_WALL    ) {
          stateLimits(l,0,k,j,0 ,iens) = stateLimits(l,1,k,j,0 ,iens);
          stateLimits(l,1,k,j,nx,iens) = stateLimits(l,0,k,j,nx,iens);
        }
      }
      for (int l=0; l < num_tracers; l++) {
        if        (bc_x == BC_PERIODIC) {
          tracerLimits(l,0,k,j,0 ,iens) = tracerLimits(l,0,k,j,nx,iens);
          tracerLimits(l,1,k,j,nx,iens) = tracerLimits(l,1,k,j,0 ,iens);
        } else if (bc_x == BC_WALL    ) {
          tracerLimits(l,0,k,j,0 ,iens) = tracerLimits(l,1,k,j,0 ,iens);
          tracerLimits(l,1,k,j,nx,iens) = tracerLimits(l,0,k,j,nx,iens);
        }
      }
    });

    //////////////////////////////////////////////////////////
    // Compute the upwind fluxes
    //////////////////////////////////////////////////////////
    parallel_for( "Spatial.h X Riemann" , SimpleBounds<4>(nz,ny,nx+1,nens) ,
                  YAKL_LAMBDA (int k, int j, int i, int iens) {
      // Get left and right state
      real r_L = stateLimits(idR,0,k,j,i,iens)    ;   real r_R = stateLimits(idR,1,k,j,i,iens)    ;
      real u_L = stateLimits(idU,0,k,j,i,iens)/r_L;   real u_R = stateLimits(idU,1,k,j,i,iens)/r_R;
      real v_L = stateLimits(idV,0,k,j,i,iens)/r_L;   real v_R = stateLimits(idV,1,k,j,i,iens)/r_R;
      real w_L = stateLimits(idW,0,k,j,i,iens)/r_L;   real w_R = stateLimits(idW,1,k,j,i,iens)/r_R;
      real t_L = stateLimits(idT,0,k,j,i,iens)/r_L;   real t_R = stateLimits(idT,1,k,j,i,iens)/r_R;
      // Compute average state
      real r = 0.5_fp * (r_L + r_R);
      real u = 0.5_fp * (u_L + u_R);
      real v = 0.5_fp * (v_L + v_R);
      real w = 0.5_fp * (w_L + w_R);
      real t = 0.5_fp * (t_L + t_R);
      real p = C0 * pow(r*t,gamma);
      real cs2 = gamma*p/r;
      real cs  = sqrt(cs2);

      // COMPUTE UPWIND STATE FLUXES
      // Get left and right fluxes
      real q1_L = stateLimits(idR,0,k,j,i,iens);   real q1_R = stateLimits(idR,1,k,j,i,iens);
      real q2_L = stateLimits(idU,0,k,j,i,iens);   real q2_R = stateLimits(idU,1,k,j,i,iens);
      real q3_L = stateLimits(idV,0,k,j,i,iens);   real q3_R = stateLimits(idV,1,k,j,i,iens);
      real q4_L = stateLimits(idW,0,k,j,i,iens);   real q4_R = stateLimits(idW,1,k,j,i,iens);
      real q5_L = stateLimits(idT,0,k,j,i,iens);   real q5_R = stateLimits(idT,1,k,j,i,iens);
      // Compute upwind characteristics
      // Waves 1-3, velocity: u
      real w1, w2, w3;
      if (u > 0) {
        w1 = q1_L - q5_L/t;
        w2 = q3_L - v*q5_L/t;
        w3 = q4_L - w*q5_L/t;
      } else {
        w1 = q1_R - q5_R/t;
        w2 = q3_R - v*q5_R/t;
        w3 = q4_R - w*q5_R/t;
      }
      // Wave 5, velocity: u-cs
      real w5 =  u*q1_R/(2*cs) - q2_R/(2*cs) + q5_R/(2*t);
      // Wave 6, velocity: u+cs
      real w6 = -u*q1_L/(2*cs) + q2_L/(2*cs) + q5_L/(2*t);
      // Use right eigenmatrix to compute upwind flux
      real q1 = w1 + w5 + w6;
      real q2 = u*w1 + (u-cs)*w5 + (u+cs)*w6;
      real q3 = w2 + v*w5 + v*w6;
      real q4 = w3 + w*w5 + w*w6;
      real q5 =      t*w5 + t*w6;

      stateLimits(idR,0,k,j,i,iens) = q2;
      stateLimits(idU,0,k,j,i,iens) = q2*q2/q1 + C0*pow(q5,gamma);
      stateLimits(idV,0,k,j,i,iens) = q2*q3/q1;
      stateLimits(idW,0,k,j,i,iens) = q2*q4/q1;
      stateLimits(idT,0,k,j,i,iens) = q2*q5/q1;

      // COMPUTE UPWIND TRACER FLUXES
      // Handle it one tracer at a time
      for (int tr=0; tr < num_tracers; tr++) {
        if (u > 0) {
          tracerLimits(tr,0,k,j,i,iens) = q2 * tracerLimits(tr,0,k,j,i,iens) / r_L;
        } else {
          tracerLimits(tr,0,k,j,i,iens) = q2 * tracerLimits(tr,1,k,j,i,iens) / r_R;
        }
      }
    });

    //////////////////////////////////////////////////////////
    // Limit the tracer fluxes for positivity
    //////////////////////////////////////////////////////////
    real5d fct_mult("fct_mult",num_tracers,nz,ny,nx+1,nens);
    parallel_for( "Spatial.h X FCT" , SimpleBounds<5>(num_tracers,nz,ny,nx+1,nens) ,
                  YAKL_LAMBDA (int tr, int k, int j, int i, int iens) {
      fct_mult(tr,k,j,i,iens) = 1.;
      // Solid wall BCs mean u == 0 at boundaries, so we assume periodic if u != 0
      if (tracer_pos(tr)) {
        // Compute and apply the flux reduction factor of the upwind cell
        if      (tracerLimits(tr,0,k,j,i,iens) > 0) {
          // if u > 0, then it pulls mass out of the left cell
          int ind_i = i-1;
          // TODO: Relax the periodic assumption here
          if (ind_i == -1) ind_i = nx-1;
          real f1 = min( tracerLimits(tr,0,k,j,ind_i  ,iens) , 0._fp );
          real f2 = max( tracerLimits(tr,0,k,j,ind_i+1,iens) , 0._fp );
          real fluxOut = dt*(f2-f1)/dx;
          real mass = tracers(tr,hs+k,hs+j,hs+ind_i,iens);
          if (fluxOut > 0) {
            fct_mult(tr,k,j,i,iens) = min( 1._fp , mass / fluxOut );
          }
        } else if (tracerLimits(tr,0,k,j,i,iens) < 0) {
          // upwind is to the right of this interface
          int ind_i = i;
          // TODO: Relax the periodic assumption here
          if (ind_i == nx) ind_i = 0;
          real f1 = min( tracerLimits(tr,0,k,j,ind_i  ,iens) , 0._fp );
          real f2 = max( tracerLimits(tr,0,k,j,ind_i+1,iens) , 0._fp );
          real fluxOut = dt*(f2-f1)/dx;
          real mass = tracers(tr,hs+k,hs+j,hs+ind_i,iens);
          if (fluxOut > 0) {
            fct_mult(tr,k,j,i,iens) = min( 1._fp , mass / fluxOut );
          }
        }
      }
    });

    //////////////////////////////////////////////////////////
    // Compute the tendencies
    //////////////////////////////////////////////////////////
    parallel_for( "Spatial.h X tendencies" , SimpleBounds<4>(nz,ny,nx,nens) ,
                  YAKL_LAMBDA(int k, int j, int i, int iens) {
      for (int l = 0; l < num_state; l++) {
        if (sim2d && l == idV) {
          stateTend(l,k,j,i,iens) = 0;
        } else {
          stateTend(l,k,j,i,iens) = - ( stateLimits(l,0,k,j,i+1,iens) - stateLimits(l,0,k,j,i,iens) ) / dx;
        }
      }
      for (int l = 0; l < num_tracers; l++) {
        // Compute tracer tendency
        tracerTend(l,k,j,i,iens) = - ( tracerLimits(l,0,k,j,i+1,iens)*fct_mult(l,k,j,i+1,iens) -
                                       tracerLimits(l,0,k,j,i  ,iens)*fct_mult(l,k,j,i  ,iens) ) / dx;
      }
    });
  }



  void computeTendenciesY( real5d const &state   , real5d const &stateTend  ,
                           real5d const &tracers , real5d const &tracerTend ,
                           real &dt ) const {
    YAKL_SCOPE( ny                      , this->ny                     );
    YAKL_SCOPE( weno_scalars            , this->weno_scalars           );
    YAKL_SCOPE( weno_winds              , this->weno_winds             );
    YAKL_SCOPE( c2g                     , this->coefs_to_gll           );
    YAKL_SCOPE( s2g                     , this->sten_to_gll            );
    YAKL_SCOPE( s2c                     , this->sten_to_coefs          );
    YAKL_SCOPE( weno_recon_lower        , this->weno_recon_lower       );
    YAKL_SCOPE( idl                     , this->idl                    );
    YAKL_SCOPE( sigma                   , this->sigma                  );
    YAKL_SCOPE( hyDensCells             , this->hyDensCells            );
    YAKL_SCOPE( hyDensThetaCells        , this->hyDensThetaCells       );
    YAKL_SCOPE( derivMatrix             , this->derivMatrix            );
    YAKL_SCOPE( dy                      , this->dy                     );
    YAKL_SCOPE( tracer_pos              , this->tracer_pos             );
    YAKL_SCOPE( num_tracers             , this->num_tracers            );
    YAKL_SCOPE( bc_y                    , this->bc_y                   );
    YAKL_SCOPE( gamma                   , this->gamma                  );
    YAKL_SCOPE( C0                      , this->C0                     );

    // Populate the halos
    if        (bc_y == BC_PERIODIC) {
      parallel_for( "Spatial.h Y BCs periodic" , SimpleBounds<4>(nz,nx,hs,nens) ,
                    YAKL_LAMBDA(int k, int i, int jj, int iens) {
        for (int l=0; l < num_state; l++) {
          state(l,hs+k,      jj,hs+i,iens) = state(l,hs+k,ny+jj,hs+i,iens);
          state(l,hs+k,hs+ny+jj,hs+i,iens) = state(l,hs+k,hs+jj,hs+i,iens);
        }
        for (int l=0; l < num_tracers; l++) {
          tracers(l,hs+k,      jj,hs+i,iens) = tracers(l,hs+k,ny+jj,hs+i,iens);
          tracers(l,hs+k,hs+ny+jj,hs+i,iens) = tracers(l,hs+k,hs+jj,hs+i,iens);
        }
      });
    } else if (bc_y == BC_WALL) {
      parallel_for( "Spatial.h Y BCs wall" , SimpleBounds<4>(nz,nx,hs,nens) ,
                    YAKL_LAMBDA(int k, int i, int jj, int iens) {
        for (int l=0; l < num_state; l++) {
          if (l == idV) {
            state(l,hs+k,      jj,hs+i,iens) = 0;
            state(l,hs+k,hs+ny+jj,hs+i,iens) = 0;
          } else {
            state(l,hs+k,      jj,hs+i,iens) = state(l,hs+k,hs     ,hs+i,iens);
            state(l,hs+k,hs+ny+jj,hs+i,iens) = state(l,hs+k,hs+ny-1,hs+i,iens);
          }
        }
        for (int l=0; l < num_tracers; l++) {
          tracers(l,hs+k,      jj,hs+i,iens) = tracers(l,hs+k,hs     ,hs+i,iens);
          tracers(l,hs+k,hs+ny+jj,hs+i,iens) = tracers(l,hs+k,hs+ny-1,hs+i,iens);
        }
      });
    }

    real6d state_gll  ("state_gll"  ,num_state  ,ngll,nz,ny,nx,nens);
    real6d tracers_gll("tracers_gll",num_tracers,ngll,nz,ny,nx,nens);

    // Loop through all cells, reconstruct in y-direction, compute centered tendencies, store cell-edge state estimates
    parallel_for( "Spatial.h Y recon" , SimpleBounds<4>(nz,ny,nx,nens) ,
                  YAKL_LAMBDA (int k, int j, int i, int iens) {
      SArray<real,1,ord>  stencil;
      SArray<real,1,ngll> gll;

      // Density
      for (int jj=0; jj < ord; jj++) { stencil(jj) = state(idR,hs+k,j+jj,hs+i,iens); }
      reconstruct_gll_values( stencil , gll , c2g , s2g , s2c , weno_recon_lower , idl , sigma , weno_scalars );
      // Add hydrostasis back on
      for (int jj=0; jj < ngll; jj++) { state_gll(idR,jj,k,j,i,iens) = gll(jj) + hyDensCells(k,iens); }

      // u values and derivatives
      for (int jj=0; jj < ord; jj++) { stencil(jj) = state(idU,hs+k,j+jj,hs+i,iens); }
      reconstruct_gll_values( stencil , gll , c2g , s2g , s2c , weno_recon_lower , idl , sigma , weno_winds );
      for (int jj=0; jj < ngll; jj++) { state_gll(idU,jj,k,j,i,iens) = gll(jj); }

      // v
      for (int jj=0; jj < ord; jj++) { stencil(jj) = state(idV,hs+k,j+jj,hs+i,iens); }
      reconstruct_gll_values( stencil , gll , c2g , s2g , s2c , weno_recon_lower , idl , sigma , weno_winds );
      for (int jj=0; jj < ngll; jj++) { state_gll(idV,jj,k,j,i,iens) = gll(jj); }

      // w
      for (int jj=0; jj < ord; jj++) { stencil(jj) = state(idW,hs+k,j+jj,hs+i,iens); }
      reconstruct_gll_values( stencil , gll , c2g , s2g , s2c , weno_recon_lower , idl , sigma , weno_winds );
      for (int jj=0; jj < ngll; jj++) { state_gll(idW,jj,k,j,i,iens) = gll(jj); }

      // theta
      for (int jj=0; jj < ord; jj++) { stencil(jj) = state(idT,hs+k,j+jj,hs+i,iens); }
      reconstruct_gll_values( stencil , gll , c2g , s2g , s2c , weno_recon_lower , idl , sigma , weno_scalars );
      // Add hydrostasis back on
      for (int jj=0; jj < ngll; jj++) { state_gll(idT,jj,k,j,i,iens) = gll(jj) + hyDensThetaCells(k,iens); }

      for (int tr=0; tr < num_tracers; tr++) {
        for (int jj=0; jj < ord; jj++) { stencil(jj) = tracers(tr,hs+k,j+jj,hs+i,iens); }
        reconstruct_gll_values( stencil , gll , c2g , s2g , s2c , weno_recon_lower , idl , sigma , weno_scalars );
        if (tracer_pos(tr)) {
          for (int jj=0; jj < ngll; jj++) { gll(jj) = max( 0._fp , gll(jj) ); }
        }
        for (int jj=0; jj < ngll; jj++) { tracers_gll(tr,jj,k,j,i,iens) = gll(jj); }
      }

    });

    // Stores two estimates of the state and tracer values at each cell interface
    real6d stateLimits ("stateLimits" ,num_state  ,2,nz,ny+1,nx,nens);
    real6d tracerLimits("tracerLimits",num_tracers,2,nz,ny+1,nx,nens);

    // Loop through all cells, reconstruct in y-direction, compute centered tendencies, store cell-edge state estimates
    parallel_for( "Spatial.h Y ader" , SimpleBounds<4>(nz,ny,nx,nens) ,
                  YAKL_LAMBDA (int k, int j, int i, int iens) {
      SArray<real,2,nAder,ngll> r_DTs , rv_DTs;
      { // BEGIN: Reconstruct, time-average, and store state and sate fluxes
        SArray<real,2,nAder,ngll> ru_DTs , rw_DTs , rt_DTs;
        SArray<real,2,nAder,ngll> rvu_DTs , rvv_DTs , rvw_DTs , rvt_DTs , rt_gamma_DTs;
        for (int jj=0; jj < ngll; jj++) {
          real r = state_gll(idR,jj,k,j,i,iens);
          real u = state_gll(idU,jj,k,j,i,iens) / r;
          real v = state_gll(idV,jj,k,j,i,iens) / r;
          real w = state_gll(idW,jj,k,j,i,iens) / r;
          real t = state_gll(idT,jj,k,j,i,iens) / r;
          r_DTs       (0,jj) = r;
          ru_DTs      (0,jj) = r*u;
          rv_DTs      (0,jj) = r*v;
          rw_DTs      (0,jj) = r*w;
          rt_DTs      (0,jj) = r*t;
          rvu_DTs     (0,jj) = r*v*u;
          rvv_DTs     (0,jj) = r*v*v;
          rvw_DTs     (0,jj) = r*v*w;
          rvt_DTs     (0,jj) = r*v*t;
          rt_gamma_DTs(0,jj) = pow(r*t,gamma);
        }

        //////////////////////////////////////////
        // Compute time derivatives if necessary
        //////////////////////////////////////////
        if (nAder > 1) {
          diffTransformEulerConsY( r_DTs , ru_DTs , rv_DTs , rw_DTs , rt_DTs , rvu_DTs , rvv_DTs , rvw_DTs ,
                                   rvt_DTs , rt_gamma_DTs , derivMatrix , C0 , gamma , dy );
        }

        //////////////////////////////////////////
        // Time average if necessary
        //////////////////////////////////////////
        // Don't overwrite r and rv because we need them for tracers
        SArray<real,1,ngll> r_tavg, rv_tavg;
        if (timeAvg) {
          compute_timeAvg( r_DTs  , r_tavg  , dt );
          compute_timeAvg( ru_DTs           , dt );
          compute_timeAvg( rv_DTs , rv_tavg , dt );
          compute_timeAvg( rw_DTs           , dt );
          compute_timeAvg( rt_DTs           , dt );
        } else {
          for (int jj=0; jj < ngll; jj++) {
            r_tavg (jj) = r_DTs (0,jj);
            rv_tavg(jj) = rv_DTs(0,jj);
          }
        }

        //////////////////////////////////////////
        // Store cell edge estimates of the state
        //////////////////////////////////////////
        // Left interface
        stateLimits(idR,1,k,j  ,i,iens) = r_tavg  (0     );
        stateLimits(idU,1,k,j  ,i,iens) = ru_DTs(0,0     );
        stateLimits(idV,1,k,j  ,i,iens) = rv_tavg (0     );
        stateLimits(idW,1,k,j  ,i,iens) = rw_DTs(0,0     );
        stateLimits(idT,1,k,j  ,i,iens) = rt_DTs(0,0     );
        // Right interface
        stateLimits(idR,0,k,j+1,i,iens) = r_tavg  (ngll-1);
        stateLimits(idU,0,k,j+1,i,iens) = ru_DTs(0,ngll-1);
        stateLimits(idV,0,k,j+1,i,iens) = rv_tavg (ngll-1);
        stateLimits(idW,0,k,j+1,i,iens) = rw_DTs(0,ngll-1);
        stateLimits(idT,0,k,j+1,i,iens) = rt_DTs(0,ngll-1);
      } // END: Reconstruct, time-average, and store state and sate fluxes

      // r_DTs and rv_DTs still exist and are computed
      { // BEGIN: Reconstruct, time-average, and store tracer fluxes
        for (int tr=0; tr < num_tracers; tr++) {
          SArray<real,2,nAder,ngll> rt_DTs;  // Density * tracer
          SArray<real,2,nAder,ngll> rvt_DTs; // Density * vwind * tracer
          for (int jj=0; jj < ngll; jj++) {
            rt_DTs (0,jj) = tracers_gll(tr,jj,k,j,i,iens);
            rvt_DTs(0,jj) = rt_DTs(0,jj) * rv_DTs(0,jj) / r_DTs(0,jj);
          }

          //////////////////////////////////////////
          // Compute time derivatives if necessary
          //////////////////////////////////////////
          if (nAder > 1) {
            diffTransformTracer( r_DTs , rv_DTs , rt_DTs , rvt_DTs , derivMatrix , dy );
          }

          //////////////////////////////////////////
          // Time average if necessary
          //////////////////////////////////////////
          if (timeAvg) {
            compute_timeAvg( rt_DTs  , dt );
          }
          if (tracer_pos(tr)) {
            for (int jj=0; jj < ngll; jj++) { rt_DTs(0,jj) = max( 0._fp , rt_DTs(0,jj) ); }
          }

          ////////////////////////////////////////////////////////////
          // Store cell edge estimates of the tracer
          ////////////////////////////////////////////////////////////
          tracerLimits(tr,1,k,j  ,i,iens) = rt_DTs (0,0     ); // Left interface
          tracerLimits(tr,0,k,j+1,i,iens) = rt_DTs (0,ngll-1); // Right interface
        }
      } // END: Reconstruct, time-average, and store tracer fluxes

    });

    ////////////////////////////////////////////////
    // BCs for the state edge estimates
    ////////////////////////////////////////////////
    parallel_for( "Spatial.h Y BCs edge" , SimpleBounds<3>(nz,nx,nens) , YAKL_LAMBDA (int k, int i, int iens) {
      for (int l=0; l < num_state; l++) {
        if        (bc_y == BC_PERIODIC) {
          stateLimits(l,0,k,0 ,i,iens) = stateLimits(l,0,k,ny,i,iens);
          stateLimits(l,1,k,ny,i,iens) = stateLimits(l,1,k,0 ,i,iens);
        } else if (bc_y == BC_WALL    ) {
          stateLimits(l,0,k,0 ,i,iens) = stateLimits(l,1,k,0 ,i,iens);
          stateLimits(l,1,k,ny,i,iens) = stateLimits(l,0,k,ny,i,iens);
        }
      }
      for (int l=0; l < num_tracers; l++) {
        if        (bc_y == BC_PERIODIC) {
          tracerLimits(l,0,k,0 ,i,iens) = tracerLimits(l,0,k,ny,i,iens);
          tracerLimits(l,1,k,ny,i,iens) = tracerLimits(l,1,k,0 ,i,iens);
        } else if (bc_y == BC_WALL    ) {
          tracerLimits(l,0,k,0 ,i,iens) = tracerLimits(l,1,k,0 ,i,iens);
          tracerLimits(l,1,k,ny,i,iens) = tracerLimits(l,0,k,ny,i,iens);
        }
      }
    });

    //////////////////////////////////////////////////////////
    // Compute the upwind fluxes
    //////////////////////////////////////////////////////////
    parallel_for( "Spatial.h Y Riemann" , SimpleBounds<4>(nz,ny+1,nx,nens) ,
                  YAKL_LAMBDA (int k, int j, int i, int iens) {
      // Get left and right state
      real r_L = stateLimits(idR,0,k,j,i,iens)    ;   real r_R = stateLimits(idR,1,k,j,i,iens)    ;
      real u_L = stateLimits(idU,0,k,j,i,iens)/r_L;   real u_R = stateLimits(idU,1,k,j,i,iens)/r_R;
      real v_L = stateLimits(idV,0,k,j,i,iens)/r_L;   real v_R = stateLimits(idV,1,k,j,i,iens)/r_R;
      real w_L = stateLimits(idW,0,k,j,i,iens)/r_L;   real w_R = stateLimits(idW,1,k,j,i,iens)/r_R;
      real t_L = stateLimits(idT,0,k,j,i,iens)/r_L;   real t_R = stateLimits(idT,1,k,j,i,iens)/r_R;
      // Compute average state
      real r = 0.5_fp * (r_L + r_R);
      real u = 0.5_fp * (u_L + u_R);
      real v = 0.5_fp * (v_L + v_R);
      real w = 0.5_fp * (w_L + w_R);
      real t = 0.5_fp * (t_L + t_R);
      real p = C0 * pow(r*t,gamma);
      real cs2 = gamma*p/r;
      real cs  = sqrt(cs2);

      // COMPUTE UPWIND STATE FLUXES
      // Get left and right fluxes
      real q1_L = stateLimits(idR,0,k,j,i,iens);   real q1_R = stateLimits(idR,1,k,j,i,iens);
      real q2_L = stateLimits(idU,0,k,j,i,iens);   real q2_R = stateLimits(idU,1,k,j,i,iens);
      real q3_L = stateLimits(idV,0,k,j,i,iens);   real q3_R = stateLimits(idV,1,k,j,i,iens);
      real q4_L = stateLimits(idW,0,k,j,i,iens);   real q4_R = stateLimits(idW,1,k,j,i,iens);
      real q5_L = stateLimits(idT,0,k,j,i,iens);   real q5_R = stateLimits(idT,1,k,j,i,iens);
      // Compute upwind characteristics
      // Waves 1-3, velocity: v
      real w1, w2, w3;
      if (v > 0) {
        w1 = q1_L - q5_L/t;
        w2 = q2_L - u*q5_L/t;
        w3 = q4_L - w*q5_L/t;
      } else {
        w1 = q1_R - q5_R/t;
        w2 = q2_R - u*q5_R/t;
        w3 = q4_R - w*q5_R/t;
      }
      // Wave 5, velocity: v-cs
      real w5 =  v*q1_R/(2*cs) - q3_R/(2*cs) + q5_R/(2*t);
      // Wave 6, velocity: v+cs
      real w6 = -v*q1_L/(2*cs) + q3_L/(2*cs) + q5_L/(2*t);
      // Use right eigenmatrix to compute upwind flux
      real q1 = w1 + w5 + w6;
      real q2 = w2 + u*w5 + u*w6;
      real q3 = v*w1 + (v-cs)*w5 + (v+cs)*w6;
      real q4 = w3 + w*w5 + w*w6;
      real q5 =      t*w5 + t*w6;

      stateLimits(idR,0,k,j,i,iens) = q3;
      stateLimits(idU,0,k,j,i,iens) = q3*q2/q1;
      stateLimits(idV,0,k,j,i,iens) = q3*q3/q1 + C0*pow(q5,gamma);
      stateLimits(idW,0,k,j,i,iens) = q3*q4/q1;
      stateLimits(idT,0,k,j,i,iens) = q3*q5/q1;

      // COMPUTE UPWIND TRACER FLUXES
      // Handle it one tracer at a time
      for (int tr=0; tr < num_tracers; tr++) {
        if (v > 0) {
          tracerLimits(tr,0,k,j,i,iens) = q3 * tracerLimits(tr,0,k,j,i,iens) / r_L;
        } else {
          tracerLimits(tr,0,k,j,i,iens) = q3 * tracerLimits(tr,1,k,j,i,iens) / r_R;
        }
      }
    });

    //////////////////////////////////////////////////////////
    // Limit the tracer fluxes for positivity
    //////////////////////////////////////////////////////////
    real5d fct_mult("fct_mult",num_tracers,nz,ny+1,nx,nens);
    parallel_for( "Spatial.h Y FCT" , SimpleBounds<5>(num_tracers,nz,ny+1,nx,nens) ,
                  YAKL_LAMBDA (int tr, int k, int j, int i, int iens) {
      fct_mult(tr,k,j,i,iens) = 1.;
      // Solid wall BCs mean u == 0 at boundaries, so we assume periodic if u != 0
      if (tracer_pos(tr)) {
        // Compute and apply the flux reduction factor of the upwind cell
        if      (tracerLimits(tr,0,k,j,i,iens) > 0) {
          // upwind is to the left of this interface
          int ind_j = j-1;
          if (ind_j == -1) ind_j = ny-1;
          real f1 = min( tracerLimits(tr,0,k,ind_j  ,i,iens) , 0._fp );
          real f2 = max( tracerLimits(tr,0,k,ind_j+1,i,iens) , 0._fp );
          real fluxOut = dt*(f2-f1)/dy;
          real mass = tracers(tr,hs+k,hs+ind_j,hs+i,iens);
          if (fluxOut > 0) {
            fct_mult(tr,k,j,i,iens) = min( 1._fp , mass / fluxOut );
          }
        } else if (tracerLimits(tr,0,k,j,i,iens) < 0) {
          // upwind is to the right of this interface
          int ind_j = j;
          if (ind_j == ny) ind_j = 0;
          real f1 = min( tracerLimits(tr,0,k,ind_j  ,i,iens) , 0._fp );
          real f2 = max( tracerLimits(tr,0,k,ind_j+1,i,iens) , 0._fp );
          real fluxOut = dt*(f2-f1)/dy;
          real mass = tracers(tr,hs+k,hs+ind_j,hs+i,iens);
          if (fluxOut > 0) {
            fct_mult(tr,k,j,i,iens) = min( 1._fp , mass / fluxOut );
          }
        }
      }
    });

    //////////////////////////////////////////////////////////
    // Compute the tendencies
    //////////////////////////////////////////////////////////
    parallel_for( "Spatial.h Y tendendies" , SimpleBounds<4>(nz,ny,nx,nens) ,
                  YAKL_LAMBDA(int k, int j, int i, int iens) {
      for (int l=0; l < num_state; l++) {
        stateTend(l,k,j,i,iens) = - ( stateLimits(l,0,k,j+1,i,iens) - stateLimits(l,0,k,j,i,iens) ) / dy;
      }
      for (int l=0; l < num_tracers; l++) {
        // Compute the tracer tendency
        tracerTend(l,k,j,i,iens) = - ( tracerLimits(l,0,k,j+1,i,iens)*fct_mult(l,k,j+1,i,iens) -
                                       tracerLimits(l,0,k,j  ,i,iens)*fct_mult(l,k,j  ,i,iens) ) / dy;
      }
    });
  }



  void computeTendenciesZ( real5d const &state   , real5d const &stateTend  ,
                           real5d const &tracers , real5d const &tracerTend ,
                           real &dt ) const {
    YAKL_SCOPE( nz                      , this->nz                     );
    YAKL_SCOPE( weno_scalars            , this->weno_scalars           );
    YAKL_SCOPE( weno_winds              , this->weno_winds             );
    YAKL_SCOPE( c2g                     , this->coefs_to_gll           );
    YAKL_SCOPE( idl                     , this->idl                    );
    YAKL_SCOPE( sigma                   , this->sigma                  );
    YAKL_SCOPE( hyDensCells             , this->hyDensCells            );
    YAKL_SCOPE( hyDensGLL               , this->hyDensGLL              );
    YAKL_SCOPE( hyDensThetaGLL          , this->hyDensThetaGLL         );
    YAKL_SCOPE( hyPressureGLL           , this->hyPressureGLL          );
    YAKL_SCOPE( sim2d                   , this->sim2d                  );
    YAKL_SCOPE( derivMatrix             , this->derivMatrix            );
    YAKL_SCOPE( dz                      , this->dz                     );
    YAKL_SCOPE( tracer_pos              , this->tracer_pos             );
    YAKL_SCOPE( num_tracers             , this->num_tracers            );
    YAKL_SCOPE( bc_z                    , this->bc_z                   );
    YAKL_SCOPE( gllWts_ngll             , this->gllWts_ngll            );
    YAKL_SCOPE( gamma                   , this->gamma                  );
    YAKL_SCOPE( C0                      , this->C0                     );
    YAKL_SCOPE( vert_sten_to_gll        , this->vert_sten_to_gll       );
    YAKL_SCOPE( vert_sten_to_coefs      , this->vert_sten_to_coefs     );
    YAKL_SCOPE( vert_weno_recon_lower   , this->vert_weno_recon_lower  );
    YAKL_SCOPE( grav                    , this->grav                   );

    // Pre-process the tracers by dividing by density inside the domain
    // After this, we can reconstruct tracers only (not rho * tracer)
    parallel_for( "Spatial.h Z tracer div dens" , SimpleBounds<5>(num_tracers,nz,ny,nx,nens) ,
                  YAKL_LAMBDA (int tr, int k, int j, int i, int iens) {
      tracers(tr,hs+k,hs+j,hs+i,iens) /= (state(idR,hs+k,hs+j,hs+i,iens) + hyDensCells(k,iens));
    });

    // Populate the halos
    if        (bc_z == BC_PERIODIC) {
      parallel_for( "Spatial.h Z BCs periodic" , SimpleBounds<4>(ny,nx,hs,nens) ,
                    YAKL_LAMBDA(int j, int i, int kk, int iens) {
        for (int l=0; l < num_state; l++) {
          state(l,      kk,hs+j,hs+i,iens) = state(l,nz+kk,hs+j,hs+i,iens);
          state(l,hs+nz+kk,hs+j,hs+i,iens) = state(l,hs+kk,hs+j,hs+i,iens);
        }
        for (int l=0; l < num_tracers; l++) {
          tracers(l,      kk,hs+j,hs+i,iens) = tracers(l,nz+kk,hs+j,hs+i,iens);
          tracers(l,hs+nz+kk,hs+j,hs+i,iens) = tracers(l,hs+kk,hs+j,hs+i,iens);
        }
      });
    } else if (bc_z == BC_WALL) {
      parallel_for( "Spatial.h Z BCs wall" , SimpleBounds<4>(ny,nx,hs,nens) ,
                    YAKL_LAMBDA(int j, int i, int kk, int iens) {
        for (int l=0; l < num_state; l++) {
          if (l == idW) {
            state(l,      kk,hs+j,hs+i,iens) = 0;
            state(l,hs+nz+kk,hs+j,hs+i,iens) = 0;
          } else {
            state(l,      kk,hs+j,hs+i,iens) = state(l,hs     ,hs+j,hs+i,iens);
            state(l,hs+nz+kk,hs+j,hs+i,iens) = state(l,hs+nz-1,hs+j,hs+i,iens);
          }
        }
        for (int l=0; l < num_tracers; l++) {
          tracers(l,      kk,hs+j,hs+i,iens) = tracers(l,hs     ,hs+j,hs+i,iens);
          tracers(l,hs+nz+kk,hs+j,hs+i,iens) = tracers(l,hs+nz-1,hs+j,hs+i,iens);
        }
      });
    }

    real6d state_gll  ("state_gll"  ,num_state  ,ngll,nz,ny,nx,nens);
    real6d tracers_gll("tracers_gll",num_tracers,ngll,nz,ny,nx,nens);

    // Loop through all cells, reconstruct in x-direction, compute centered tendencies, store cell-edge state estimates
    parallel_for( "Spatial.h Z recon" , SimpleBounds<4>(nz,ny,nx,nens) ,
                  YAKL_LAMBDA (int k, int j, int i, int iens) {
      SArray<real,2,ord,ngll>       s2g_loc;
      SArray<real,2,ord,ord>        s2c_loc;
      SArray<real,3,hs+1,hs+1,hs+1> weno_recon_lower_loc;
      for (int jj=0; jj < ord; jj++) {
        for (int ii=0; ii < ngll; ii++) {
          s2g_loc(jj,ii) = vert_sten_to_gll(k,jj,ii,iens);
        }
      }
      for (int jj=0; jj < ord; jj++) {
        for (int ii=0; ii < ord; ii++) {
          s2c_loc(jj,ii) = vert_sten_to_coefs(k,jj,ii,iens);
        }
      }
      for (int kk=0; kk < hs+1; kk++) {
        for (int jj=0; jj < hs+1; jj++) {
          for (int ii=0; ii < hs+1; ii++) {
            weno_recon_lower_loc(kk,jj,ii) = vert_weno_recon_lower(k,kk,jj,ii,iens);
          }
        }
      }

      SArray<real,1,ord>  stencil;
      SArray<real,1,ngll> gll;

      // Density
      for (int kk=0; kk < ord; kk++) { stencil(kk) = state(idR,k+kk,hs+j,hs+i,iens); }
      reconstruct_gll_values( stencil , gll , c2g , s2g_loc , s2c_loc , weno_recon_lower_loc ,
                              idl , sigma , weno_scalars );
      // Add hydrostasis back on
      for (int kk=0; kk < ngll; kk++) { state_gll(idR,kk,k,j,i,iens) = gll(kk) + hyDensGLL(k,kk,iens); }

      // u values and derivatives
      for (int kk=0; kk < ord; kk++) { stencil(kk) = state(idU,k+kk,hs+j,hs+i,iens); }
      reconstruct_gll_values( stencil , gll , c2g , s2g_loc , s2c_loc , weno_recon_lower_loc ,
                              idl , sigma , weno_winds );
      for (int kk=0; kk < ngll; kk++) { state_gll(idU,kk,k,j,i,iens) = gll(kk); }

      // v
      for (int kk=0; kk < ord; kk++) { stencil(kk) = state(idV,k+kk,hs+j,hs+i,iens); }
      reconstruct_gll_values( stencil , gll , c2g , s2g_loc , s2c_loc , weno_recon_lower_loc ,
                              idl , sigma , weno_winds );
      for (int kk=0; kk < ngll; kk++) { state_gll(idV,kk,k,j,i,iens) = gll(kk); }

      // w
      for (int kk=0; kk < ord; kk++) { stencil(kk) = state(idW,k+kk,hs+j,hs+i,iens); }
      reconstruct_gll_values( stencil , gll , c2g , s2g_loc , s2c_loc , weno_recon_lower_loc ,
                              idl , sigma , weno_winds );
      if (bc_z == BC_WALL) {
        if (k == nz-1) gll(ngll-1) = 0;
        if (k == 0   ) gll(0     ) = 0;
      }
      for (int kk=0; kk < ngll; kk++) { state_gll(idW,kk,k,j,i,iens) = gll(kk); }

      // theta
      for (int kk=0; kk < ord; kk++) { stencil(kk) = state(idT,k+kk,hs+j,hs+i,iens); }
      reconstruct_gll_values( stencil , gll , c2g , s2g_loc , s2c_loc , weno_recon_lower_loc ,
                              idl , sigma , weno_scalars );
      // Add hydrostasis back on
      for (int kk=0; kk < ngll; kk++) { state_gll(idT,kk,k,j,i,iens) = gll(kk) + hyDensThetaGLL(k,kk,iens); }

      for (int tr=0; tr < num_tracers; tr++) {
        for (int kk=0; kk < ord; kk++) { stencil(kk) = tracers(tr,k+kk,hs+j,hs+i,iens); }
        reconstruct_gll_values( stencil , gll , c2g , s2g_loc , s2c_loc , weno_recon_lower_loc ,
                                idl , sigma , weno_scalars );
        for (int kk=0; kk < ngll; kk++) { gll(kk) *= state_gll(idR,kk,k,j,i,iens); }
        if (tracer_pos(tr)) {
          for (int kk=0; kk < ngll; kk++) { gll(kk) = max( 0._fp , gll(kk) ); }
        }
        for (int kk=0; kk < ngll; kk++) { tracers_gll(tr,kk,k,j,i,iens) = gll(kk); }
      }

    });

    // Stores two estimates of the state and tracer values at each cell interface
    real6d stateLimits ("stateLimits" ,num_state  ,2,nz+1,ny,nx,nens);
    real6d tracerLimits("tracerLimits",num_tracers,2,nz+1,ny,nx,nens);

    // Loop through all cells, reconstruct in x-direction, compute centered tendencies, store cell-edge state estimates
    parallel_for( "Spatial.h Z ader" , SimpleBounds<4>(nz,ny,nx,nens) , YAKL_LAMBDA (int k, int j, int i, int iens) {
      // We need these to persist to evolve tracers with ADER
      SArray<real,2,nAder,ngll> r_DTs , rw_DTs;
      { // BEGIN: reconstruct, time-avg, and store state & state fluxes
        SArray<real,2,nAder,ngll> ru_DTs , rv_DTs , rt_DTs;
        SArray<real,2,nAder,ngll> rwu_DTs , rwv_DTs , rww_DTs , rwt_DTs , rt_gamma_DTs;
        for (int kk=0; kk < ngll; kk++) {
          real r = state_gll(idR,kk,k,j,i,iens);
          real u = state_gll(idU,kk,k,j,i,iens) / r;
          real v = state_gll(idV,kk,k,j,i,iens) / r;
          real w = state_gll(idW,kk,k,j,i,iens) / r;
          real t = state_gll(idT,kk,k,j,i,iens) / r;
          r_DTs       (0,kk) = r;
          ru_DTs      (0,kk) = r*u;
          rv_DTs      (0,kk) = r*v;
          rw_DTs      (0,kk) = r*w;
          rt_DTs      (0,kk) = r*t;
          rwu_DTs     (0,kk) = r*w*u;
          rwv_DTs     (0,kk) = r*w*v;
          rww_DTs     (0,kk) = r*w*w;
          rwt_DTs     (0,kk) = r*w*t;
          rt_gamma_DTs(0,kk) = pow(r*t,gamma);
        }

        //////////////////////////////////////////
        // Compute time derivatives if necessary
        //////////////////////////////////////////
        if (nAder > 1) {
          diffTransformEulerConsZ( r_DTs , ru_DTs , rv_DTs , rw_DTs , rt_DTs , rwu_DTs , rwv_DTs , rww_DTs ,
                                   rwt_DTs , rt_gamma_DTs , derivMatrix , hyPressureGLL , C0 , gamma , k ,
                                   dz(k,iens) , bc_z , nz , iens );
        }

        //////////////////////////////////////////
        // Time average if necessary
        //////////////////////////////////////////
        // We can't alter density and momentum because they're needed for tracers later
        SArray<real,1,ngll> r_tavg, rw_tavg;
        if (timeAvg) {
          compute_timeAvg( r_DTs  , r_tavg  , dt );
          compute_timeAvg( ru_DTs           , dt );
          compute_timeAvg( rv_DTs           , dt );
          compute_timeAvg( rw_DTs , rw_tavg , dt );
          compute_timeAvg( rt_DTs           , dt );
        } else {
          for (int ii=0; ii < ngll; ii++) {
            r_tavg (ii) = r_DTs (0,ii);
            rw_tavg(ii) = rw_DTs(0,ii);
          }
        }

        //////////////////////////////////////////
        // Store cell edge estimates of the state
        //////////////////////////////////////////
        // Left interface
        stateLimits(idR,1,k  ,j,i,iens) = r_tavg  (0     );
        stateLimits(idU,1,k  ,j,i,iens) = ru_DTs(0,0     );
        stateLimits(idV,1,k  ,j,i,iens) = rv_DTs(0,0     );
        stateLimits(idW,1,k  ,j,i,iens) = rw_tavg (0     );
        stateLimits(idT,1,k  ,j,i,iens) = rt_DTs(0,0     );
        // Right interface
        stateLimits(idR,0,k+1,j,i,iens) = r_tavg  (ngll-1);
        stateLimits(idU,0,k+1,j,i,iens) = ru_DTs(0,ngll-1);
        stateLimits(idV,0,k+1,j,i,iens) = rv_DTs(0,ngll-1);
        stateLimits(idW,0,k+1,j,i,iens) = rw_tavg (ngll-1);
        stateLimits(idT,0,k+1,j,i,iens) = rt_DTs(0,ngll-1);

        ////////////////////////////////////////////
        // Assign gravity source term
        ////////////////////////////////////////////
        real ravg = 0;
        for (int kk=0; kk < ngll; kk++) {
          ravg += r_tavg(kk) * gllWts_ngll(kk);
        }
        stateTend(idR,k,j,i,iens) = 0;
        stateTend(idU,k,j,i,iens) = 0;
        stateTend(idV,k,j,i,iens) = 0;
        stateTend(idW,k,j,i,iens) = -grav * ravg;
        stateTend(idT,k,j,i,iens) = 0;
      } // END: reconstruct, time-avg, and store state & state fluxes

      // r_DTs and rw_DTs still exist and are computed
      { // BEGIN: Reconstruct, time-average, and store tracer fluxes
        // Only process one tracer at a time to save on local memory / register requirements
        for (int tr=0; tr < num_tracers; tr++) {
          SArray<real,2,nAder,ngll> rt_DTs;  // Density * tracer
          SArray<real,2,nAder,ngll> rwt_DTs; // Density * wwind * tracer
          for (int kk=0; kk < ngll; kk++) {
            rt_DTs (0,kk) = tracers_gll(tr,kk,k,j,i,iens);
            rwt_DTs(0,kk) = rt_DTs(0,kk) * rw_DTs(0,kk) / r_DTs(0,kk);
          }

          //////////////////////////////////////////
          // Compute time derivatives if necessary
          //////////////////////////////////////////
          if (nAder > 1) {
            diffTransformTracer( r_DTs , rw_DTs , rt_DTs , rwt_DTs , derivMatrix , dz(k,iens) );
          }

          //////////////////////////////////////////
          // Time average if necessary
          //////////////////////////////////////////
          if (timeAvg) {
            compute_timeAvg( rt_DTs  , dt );
          }
          if (tracer_pos(tr)) {
            for (int kk=0; kk < ngll; kk++) { rt_DTs(0,kk) = max( 0._fp , rt_DTs(0,kk) ); }
          }
          if (bc_z == BC_WALL) {
            if (k == nz-1) rwt_DTs(0,ngll-1) = 0;
            if (k == 0   ) rwt_DTs(0,0     ) = 0;
          }

          ////////////////////////////////////////////////////////////
          // Store cell edge estimates of the tracer
          ////////////////////////////////////////////////////////////
          tracerLimits(tr,1,k  ,j,i,iens) = rt_DTs (0,0     ); // Left interface
          tracerLimits(tr,0,k+1,j,i,iens) = rt_DTs (0,ngll-1); // Right interface
        }
      } // END: Reconstruct, time-average, and store tracer fluxes

    });

    state_gll   = real6d();
    tracers_gll = real6d();

    ////////////////////////////////////////////////
    // BCs for the state edge estimates
    ////////////////////////////////////////////////
    parallel_for( "Spatial.h Z BCs edge" , SimpleBounds<3>(ny,nx,nens) , YAKL_LAMBDA (int j, int i, int iens) {
      for (int l = 0; l < num_state; l++) {
        if        (bc_z == BC_PERIODIC) {
          stateLimits     (l,0,0 ,j,i,iens) = stateLimits     (l,0,nz,j,i,iens);
          stateLimits     (l,1,nz,j,i,iens) = stateLimits     (l,1,0 ,j,i,iens);
        } else if (bc_z == BC_WALL    ) {
          stateLimits     (l,0,0 ,j,i,iens) = stateLimits     (l,1,0 ,j,i,iens);
          stateLimits     (l,1,nz,j,i,iens) = stateLimits     (l,0,nz,j,i,iens);
        }
      }
      for (int l = 0; l < num_tracers; l++) {
        if        (bc_z == BC_PERIODIC) {
          tracerLimits(l,0,0 ,j,i,iens) = tracerLimits(l,0,nz,j,i,iens);
          tracerLimits(l,1,nz,j,i,iens) = tracerLimits(l,1,0 ,j,i,iens);
        } else if (bc_z == BC_WALL    ) {
          tracerLimits(l,0,0 ,j,i,iens) = tracerLimits(l,1,0 ,j,i,iens);
          tracerLimits(l,1,nz,j,i,iens) = tracerLimits(l,0,nz,j,i,iens);
        }
      }
    });

    //////////////////////////////////////////////////////////
    // Compute the upwind fluxes
    //////////////////////////////////////////////////////////
    parallel_for( "Spatial.h Z Riemann" , SimpleBounds<4>(nz+1,ny,nx,nens) ,
                  YAKL_LAMBDA (int k, int j, int i, int iens) {
      // Get left and right state
      real r_L = stateLimits(idR,0,k,j,i,iens)    ;   real r_R = stateLimits(idR,1,k,j,i,iens)    ;
      real u_L = stateLimits(idU,0,k,j,i,iens)/r_L;   real u_R = stateLimits(idU,1,k,j,i,iens)/r_R;
      real v_L = stateLimits(idV,0,k,j,i,iens)/r_L;   real v_R = stateLimits(idV,1,k,j,i,iens)/r_R;
      real w_L = stateLimits(idW,0,k,j,i,iens)/r_L;   real w_R = stateLimits(idW,1,k,j,i,iens)/r_R;
      real t_L = stateLimits(idT,0,k,j,i,iens)/r_L;   real t_R = stateLimits(idT,1,k,j,i,iens)/r_R;
      // Compute average state
      real r = 0.5_fp * (r_L + r_R);
      real u = 0.5_fp * (u_L + u_R);
      real v = 0.5_fp * (v_L + v_R);
      real w = 0.5_fp * (w_L + w_R);
      real t = 0.5_fp * (t_L + t_R);
      real p = C0 * pow(r*t,gamma);
      real cs2 = gamma*p/r;
      real cs  = sqrt(cs2);
      // Get left and right fluxes
      real q1_L = stateLimits(idR,0,k,j,i,iens);   real q1_R = stateLimits(idR,1,k,j,i,iens);
      real q2_L = stateLimits(idU,0,k,j,i,iens);   real q2_R = stateLimits(idU,1,k,j,i,iens);
      real q3_L = stateLimits(idV,0,k,j,i,iens);   real q3_R = stateLimits(idV,1,k,j,i,iens);
      real q4_L = stateLimits(idW,0,k,j,i,iens);   real q4_R = stateLimits(idW,1,k,j,i,iens);
      real q5_L = stateLimits(idT,0,k,j,i,iens);   real q5_R = stateLimits(idT,1,k,j,i,iens);
      // Compute upwind characteristics
      // Waves 1-3, velocity: w
      real w1, w2, w3;
      if (w > 0) {
        w1 = q1_L - q5_L/t;
        w2 = q2_L - u*q5_L/t;
        w3 = q3_L - v*q5_L/t;
      } else {
        w1 = q1_R - q5_R/t;
        w2 = q2_R - u*q5_R/t;
        w3 = q3_R - v*q5_R/t;
      }
      // Wave 5, velocity: w-cs
      real w5 =  w*q1_R/(2*cs) - q4_R/(2*cs) + q5_R/(2*t);
      // Wave 6, velocity: w+cs
      real w6 = -w*q1_L/(2*cs) + q4_L/(2*cs) + q5_L/(2*t);
      // Use right eigenmatrix to compute upwind flux
      real q1 = w1 + w5 + w6;
      real q2 = w2 + u*w5 + u*w6;
      real q3 = w3 + v*w5 + v*w6;
      real q4 = w*w1 + (w-cs)*w5 + (w+cs)*w6;
      real q5 =      t*w5 + t*w6;

      stateLimits(idR,0,k,j,i,iens) = q4;
      stateLimits(idU,0,k,j,i,iens) = q4*q2/q1;
      stateLimits(idV,0,k,j,i,iens) = q4*q3/q1;
      stateLimits(idW,0,k,j,i,iens) = q4*q4/q1 + C0*pow(q5,gamma);
      stateLimits(idT,0,k,j,i,iens) = q4*q5/q1;

      // COMPUTE UPWIND TRACER FLUXES
      // Handle it one tracer at a time
      for (int tr=0; tr < num_tracers; tr++) {
        if (w > 0) {
          tracerLimits(tr,0,k,j,i,iens) = q4 * tracerLimits(tr,0,k,j,i,iens) / r_L;
        } else {
          tracerLimits(tr,0,k,j,i,iens) = q4 * tracerLimits(tr,1,k,j,i,iens) / r_R;
        }
      }
    });

    //////////////////////////////////////////////////////////
    // Limit the tracer fluxes for positivity
    //////////////////////////////////////////////////////////
    real5d fct_mult("fct_mult",num_tracers,nz+1,ny,nx,nens);
    parallel_for( "Spatial.h Z FCT" , SimpleBounds<5>(num_tracers,nz+1,ny,nx,nens) ,
                  YAKL_LAMBDA (int tr, int k, int j, int i, int iens) {
      fct_mult(tr,k,j,i,iens) = 1.;
      if (k == 0 || k == nz) tracerLimits(tr,0,k,j,i,iens) = 0;
      // Solid wall BCs mean w == 0 at boundaries
      if (tracer_pos(tr)) {
        // Compute and apply the flux reduction factor of the upwind cell
        if      (tracerLimits(tr,0,k,j,i,iens) > 0) {
          int ind_k = k-1;
          // upwind is to the left of this interface
          real f1 = min( tracerLimits(tr,0,ind_k  ,j,i,iens) , 0._fp );
          real f2 = max( tracerLimits(tr,0,ind_k+1,j,i,iens) , 0._fp );
          real fluxOut = dt*(f2-f1)/dz(ind_k,iens);
          real dens = state(idR,hs+ind_k,hs+j,hs+i,iens) + hyDensCells(ind_k,iens);
          real mass = tracers(tr,hs+ind_k,hs+j,hs+i,iens) * dens;
          if (fluxOut > 0) {
            fct_mult(tr,k,j,i,iens) = min( 1._fp , mass / fluxOut );
          }
        } else if (tracerLimits(tr,0,k,j,i,iens) < 0) {
          int ind_k = k;
          // upwind is to the right of this interface
          real f1 = min( tracerLimits(tr,0,ind_k  ,j,i,iens) , 0._fp );
          real f2 = max( tracerLimits(tr,0,ind_k+1,j,i,iens) , 0._fp );
          real fluxOut = dt*(f2-f1)/dz(ind_k,iens);
          real dens = state(idR,hs+ind_k,hs+j,hs+i,iens) + hyDensCells(ind_k,iens);
          real mass = tracers(tr,hs+ind_k,hs+j,hs+i,iens) * dens;
          if (fluxOut > 0) {
            fct_mult(tr,k,j,i,iens) = min( 1._fp , mass / fluxOut );
          }
        }
      }
    });

    //////////////////////////////////////////////////////////
    // Compute the tendencies
    //////////////////////////////////////////////////////////
    parallel_for( "Spatial.h Z tendencies" , SimpleBounds<4>(nz,ny,nx,nens) ,
                  YAKL_LAMBDA(int k, int j, int i, int iens) {
      for (int l=0; l < num_state; l++) {
        if (sim2d && l == idV) {
          stateTend(l,k,j,i,iens) = 0;
        } else {
          stateTend(l,k,j,i,iens) += - ( stateLimits(l,0,k+1,j,i,iens) - stateLimits(l,0,k,j,i,iens) ) / dz(k,iens);
        }
      }
      for (int l=0; l < num_tracers; l++) {
        // Compute tracer tendency
        tracerTend(l,k,j,i,iens) = - ( tracerLimits(l,0,k+1,j,i,iens)*fct_mult(l,k+1,j,i,iens) -
                                       tracerLimits(l,0,k  ,j,i,iens)*fct_mult(l,k  ,j,i,iens) ) / dz(k,iens);
        // Multiply density back onto the tracers
        tracers(l,hs+k,hs+j,hs+i,iens) *= (state(idR,hs+k,hs+j,hs+i,iens) + hyDensCells(k,iens));
      }
    });
  }



  const char * getName() { return ""; }



  void output(PamCoupler const &coupler, real etime) {
    YAKL_SCOPE( dx                    , this->dx                   );
    YAKL_SCOPE( dy                    , this->dy                   );
    YAKL_SCOPE( hyDensCells           , this->hyDensCells          );
    YAKL_SCOPE( hyDensThetaCells      , this->hyDensThetaCells     );
    YAKL_SCOPE( hyThetaCells          , this->hyThetaCells         );
    YAKL_SCOPE( hyPressureCells       , this->hyPressureCells      );
    YAKL_SCOPE( gamma                 , this->gamma                );
    YAKL_SCOPE( C0                    , this->C0                   );

    real5d state   = createStateArr();
    real5d tracers = createTracerArr();
    convert_coupler_state_to_dynamics( coupler , state , tracers );

    for (int iens = 0; iens < nens; iens++) {
      std::string fname = out_prefix + std::string("_") + std::to_string(iens) + std::string(".nc");

      yakl::SimpleNetCDF nc;
      int ulIndex = 0; // Unlimited dimension index to place this data at
      // Create or open the file
      if (etime == 0.) {
        nc.create(fname);

        // x-coordinate
        real1d xloc("xloc",nx);
        parallel_for( "Spatial.h output 1" , nx , YAKL_LAMBDA (int i) { xloc(i) = (i+0.5)*dx; });
        nc.write(xloc.createHostCopy(),"x",{"x"});

        // y-coordinate
        real1d yloc("yloc",ny);
        parallel_for( "Spatial.h output 2" , ny , YAKL_LAMBDA (int i) { yloc(i) = (i+0.5)*dy; });
        nc.write(yloc.createHostCopy(),"y",{"y"});

        // z-coordinate
        auto zint = coupler.dm.get<real const,2>("vertical_interface_height");
        real1d zmid("zmid",nz);
        parallel_for( "Spatial.h output 3" , nz , YAKL_LAMBDA (int i) {
          zmid(i) = ( zint(i,iens) + zint(i+1,iens) ) / 2;
        });
        nc.write(zmid.createHostCopy(),"z",{"z"});

        // hydrostatic density, theta, and pressure
        parallel_for( "Spatial.h output 4" , nz , YAKL_LAMBDA (int k) { zmid(k) = hyDensCells(k,iens); });
        nc.write(zmid.createHostCopy(),"hyDens"    ,{"z"});

        parallel_for( "Spatial.h output 5" , nz , YAKL_LAMBDA (int k) { zmid(k) = hyPressureCells(k,iens); });
        nc.write(zmid.createHostCopy(),"hyPressure",{"z"});

        parallel_for( "Spatial.h output 6" , nz , YAKL_LAMBDA (int k) { zmid(k) = hyThetaCells(k,iens); });
        nc.write(zmid.createHostCopy(),"hyTheta"   ,{"z"});

        // Create time variable
        nc.write1(0._fp,"t",0,"t");
      } else {
        nc.open(fname,yakl::NETCDF_MODE_WRITE);
        ulIndex = nc.getDimSize("t");

        // Write the elapsed time
        nc.write1(etime,"t",ulIndex,"t");
      }

      real3d data("data",nz,ny,nx);
      // rho'
      parallel_for( "Spatial.h output 7" , SimpleBounds<3>(nz,ny,nx) , YAKL_LAMBDA (int k, int j, int i) {
        data(k,j,i) = state(idR,hs+k,hs+j,hs+i,iens);
      });
      nc.write1(data.createHostCopy(),"dens_pert",{"z","y","x"},ulIndex,"t");
      // u
      parallel_for( "Spatial.h output 8" , SimpleBounds<3>(nz,ny,nx) , YAKL_LAMBDA (int k, int j, int i) {
        data(k,j,i) = state(idU,hs+k,hs+j,hs+i,iens) / ( state(idR,hs+k,hs+j,hs+i,iens) + hyDensCells(k,iens) );
      });
      nc.write1(data.createHostCopy(),"u",{"z","y","x"},ulIndex,"t");
      // v
      parallel_for( "Spatial.h output 9" , SimpleBounds<3>(nz,ny,nx) , YAKL_LAMBDA (int k, int j, int i) {
        data(k,j,i) = state(idV,hs+k,hs+j,hs+i,iens) / ( state(idR,hs+k,hs+j,hs+i,iens) + hyDensCells(k,iens) );
      });
      nc.write1(data.createHostCopy(),"v",{"z","y","x"},ulIndex,"t");
      // w
      parallel_for( "Spatial.h output 10" , SimpleBounds<3>(nz,ny,nx) , YAKL_LAMBDA (int k, int j, int i) {
        data(k,j,i) = state(idW,hs+k,hs+j,hs+i,iens) / ( state(idR,hs+k,hs+j,hs+i,iens) + hyDensCells(k,iens) );
      });
      nc.write1(data.createHostCopy(),"w",{"z","y","x"},ulIndex,"t");
      // theta'
      parallel_for( "Spatial.h output 11" , SimpleBounds<3>(nz,ny,nx) , YAKL_LAMBDA (int k, int j, int i) {
        real r =   state(idR,hs+k,hs+j,hs+i,iens) + hyDensCells     (k,iens);
        real t = ( state(idT,hs+k,hs+j,hs+i,iens) + hyDensThetaCells(k,iens) ) / r;
        data(k,j,i) = t - hyThetaCells(k,iens);
      });
      nc.write1(data.createHostCopy(),"pot_temp_pert",{"z","y","x"},ulIndex,"t");
      // pressure'
      parallel_for( "Spatial.h output 12" , SimpleBounds<3>(nz,ny,nx) , YAKL_LAMBDA (int k, int j, int i) {
        real r  = state(idR,hs+k,hs+j,hs+i,iens) + hyDensCells(k,iens);
        real rt = state(idT,hs+k,hs+j,hs+i,iens) + hyDensThetaCells(k,iens);
        real p  = C0*pow(rt,gamma);
        data(k,j,i) = p - hyPressureCells(k,iens);
      });
      nc.write1(data.createHostCopy(),"pressure_pert",{"z","y","x"},ulIndex,"t");

      for (int tr=0; tr < num_tracers; tr++) {
        parallel_for( "Spatial.h output 13" , SimpleBounds<3>(nz,ny,nx) , YAKL_LAMBDA (int k, int j, int i) {
          real r = state(idR,hs+k,hs+j,hs+i,iens) + hyDensCells(k,iens);
          data(k,j,i) = tracers(tr,hs+k,hs+j,hs+i,iens)/r;
        });
        nc.write1(data.createHostCopy(),std::string("tracer_")+tracer_name[tr],{"z","y","x"},ulIndex,"t");
      }

      // Close the file
      nc.close();

    }
  }



  void finalize(real4d const &state , real4d const &tracers) {}



  // ord stencil values to ngll GLL values; store in DTs
  YAKL_INLINE static void reconstruct_gll_values( SArray<real,1,ord> const stencil                      ,
                                                  SArray<real,1,ngll> &gll                              ,
                                                  SArray<real,2,ord,ngll> const &coefs_to_gll           ,
                                                  SArray<real,2,ord,ngll> const &sten_to_gll            ,
                                                  SArray<real,2,ord,ord>  const &sten_to_coefs          ,
                                                  SArray<real,3,hs+1,hs+1,hs+1> const &weno_recon_lower ,
                                                  SArray<real,1,hs+2> const &idl                        ,
                                                  real sigma, bool doweno ) {
    if (doweno) {

      // Reconstruct values
      SArray<real,1,ord> wenoCoefs;
      weno::compute_weno_coefs<ord>( weno_recon_lower , sten_to_coefs , stencil , wenoCoefs , idl , sigma );
      // Transform ord weno coefficients into ngll GLL points
      for (int ii=0; ii<ngll; ii++) {
        real tmp = 0;
        for (int s=0; s < ord; s++) {
          tmp += coefs_to_gll(s,ii) * wenoCoefs(s);
        }
        gll(ii) = tmp;
      }

    } else {

      // Transform ord stencil cell averages into ngll GLL points
      for (int ii=0; ii<ngll; ii++) {
        real tmp = 0;
        for (int s=0; s < ord; s++) {
          tmp += sten_to_gll(s,ii) * stencil(s);
        }
        gll(ii) = tmp;
      }

    } // if doweno
  }



  YAKL_INLINE static void diffTransformEulerConsX( SArray<real,2,nAder,ngll> &r  ,
                                                   SArray<real,2,nAder,ngll> &ru ,
                                                   SArray<real,2,nAder,ngll> &rv ,
                                                   SArray<real,2,nAder,ngll> &rw ,
                                                   SArray<real,2,nAder,ngll> &rt ,
                                                   SArray<real,2,nAder,ngll> &ruu ,
                                                   SArray<real,2,nAder,ngll> &ruv ,
                                                   SArray<real,2,nAder,ngll> &ruw ,
                                                   SArray<real,2,nAder,ngll> &rut ,
                                                   SArray<real,2,nAder,ngll> &rt_gamma ,
                                                   SArray<real,2,ngll,ngll> const &deriv ,
                                                   real C0, real gamma, real dx ) {
    // zero out the non-linear DTs
    for (int kt=1; kt < nAder; kt++) {
      for (int ii=0; ii < ngll; ii++) {
        ruu     (kt,ii) = 0;
        ruv     (kt,ii) = 0;
        ruw     (kt,ii) = 0;
        rut     (kt,ii) = 0;
        rt_gamma(kt,ii) = 0;
      }
    }

    // Loop over the time derivatives
    for (int kt=0; kt<nAder-1; kt++) {
      // Compute the state at the next time level
      for (int ii=0; ii<ngll; ii++) {
        real df1_dx = 0;
        real df2_dx = 0;
        real df3_dx = 0;
        real df4_dx = 0;
        real df5_dx = 0;
        for (int s=0; s<ngll; s++) {
          df1_dx += deriv(s,ii) * ( ru (kt,s) );
          if (kt == 0) { df2_dx += deriv(s,ii) * ( ruu(kt,s) + C0*rt_gamma(kt,s)   ); }
          else         { df2_dx += deriv(s,ii) * ( ruu(kt,s) + C0*rt_gamma(kt,s)/2 ); }
          df3_dx += deriv(s,ii) * ( ruv(kt,s) );
          df4_dx += deriv(s,ii) * ( ruw(kt,s) );
          df5_dx += deriv(s,ii) * ( rut(kt,s) );
        }
        r (kt+1,ii) = -df1_dx/dx/(kt+1._fp);
        ru(kt+1,ii) = -df2_dx/dx/(kt+1._fp);
        rv(kt+1,ii) = -df3_dx/dx/(kt+1._fp);
        rw(kt+1,ii) = -df4_dx/dx/(kt+1._fp);
        rt(kt+1,ii) = -df5_dx/dx/(kt+1._fp);
      }

      // Compute ru* at the next time level
      for (int ii=0; ii<ngll; ii++) {
        real tot_ruu = 0;
        real tot_ruv = 0;
        real tot_ruw = 0;
        real tot_rut = 0;
        for (int ir=0; ir<=kt+1; ir++) {
          tot_ruu += ru(ir,ii) * ru(kt+1-ir,ii) - r(ir,ii) * ruu(kt+1-ir,ii);
          tot_ruv += ru(ir,ii) * rv(kt+1-ir,ii) - r(ir,ii) * ruv(kt+1-ir,ii);
          tot_ruw += ru(ir,ii) * rw(kt+1-ir,ii) - r(ir,ii) * ruw(kt+1-ir,ii);
          tot_rut += ru(ir,ii) * rt(kt+1-ir,ii) - r(ir,ii) * rut(kt+1-ir,ii);
        }
        ruu(kt+1,ii) = tot_ruu / r(0,ii);
        ruv(kt+1,ii) = tot_ruv / r(0,ii);
        ruw(kt+1,ii) = tot_ruw / r(0,ii);
        rut(kt+1,ii) = tot_rut / r(0,ii);

        // Compute rt_gamma at the next time level
        real tot_rt_gamma = 0;
        for (int ir=0; ir<=kt; ir++) {
          tot_rt_gamma += (kt+1._fp -ir) * ( gamma*rt_gamma(ir,ii)*rt(kt+1-ir,ii) - rt(ir,ii)*rt_gamma(kt+1-ir,ii) );
        }
        rt_gamma(kt+1,ii) = ( gamma*rt_gamma(0,ii)*rt(kt+1,ii) + tot_rt_gamma / (kt+1._fp) ) / rt(0,ii);
      }
    }

    // Fix the rt_gamma
    for (int kt=1; kt < nAder; kt++) {
      for (int ii=0; ii < ngll; ii++) {
        rt_gamma(kt,ii) /= 2;
      }
    }
  }



  YAKL_INLINE static void diffTransformEulerConsY( SArray<real,2,nAder,ngll> &r  ,
                                                   SArray<real,2,nAder,ngll> &ru ,
                                                   SArray<real,2,nAder,ngll> &rv ,
                                                   SArray<real,2,nAder,ngll> &rw ,
                                                   SArray<real,2,nAder,ngll> &rt ,
                                                   SArray<real,2,nAder,ngll> &rvu ,
                                                   SArray<real,2,nAder,ngll> &rvv ,
                                                   SArray<real,2,nAder,ngll> &rvw ,
                                                   SArray<real,2,nAder,ngll> &rvt ,
                                                   SArray<real,2,nAder,ngll> &rt_gamma ,
                                                   SArray<real,2,ngll,ngll> const &deriv ,
                                                   real C0, real gamma, real dy ) {
    // zero out the non-linear DTs
    for (int kt=1; kt < nAder; kt++) {
      for (int ii=0; ii < ngll; ii++) {
        rvu     (kt,ii) = 0;
        rvv     (kt,ii) = 0;
        rvw     (kt,ii) = 0;
        rvt     (kt,ii) = 0;
        rt_gamma(kt,ii) = 0;
      }
    }

    // Loop over the time derivatives
    for (int kt=0; kt<nAder-1; kt++) {
      // Compute the state at the next time level
      for (int ii=0; ii<ngll; ii++) {
        real drv_dy    = 0;
        real drvu_dy   = 0;
        real drvv_p_dy = 0;
        real drvw_dy   = 0;
        real drvt_dy   = 0;
        for (int s=0; s<ngll; s++) {
          drv_dy    += deriv(s,ii) * rv(kt,s);
          drvu_dy   += deriv(s,ii) * rvu(kt,s);
          if (kt == 0) { drvv_p_dy += deriv(s,ii) * ( rvv(kt,s) + C0*rt_gamma(kt,s)   ); }
          else         { drvv_p_dy += deriv(s,ii) * ( rvv(kt,s) + C0*rt_gamma(kt,s)/2 ); }
          drvw_dy   += deriv(s,ii) * rvw(kt,s);
          drvt_dy   += deriv(s,ii) * rvt(kt,s);
        }
        r (kt+1,ii) = -drv_dy   /dy/(kt+1);
        ru(kt+1,ii) = -drvu_dy  /dy/(kt+1);
        rv(kt+1,ii) = -drvv_p_dy/dy/(kt+1);
        rw(kt+1,ii) = -drvw_dy  /dy/(kt+1);
        rt(kt+1,ii) = -drvt_dy  /dy/(kt+1);
      }

      // Compute ru* at the next time level
      for (int ii=0; ii<ngll; ii++) {
        // Compute the non-linear differential transforms
        real tot_rvu = 0;
        real tot_rvv = 0;
        real tot_rvw = 0;
        real tot_rvt = 0;
        for (int l=0; l<=kt+1; l++) {
          tot_rvu += rv(l,ii) * ru(kt+1-l,ii) - r(l,ii) * rvu(kt+1-l,ii);
          tot_rvv += rv(l,ii) * rv(kt+1-l,ii) - r(l,ii) * rvv(kt+1-l,ii);
          tot_rvw += rv(l,ii) * rw(kt+1-l,ii) - r(l,ii) * rvw(kt+1-l,ii);
          tot_rvt += rv(l,ii) * rt(kt+1-l,ii) - r(l,ii) * rvt(kt+1-l,ii);
        }
        rvu(kt+1,ii) = tot_rvu / r(0,ii);
        rvv(kt+1,ii) = tot_rvv / r(0,ii);
        rvw(kt+1,ii) = tot_rvw / r(0,ii);
        rvt(kt+1,ii) = tot_rvt / r(0,ii);

        // Compute rt_gamma at the next time level
        real tot_rt_gamma = 0;
        for (int l=0; l<=kt; l++) {
          tot_rt_gamma += (kt+1._fp -l) * ( gamma*rt_gamma(l,ii)*rt(kt+1-l,ii) - rt(l,ii)*rt_gamma(kt+1-l,ii) );
        }
        rt_gamma(kt+1,ii) = ( gamma*rt_gamma(0,ii)*rt(kt+1,ii) + tot_rt_gamma / (kt+1._fp) ) / rt(0,ii);
      }
    }

    // Fix the rt_gamma
    for (int kt=1; kt < nAder; kt++) {
      for (int ii=0; ii < ngll; ii++) {
        rt_gamma(kt,ii) /= 2;
      }
    }
  }



  YAKL_INLINE static void diffTransformEulerConsZ( SArray<real,2,nAder,ngll> &r  ,
                                                   SArray<real,2,nAder,ngll> &ru ,
                                                   SArray<real,2,nAder,ngll> &rv ,
                                                   SArray<real,2,nAder,ngll> &rw ,
                                                   SArray<real,2,nAder,ngll> &rt ,
                                                   SArray<real,2,nAder,ngll> &rwu ,
                                                   SArray<real,2,nAder,ngll> &rwv ,
                                                   SArray<real,2,nAder,ngll> &rww ,
                                                   SArray<real,2,nAder,ngll> &rwt ,
                                                   SArray<real,2,nAder,ngll> &rt_gamma ,
                                                   SArray<real,2,ngll,ngll> const &deriv ,
                                                   real3d const &hyPressureGLL ,
                                                   real C0, real gamma ,
                                                   int k , real dz , int bc_z , int nz , int iens ) {
    // zero out the non-linear DTs
    for (int kt=1; kt < nAder; kt++) {
      for (int ii=0; ii < ngll; ii++) {
        rwu     (kt,ii) = 0;
        rwv     (kt,ii) = 0;
        rww     (kt,ii) = 0;
        rwt     (kt,ii) = 0;
        rt_gamma(kt,ii) = 0;
      }
    }

    // Loop over the time derivatives
    for (int kt=0; kt<nAder-1; kt++) {
      // Compute the state at the next time level
      for (int ii=0; ii<ngll; ii++) {
        real drw_dz    = 0;
        real drwu_dz   = 0;
        real drwv_dz   = 0;
        real drww_p_dz = 0;
        real drwt_dz   = 0;
        for (int s=0; s<ngll; s++) {
          drw_dz    += deriv(s,ii) * rw(kt,s);
          drwu_dz   += deriv(s,ii) * rwu(kt,s);
          drwv_dz   += deriv(s,ii) * rwv(kt,s);
          if (kt == 0) { drww_p_dz += deriv(s,ii) * ( rww(kt,s) + C0*rt_gamma(kt,s) - hyPressureGLL(k,s,iens) ); }
          else         { drww_p_dz += deriv(s,ii) * ( rww(kt,s) + C0*rt_gamma(kt,s)/2                         ); }
          drwt_dz   += deriv(s,ii) * rwt(kt,s);
        }
        r (kt+1,ii) = -drw_dz   /dz/(kt+1);
        ru(kt+1,ii) = -drwu_dz  /dz/(kt+1);
        rv(kt+1,ii) = -drwv_dz  /dz/(kt+1);
        rw(kt+1,ii) = -drww_p_dz/dz/(kt+1);
        rt(kt+1,ii) = -drwt_dz  /dz/(kt+1);

        if (bc_z == BC_WALL) {
          if (k == nz-1) rw(kt+1,ngll-1) = 0;
          if (k == 0   ) rw(kt+1,0     ) = 0;
        }
      }

      // Compute ru* at the next time level
      for (int ii=0; ii<ngll; ii++) {
        // Compute the non-linear differential transforms
        real tot_rwu = 0;
        real tot_rwv = 0;
        real tot_rww = 0;
        real tot_rwt = 0;
        for (int l=0; l<=kt+1; l++) {
          tot_rwu += rw(l,ii) * ru(kt+1-l,ii) - r(l,ii) * rwu(kt+1-l,ii);
          tot_rwv += rw(l,ii) * rv(kt+1-l,ii) - r(l,ii) * rwv(kt+1-l,ii);
          tot_rww += rw(l,ii) * rw(kt+1-l,ii) - r(l,ii) * rww(kt+1-l,ii);
          tot_rwt += rw(l,ii) * rt(kt+1-l,ii) - r(l,ii) * rwt(kt+1-l,ii);
        }
        rwu(kt+1,ii) = tot_rwu / r(0,ii);
        rwv(kt+1,ii) = tot_rwv / r(0,ii);
        rww(kt+1,ii) = tot_rww / r(0,ii);
        rwt(kt+1,ii) = tot_rwt / r(0,ii);

        // Compute rt_gamma at the next time level
        real tot_rt_gamma = 0;
        for (int l=0; l<=kt; l++) {
          tot_rt_gamma += (kt+1-l) * ( gamma*rt_gamma(l,ii)*rt(kt+1-l,ii) - rt(l,ii)*rt_gamma(kt+1-l,ii) );
        }
        rt_gamma(kt+1,ii) = ( gamma*rt_gamma(0,ii)*rt(kt+1,ii) + tot_rt_gamma / (kt+1) ) / rt(0,ii);
      }
    }

    // Fix the rt_gamma
    for (int kt=1; kt < nAder; kt++) {
      for (int ii=0; ii < ngll; ii++) {
        rt_gamma(kt,ii) /= 2;
      }
    }
  }



  YAKL_INLINE static void diffTransformTracer( SArray<real,2,nAder,ngll> const &r  ,
                                               SArray<real,2,nAder,ngll> const &ru ,
                                               SArray<real,2,nAder,ngll> &rt ,
                                               SArray<real,2,nAder,ngll> &rut ,
                                               SArray<real,2,ngll,ngll> const &deriv ,
                                               real dx ) {
    // zero out the non-linear DT
    for (int kt=1; kt < nAder; kt++) {
      for (int ii=0; ii < ngll; ii++) {
        rut(kt,ii) = 0;
      }
    }
    // Loop over the time derivatives
    for (int kt=0; kt<nAder-1; kt++) {
      // Compute the rho*tracer at the next time level
      for (int ii=0; ii<ngll; ii++) {
        real df_dx = 0;
        for (int s=0; s<ngll; s++) {
          df_dx += deriv(s,ii) * rut(kt,s);
        }
        rt(kt+1,ii) = -df_dx/dx/(kt+1._fp);
      }
      // Compute rut at the next time level
      for (int ii=0; ii<ngll; ii++) {
        real tot_rut = 0;
        for (int ir=0; ir<=kt+1; ir++) {
          tot_rut += ru(ir,ii) * rt(kt+1-ir,ii) - r(ir,ii) * rut(kt+1-ir,ii);
        }
        rut(kt+1,ii) = tot_rut / r(0,ii);
      }
    }
  }



  YAKL_INLINE static void compute_timeAvg( SArray<real,3,num_state,nAder,ngll> &dts , real dt ) {
    real dtmult = dt;
    for (int kt=1; kt<nAder; kt++) {
      for (int l=0; l<num_state; l++) {
        for (int ii=0; ii<ngll; ii++) {
          dts(l,0,ii) += dts(l,kt,ii) * dtmult / (kt+1._fp);
        }
      }
      dtmult *= dt;
    }
  }



  YAKL_INLINE static void compute_timeAvg( SArray<real,2,nAder,ngll> &dts , real dt ) {
    real dtmult = dt;
    for (int kt=1; kt<nAder; kt++) {
      for (int ii=0; ii<ngll; ii++) {
        dts(0,ii) += dts(kt,ii) * dtmult / (kt+1._fp);
      }
      dtmult *= dt;
    }
  }



  YAKL_INLINE static void compute_timeAvg( SArray<real,2,nAder,ngll> const &dts , SArray<real,1,ngll> &tavg ,
                                           real dt ) {
    for (int ii=0; ii<ngll; ii++) {
      tavg(ii) = dts(0,ii);
    }
    real dtmult = dt;
    for (int kt=1; kt<nAder; kt++) {
      for (int ii=0; ii<ngll; ii++) {
        tavg(ii) += dts(kt,ii) * dtmult / (kt+1._fp);
      }
      dtmult *= dt;
    }
  }


};

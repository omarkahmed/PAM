
#pragma once

#include "pam_coupler.h"
#include "Sam1mom.h"

extern "C"
void sam1mom_main_fortran(double &dt, int &ncol, int &nz, double *zint, double *rho, double *rhow, double *pres, 
                          double *tabs, double *qv, double *qn, double *qp);

int static constexpr num_tracers_micro = 3;

class Microphysics {
public:
  // Doesn't actually have to be static or constexpr. Could be assigned in the constructor
  int static constexpr num_tracers = 3;

  // You should set these in the constructor
  real R_d    ;
  real cp_d   ;
  real cv_d   ;
  real gamma_d;
  real kappa_d;
  real R_v    ;
  real cp_v   ;
  real cv_v   ;
  real p0     ;
  real grav   ;

  real cp_l;

  bool first_step;

  // Indices for all of your tracer quantities
  int static constexpr ID_V  = 0;  // Local index for Water Vapor       
  int static constexpr ID_N  = 1;  // Local index for Total Cloud Condensage (liquid + ice)
  int static constexpr ID_P  = 2;  // Local index for Total Precip



  // TODO: Make sure the constants vibe with sam1mom
  // Set constants and likely num_tracers as well, and anything else you can do immediately
  Microphysics() {
    R_d        = 287.;
    cp_d       = 1004.;
    cv_d       = cp_d - R_d;
    gamma_d    = cp_d / cv_d;
    kappa_d    = R_d  / cp_d;
    R_v        = 461.;
    cp_v       = 1859;
    cv_v       = R_v - cp_v;
    p0         = 1.e5;
    grav       = 9.81;
    cp_l       = 4218.;
    first_step = true;
  }



  // This must return the correct # of tracers **BEFORE** init(...) is called
  YAKL_INLINE static int get_num_tracers() {
    return num_tracers;
  }



  // Can do whatever you want, but mainly for registering tracers and allocating data
  void init(pam::PamCoupler &coupler) {
    using yakl::c::parallel_for;
    using yakl::c::SimpleBounds;

    int nx   = coupler.get_nx  ();
    int ny   = coupler.get_ny  ();
    int nz   = coupler.get_nz  ();
    int nens = coupler.get_nens();

    // Register tracers in the coupler
    //                 name            description                positive   adds mass
    coupler.add_tracer("water_vapor" , "Water Vapor"            , true     , true );
    coupler.add_tracer("cloud_cond"  , "Total Cloud Condensate" , true     , true );
    coupler.add_tracer("precip"      , "Total Precip"           , true     , true );

    auto &dm = coupler.get_data_manager_readwrite();

    auto water_vapor = dm.get_collapsed<real>( "water_vapor" );
    auto cloud_cond  = dm.get_collapsed<real>( "cloud_cond"  );
    auto precip      = dm.get_collapsed<real>( "precip"      );

    parallel_for( "micro zero" , nz*ny*nx*nens , YAKL_LAMBDA (int i) {
      water_vapor(i) = 0;
      cloud_cond (i) = 0;
      precip     (i) = 0;
    });

    coupler.set_option<std::string>("micro","sam1mom");
  }



  void timeStep( pam::PamCoupler &coupler , real dt ) const {
    using yakl::c::parallel_for;
    using yakl::c::SimpleBounds;

    auto &dm = coupler.get_data_manager_readwrite();

    // Get the dimensions sizes
    int nz   = dm.get_dimension_size("z"   );
    int ny   = dm.get_dimension_size("y"   );
    int nx   = dm.get_dimension_size("x"   );
    int nens = dm.get_dimension_size("nens");
    int ncol = ny*nx*nens;

    // Get tracers dimensioned as (nz,ny*nx*nens)
    auto rho_v = dm.get_lev_col<real>("water_vapor");
    auto rho_n = dm.get_lev_col<real>("cloud_cond" );
    auto rho_p = dm.get_lev_col<real>("precip"     );

    // Get coupler state
    auto rho_dry = dm.get_lev_col<real const>("density_dry");
    auto temp    = dm.get_lev_col<real      >("temp");

    // Calculate the grid spacing
    auto zint_in = dm.get<real const,2>("vertical_interface_height");
    real2d zint("zint",nz+1,ny*nx*nens);
    parallel_for( "micro dz" , SimpleBounds<4>(nz+1,ny,nx,nens) , YAKL_LAMBDA (int k, int j, int i, int iens) {
      zint(k,j*nx*nens + i*nens + iens) = zint_in(k,iens);
    });

    // These are inputs to sam1mom
    real2d qv         ("qv"         ,nz,ncol);
    real2d qn         ("qn"         ,nz,ncol);
    real2d qp         ("qp"         ,nz,ncol);
    real2d density    ("density"    ,nz,ncol);
    real2d pressure   ("pressure"   ,nz,ncol);

    //////////////////////////////////////////////////////////////////////////////
    // Compute quantities needed for inputs to sam1mom
    //////////////////////////////////////////////////////////////////////////////
    // Force constants into local scope
    real gamma_d = this->gamma_d;
    real R_d     = this->R_d;
    real R_v     = this->R_v;
    real cp_d    = this->cp_d;
    real cp_v    = this->cp_v;
    real cp_l    = this->cp_l;
    real p0      = this->p0;

    // Save initial state, and compute inputs for sam1mom
    parallel_for( "micro adjust preprocess" , SimpleBounds<2>(nz,ncol) , YAKL_LAMBDA (int k, int i) {
      // Compute total density
      density(k,i) = rho_dry(k,i) + rho_v(k,i) + rho_n(k,i) + rho_p(k,i);

      // Compute quantities for sam1mom
      qv      (k,i) = rho_v(k,i) / density(k,i);
      qn      (k,i) = rho_n(k,i) / density(k,i);
      qp      (k,i) = rho_p(k,i) / density(k,i);
      pressure(k,i) = ( R_d * rho_dry(k,i) * temp(k,i) + R_v * rho_v(k,i) * temp(k,i) ) / 100.;
    });

    auto density_int = coupler.interp_density_interfaces( density.reshape<4>({nz,ny,nx,nens}) ).reshape<2>({nz+1,ncol});

    #ifdef MICRO_DUMP
      // Valid for 2-D runs with nens=1 only
      yakl::SimpleNetCDF nc;
      int unlim_ind = 0;
      if (first_step) {
        nc.create( "sam1mom_data.nc" );
        nc.write( dt , "dt" );
        first_step = false;
      } else {
        nc.open( "sam1mom_data.nc" , yakl::NETCDF_MODE_WRITE );
        unlim_ind = nc.getDimSize( "num_samples" );
      }
      real1d data1("data1",nz  );
      real1d data2("data2",nz+1);
      parallel_for( nz   , YAKL_LAMBDA (int k) { data1(k) = qv         (k,nx/2); });    nc.write1( data1 , "qv"          , {"z"  } , unlim_ind , "num_samples" );
      parallel_for( nz   , YAKL_LAMBDA (int k) { data1(k) = qn         (k,nx/2); });    nc.write1( data1 , "qn"          , {"z"  } , unlim_ind , "num_samples" );
      parallel_for( nz   , YAKL_LAMBDA (int k) { data1(k) = qp         (k,nx/2); });    nc.write1( data1 , "qp"          , {"z"  } , unlim_ind , "num_samples" );
      parallel_for( nz+1 , YAKL_LAMBDA (int k) { data2(k) = zint       (k,nx/2); });    nc.write1( data2 , "zint"        , {"zp1"} , unlim_ind , "num_samples" );
      parallel_for( nz   , YAKL_LAMBDA (int k) { data1(k) = pressure   (k,nx/2); });    nc.write1( data1 , "pressure"    , {"z"  } , unlim_ind , "num_samples" );
      parallel_for( nz   , YAKL_LAMBDA (int k) { data1(k) = temp       (k,nx/2); });    nc.write1( data1 , "temp"        , {"z"  } , unlim_ind , "num_samples" );
      parallel_for( nz   , YAKL_LAMBDA (int k) { data1(k) = density    (k,nx/2); });    nc.write1( data1 , "density"     , {"z"  } , unlim_ind , "num_samples" );
      parallel_for( nz+1 , YAKL_LAMBDA (int k) { data2(k) = density_int(k,nx/2); });    nc.write1( data2 , "density_int" , {"zp1"} , unlim_ind , "num_samples" );
      nc.close();
    #endif

    // Convert C-style to Fortran-style
    F_real2d qv_fortran          = F_real2d("qv         ",qv         .data(),ncol,nz  );
    F_real2d qn_fortran          = F_real2d("qn         ",qn         .data(),ncol,nz  );
    F_real2d qp_fortran          = F_real2d("qp         ",qp         .data(),ncol,nz  );
    F_real2d zint_fortran        = F_real2d("zint       ",zint       .data(),ncol,nz+1);
    F_real2d pressure_fortran    = F_real2d("pressure   ",pressure   .data(),ncol,nz  );
    F_real2d temp_fortran        = F_real2d("temp       ",temp       .data(),ncol,nz  );
    F_real2d density_fortran     = F_real2d("density    ",density    .data(),ncol,nz  );
    F_real2d density_int_fortran = F_real2d("density_int",density_int.data(),ncol,nz+1);

    sam1mom::Sam1mom micro;
    micro.main( dt , zint_fortran , density_fortran , density_int_fortran , pressure_fortran , temp_fortran , qv_fortran , qn_fortran , qp_fortran );
                    
    ///////////////////////////////////////////////////////////////////////////////
    // Convert sam1mom outputs into dynamics coupler state and tracer masses
    ///////////////////////////////////////////////////////////////////////////////
    parallel_for( "micro post process" , SimpleBounds<2>(nz,ncol) , YAKL_LAMBDA (int k, int i) {
      rho_v(k,i) = max( qv(k,i)*density(k,i) , 0._fp );
      rho_n(k,i) = max( qn(k,i)*density(k,i) , 0._fp );
      rho_p(k,i) = max( qp(k,i)*density(k,i) , 0._fp );
    });

  }



  std::string micro_name() const {
    return "sam1mom";
  }



};





#pragma once

#include "pam_const.h"
#include "DataManager.h"
#include "vertical_interp.h"
//#include "YAKL_netcdf.h"
#include "Options.h"


namespace pam {



  YAKL_INLINE real hydrostatic_pressure( realConst3d hy_params , real z_in , real z0 , real dz ,
                                         int k, int iens             ) {
    real z = ( z_in - z0 ) / dz;
    real a0 = hy_params(k,0,iens);
    real a1 = hy_params(k,1,iens);
    real a2 = hy_params(k,2,iens);
    real a3 = hy_params(k,3,iens);
    real a4 = hy_params(k,4,iens);
    real lnp = a0 + ( a1 + ( a2 + ( a3 + ( a4)*z)*z)*z)*z;
    return exp(lnp);
  }



  YAKL_INLINE real hydrostatic_density( realConst3d hy_params , real z_in , real z0 , real dz ,
                                        int k, int iens , real grav ) {
    real z = ( z_in - z0 ) / dz;
    real a1 = hy_params(k,1,iens);
    real a2 = hy_params(k,2,iens);
    real a3 = hy_params(k,3,iens);
    real a4 = hy_params(k,4,iens);
    real p = hydrostatic_pressure( hy_params , z_in , z0 , dz , k , iens );
    real mult = a1 + (2*a2 + (3*a3 + (4*a4)*z)*z)*z;
    real dpdz = mult*p/dz;
    return -dpdz/grav;
  }



  YAKL_INLINE real compute_pressure( real rho_d, real rho_v, real T, real R_d, real R_v ) {
    return rho_d*R_d*T + rho_v*R_v*T;
  }



  class PamCoupler {
  protected:

    std::thread::id thread_id;

    Options options;

    real xlen;   // Domain length in the x-direction in meters
    real ylen;   // Domain length in the y-direction in meters

    DataManager     dm;
    DataManagerHost dm_host;

    struct Tracer {
      std::string name;
      std::string desc;
      bool        positive;
      bool        adds_mass;
    };
    std::vector<Tracer> tracers;


  public:

    PamCoupler() {
      this->xlen   = -1;
      this->ylen   = -1;
      this->thread_id = std::this_thread::get_id();
    }


    PamCoupler(PamCoupler &&) = default;
    PamCoupler &operator=(PamCoupler &&) = default;
    PamCoupler(PamCoupler const &) = delete;
    PamCoupler &operator=(PamCoupler const &) = delete;


    ~PamCoupler() {
      dm.finalize();
      options.finalize();
      tracers = std::vector<Tracer>();
      this->xlen   = -1;
      this->ylen   = -1;
    }


    std::thread::id         get_thread_id                    () const { return this->thread_id    ; }
    real                    get_xlen                         () const { return this->xlen         ; }
    real                    get_ylen                         () const { return this->ylen         ; }
    real                    get_dx                           () const { return get_xlen()/get_nx(); }
    real                    get_dy                           () const { return get_ylen()/get_ny(); }
    DataManager const &     get_data_manager_device_readonly () const { return this->dm           ; }
    DataManager       &     get_data_manager_device_readwrite()       { return this->dm           ; }
    DataManagerHost const & get_data_manager_host_readonly   () const { return this->dm_host      ; }
    DataManagerHost       & get_data_manager_host_readwrite  ()       { return this->dm_host      ; }


    int get_nx() const {
      if (dm.find_dimension("x") == -1) return -1;
      return dm.get_dimension_size("x");
    }


    int get_ny() const {
      if (dm.find_dimension("y") == -1) return -1;
      return dm.get_dimension_size("y");
    }


    int get_nz() const {
      if (dm.find_dimension("z") == -1) return -1;
      return dm.get_dimension_size("z");
    }


    int get_nens() const {
      if (dm.find_dimension("nens") == -1) return -1;
      return dm.get_dimension_size("nens");
    }


    int get_ncrms() const {
      return get_nens();
    }


    template <class T>
    void add_option( std::string key , T value ) {
      options.add_option<T>(key,value);
    }


    template <class T>
    void set_option( std::string key , T value ) {
      options.set_option<T>(key,value);
    }


    template <class T>
    T get_option( std::string key ) const {
      return options.get_option<T>(key);
    }


    bool option_exists( std::string key ) const {
      return options.option_exists(key);
    }


    void delete_option( std::string key ) {
      options.delete_option(key);
    }


    void make_option_readonly( std::string key ) {
      options.make_readonly(key);
    }


    template <class F>
    void run_module( std::string name , F const &f ) {
      #ifdef PAM_FUNCTION_TRACE
        dm.clean_all_entries();
      #endif
      #ifdef PAM_FUNCTION_TIMERS
        yakl::timer_start( name.c_str() );
      #endif
      f( *this );
      #ifdef PAM_FUNCTION_TIMERS
        yakl::timer_stop ( name.c_str() );
      #endif
      #ifdef PAM_FUNCTION_TRACE
        auto dirty_entry_names = dm.get_dirty_entries();
        std::cout << "MMF Module " << name << " wrote to the following coupler entries: ";
        for (int e=0; e < dirty_entry_names.size(); e++) {
          std::cout << dirty_entry_names[e];
          if (e < dirty_entry_names.size()-1) std::cout << ", ";
        }
        std::cout << "\n\n";
      #endif
    }


    void set_grid(real xlen, real ylen, realConst2d zint_in) {
      using yakl::c::parallel_for;
      using yakl::c::SimpleBounds;

      int nz    = get_nz();
      int nens  = get_nens();
      this->xlen = xlen;
      this->ylen = ylen;
      auto zint = dm.get<real,2>("vertical_interface_height");
      auto dz   = dm.get<real,2>("vertical_cell_dz");
      auto zmid = dm.get<real,2>("vertical_midpoint_height" );
      parallel_for( "vert grid 1" , SimpleBounds<2>(nz+1,nens) , YAKL_LAMBDA (int k, int iens) {
        zint(k,iens) = zint_in(k,iens);
        if (k < nz) {
          zmid(k,iens) = 0.5_fp * (zint_in(k,iens) + zint_in(k+1,iens));
          dz  (k,iens) = zint_in(k+1,iens) - zint_in(k,iens);
        }
      });
    }


    void set_grid(real xlen, real ylen, realConst1d zint_in) {
      using yakl::c::parallel_for;
      using yakl::c::SimpleBounds;

      int nz    = get_nz();
      int nens  = get_nens();
      this->xlen = xlen;
      this->ylen = ylen;
      auto zint = dm.get<real,2>("vertical_interface_height");
      auto dz   = dm.get<real,2>("vertical_cell_dz");
      auto zmid = dm.get<real,2>("vertical_midpoint_height" );
      parallel_for( "vert grid 2" , SimpleBounds<2>(nz+1,nens) , YAKL_LAMBDA (int k, int iens) {
        zint(k,iens) = zint_in(k);
        if (k < nz) {
          zmid(k,iens) = 0.5_fp * (zint_in(k) + zint_in(k+1));
          dz  (k,iens) = zint_in(k+1) - zint_in(k);
        }
      });
    }


    
    void add_tracer( std::string tracer_name , std::string tracer_desc , bool positive , bool adds_mass ) {
      int nz   = get_nz  ();
      int ny   = get_ny  ();
      int nx   = get_nx  ();
      int nens = get_nens();
      dm.register_and_allocate<real>( tracer_name , tracer_desc , {nz,ny,nx,nens} , {"z","y","x","nens"} );
      tracers.push_back( { tracer_name , tracer_desc , positive , adds_mass } );
    }



    int get_num_tracers() const { return tracers.size(); }


    
    std::vector<std::string> get_tracer_names() const {
      std::vector<std::string> ret;
      for (int i=0; i < tracers.size(); i++) { ret.push_back( tracers[i].name ); }
      return ret;
    }


    
    void get_tracer_info(std::string tracer_name , std::string &tracer_desc, bool &tracer_found ,
                         bool &positive , bool &adds_mass) const {
      std::vector<std::string> ret;
      for (int i=0; i < tracers.size(); i++) {
        if (tracer_name == tracers[i].name) {
          positive     = tracers[i].positive ;
          tracer_desc  = tracers[i].desc     ;
          adds_mass    = tracers[i].adds_mass;
          tracer_found = true;
          return;
        }
      }
      tracer_found = false;
    }


    
    bool tracer_exists( std::string tracer_name ) const {
      for (int i=0; i < tracers.size(); i++) {
        if (tracer_name == tracers[i].name) return true;
      }
      return false;
    }



    void allocate_coupler_state( int nz, int ny, int nx, int nens ) {
      using yakl::c::parallel_for;
      using yakl::c::SimpleBounds;

      dm.register_and_allocate<real>("density_dry"              ,"dry density"                ,{nz,ny,nx,nens},{"z","y","x","nens"});
      dm.register_and_allocate<real>("uvel"                     ,"x-direction velocity"       ,{nz,ny,nx,nens},{"z","y","x","nens"});
      dm.register_and_allocate<real>("vvel"                     ,"y-direction velocity"       ,{nz,ny,nx,nens},{"z","y","x","nens"});
      dm.register_and_allocate<real>("wvel"                     ,"z-direction velocity"       ,{nz,ny,nx,nens},{"z","y","x","nens"});
      dm.register_and_allocate<real>("temp"                     ,"temperature"                ,{nz,ny,nx,nens},{"z","y","x","nens"});
      dm.register_and_allocate<real>("vertical_interface_height","vertical interface height"  ,{nz+1    ,nens},{"zp1"      ,"nens"});
      dm.register_and_allocate<real>("vertical_cell_dz"         ,"vertical grid spacing"      ,{nz      ,nens},{"z"        ,"nens"});
      dm.register_and_allocate<real>("vertical_midpoint_height" ,"vertical midpoint height"   ,{nz      ,nens},{"z"        ,"nens"});
      dm.register_and_allocate<real>("hydrostasis_parameters"   ,"hydrostasis parameters"     ,{nz,5    ,nens},{"z","nhy"  ,"nens"});

      dm.register_and_allocate<real>("gcm_density_dry","GCM column dry density"                    ,{nz,nens},{"z","nens"});
      dm.register_and_allocate<real>("gcm_uvel"       ,"GCM column u-velocity"                     ,{nz,nens},{"z","nens"});
      dm.register_and_allocate<real>("gcm_vvel"       ,"GCM column v-velocity"                     ,{nz,nens},{"z","nens"});
      dm.register_and_allocate<real>("gcm_wvel"       ,"GCM column w-velocity"                     ,{nz,nens},{"z","nens"});
      dm.register_and_allocate<real>("gcm_temp"       ,"GCM column temperature"                    ,{nz,nens},{"z","nens"});
      dm.register_and_allocate<real>("gcm_water_vapor","GCM column water vapor density"            ,{nz,nens},{"z","nens"});
      dm.register_and_allocate<real>("gcm_cloud_water","GCM column cloud water density"            ,{nz,nens},{"z","nens"});
      dm.register_and_allocate<real>("gcm_cloud_ice"  ,"GCM column cloud ice density"              ,{nz,nens},{"z","nens"});

      dm.register_and_allocate<real>("ref_density_dry"  ,"Reference state column dry density"        ,{nz,nens},{"z","nens"});
      dm.register_and_allocate<real>("ref_density_vapor","Reference state column water vapor density",{nz,nens},{"z","nens"});
      dm.register_and_allocate<real>("ref_density_liq"  ,"Reference state column water liq density"  ,{nz,nens},{"z","nens"});
      dm.register_and_allocate<real>("ref_density_ice"  ,"Reference state column water ice density"  ,{nz,nens},{"z","nens"});
      dm.register_and_allocate<real>("ref_temp"         ,"Reference state column temperature"        ,{nz,nens},{"z","nens"});

      auto density_dry  = dm.get_collapsed<real>("density_dry"              );
      auto uvel         = dm.get_collapsed<real>("uvel"                     );
      auto vvel         = dm.get_collapsed<real>("vvel"                     );
      auto wvel         = dm.get_collapsed<real>("wvel"                     );
      auto temp         = dm.get_collapsed<real>("temp"                     );
      auto zint         = dm.get_collapsed<real>("vertical_interface_height");
      auto dz           = dm.get_collapsed<real>("vertical_cell_dz"         );
      auto zmid         = dm.get_collapsed<real>("vertical_midpoint_height" );
      auto hy_params    = dm.get_collapsed<real>("hydrostasis_parameters"   );
      auto gcm_rho_d    = dm.get_collapsed<real>("gcm_density_dry"          );
      auto gcm_uvel     = dm.get_collapsed<real>("gcm_uvel"                 );
      auto gcm_vvel     = dm.get_collapsed<real>("gcm_vvel"                 );
      auto gcm_wvel     = dm.get_collapsed<real>("gcm_wvel"                 );
      auto gcm_temp     = dm.get_collapsed<real>("gcm_temp"                 );
      auto gcm_rho_v    = dm.get_collapsed<real>("gcm_water_vapor"          );
      auto gcm_rho_l    = dm.get_collapsed<real>("gcm_cloud_water"          );
      auto gcm_rho_i    = dm.get_collapsed<real>("gcm_cloud_ice"            );

      parallel_for( "coupler zero" , SimpleBounds<1>(nz*ny*nx*nens) , YAKL_LAMBDA (int i) {
        density_dry (i) = 0;
        uvel        (i) = 0;
        vvel        (i) = 0;
        wvel        (i) = 0;
        temp        (i) = 0;
        if (i < (nz+1)*nens) zint(i) = 0;
        if (i < (nz  )*nens) {
          zmid     (i) = 0;
          dz       (i) = 0;
          gcm_rho_d(i) = 0;
          gcm_uvel (i) = 0;
          gcm_vvel (i) = 0;
          gcm_wvel (i) = 0;
          gcm_temp (i) = 0;
          gcm_rho_v(i) = 0;
          gcm_rho_l(i) = 0;
          gcm_rho_i(i) = 0;
        }
        if (i < nz*3  *nens) hy_params(i) = 0;
      });
    }



    void update_hydrostasis() {
      using yakl::c::parallel_for;
      using yakl::c::SimpleBounds;
      using yakl::intrinsics::matmul_cr;
      using yakl::intrinsics::matinv_ge;
      using yakl::atomicAdd;

      auto zint      = dm.get<real const,2>("vertical_interface_height");
      auto zmid      = dm.get<real const,2>("vertical_midpoint_height" );
      auto dz        = dm.get<real const,2>("vertical_cell_dz");
      auto hy_params = dm.get<real,3>("hydrostasis_parameters"   );

      auto dens_dry = dm.get<real const,4>("density_dry");
      auto dens_wv  = dm.get<real const,4>("water_vapor");
      auto temp     = dm.get<real const,4>("temp");

      int nz   = get_nz();
      int ny   = get_ny();
      int nx   = get_nx();
      int nens = get_nens();

      auto R_d = get_option<real>("R_d");
      auto R_v = get_option<real>("R_v");

      // Compute average column of pressure for each ensemble
      real2d pressure_col("pressure_col",nz,nens);
      memset( pressure_col , 0._fp );
      real r_nx_ny = 1._fp / (nx*ny);
      parallel_for( SimpleBounds<4>(nz,ny,nx,nens) , YAKL_LAMBDA (int k, int j, int i, int iens) {
        real rho_d = dens_dry(k,j,i,iens);
        real rho_v = dens_wv (k,j,i,iens);
        real T     = temp    (k,j,i,iens);
        atomicAdd( pressure_col(k,iens) , compute_pressure( rho_d , rho_v , T , R_d , R_v )*r_nx_ny );
      });

      parallel_for( SimpleBounds<2>(nz,nens) , YAKL_LAMBDA (int k, int iens) {
        int constexpr npts = 5;
        int kmid = k;
        int kbot = k-2;
        int ktop = k+2;
        while (kbot < 0   ) { kbot++; ktop++; }
        while (ktop > nz-1) { kbot--; ktop--; }

        SArray<double,1,npts> z;
        real z0 = zmid(kmid,iens);
        for (int i=0; i < npts; i++) { z(i) = ( zmid(kbot+i,iens) - z0 ) / dz(k,iens); }

        SArray<double,2,npts,npts> vand;
        for (int j=0; j < npts; j++) {
          for (int i=0; i < npts; i++) {
            vand(j,i) = pow( z(i) , (double) j );
          }
        }

        auto vand_inv = matinv_ge( vand );

        SArray<double,1,npts> logp;
        for (int i=0; i < npts; i++) {
          logp(i) = log(pressure_col(kbot+i,iens));
        }

        auto params = matmul_cr( vand_inv , logp );

        for (int i=0; i < npts; i++) { hy_params(k,i,iens) = params(i); }
      });

    }



    real4d compute_pressure_array() const {
      using yakl::c::parallel_for;
      using yakl::c::SimpleBounds;

      auto dens_dry = dm.get<real const,4>("density_dry");
      auto dens_wv  = dm.get<real const,4>("water_vapor");
      auto temp     = dm.get<real const,4>("temp");

      int nz   = get_nz();
      int ny   = get_ny();
      int nx   = get_nx();
      int nens = get_nens();

      real4d pressure("pressure",nz,ny,nx,nens);

      auto R_d = get_option<real>("R_d");
      auto R_v = get_option<real>("R_v");

      parallel_for( "coupler pressure" , SimpleBounds<4>(nz,ny,nx,nens) ,
                    YAKL_LAMBDA (int k, int j, int i, int iens) {
        real rho_d = dens_dry(k,j,i,iens);
        real rho_v = dens_wv (k,j,i,iens);
        real T     = temp    (k,j,i,iens);
        pressure(k,j,i,iens) = compute_pressure( rho_d , rho_v , T , R_d , R_v );
      });

      return pressure;
    }


  };

}



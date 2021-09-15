
#pragma once

#include "sam1mom_include.h"


namespace sam1mom {
  class Sam1mom {
  public:
    real static constexpr rhor = 1000.;           // Density of water, kg/m3
    real static constexpr rhos = 100.;            // Density of snow, kg/m3
    real static constexpr rhog = 400.;            // Density of graupel, kg/m3
    real static constexpr tbgmin = 253.16;        // Minimum temperature for cloud water., K
    real static constexpr tbgmax = 273.16;        // Maximum temperature for cloud ice, K
    real static constexpr tprmin = 268.16;        // Minimum temperature for rain, K
    real static constexpr tprmax = 283.16;        // Maximum temperature for snow+graupel, K
    real static constexpr tgrmin = 223.16;        // Minimum temperature for snow, K
    real static constexpr tgrmax = 283.16;        // Maximum temperature for graupel, K
    real static constexpr a_rain = 842.;          // Coeff.for rain term vel
    real static constexpr b_rain = 0.8;           // Fall speed exponent for rain
    real static constexpr a_snow = 4.84;          // Coeff.for snow term vel
    real static constexpr b_snow = 0.25;          // Fall speed exponent for snow
    real static constexpr a_grau = 94.5;          // Lin (1983) (rhog=400)
    real static constexpr b_grau = 0.5;           // Fall speed exponent for graupel
    real static constexpr qcw0 = 1.e-3;           // Threshold for water autoconversion, g/g
    real static constexpr qci0 = 1.e-4;           // Threshold for ice autoconversion, g/g
    real static constexpr alphaelq = 1.e-3;       // autoconversion of cloud water rate coef
    real static constexpr betaelq = 1.e-3;        // autoconversion of cloud ice rate coef
    real static constexpr erccoef = 1.0;          // Rain/Cloud water collection efficiency
    real static constexpr esccoef = 1.0;          // Snow/Cloud water collection efficiency
    real static constexpr esicoef = 0.1;          // Snow/cloud ice collection efficiency
    real static constexpr egccoef = 1.0;          // Graupel/Cloud water collection efficiency
    real static constexpr egicoef = 0.1;          // Graupel/Cloud ice collection efficiency
    real static constexpr nzeror = 8.e6;          // Intercept coeff. for rain
    real static constexpr nzeros = 3.e6;          // Intersept coeff. for snow
    real static constexpr nzerog = 4.e6;          // Intersept coeff. for graupel
    real static constexpr qp_threshold = 1.e-8;   // minimal rain/snow water content
    real static constexpr cp = 1004.;             // Specific heat of air, J/kg/K
    real static constexpr ggr = 9.81;             // Gravity acceleration, m/s2
    real static constexpr lcond = 2.5104e+06;     // Latent heat of condensation, J/kg
    real static constexpr lfus = 0.3336e+06;      // Latent heat of fusion, J/kg
    real static constexpr lsub = 2.8440e+06;      // Latent heat of sublimation, J/kg
    real static constexpr rv = 461.;              // Gas constant for water vapor, J/kg/K
    real static constexpr rgas = 287.;            // Gas constant for dry air, J/kg/K
    real static constexpr diffelq = 2.21e-05;     // Diffusivity of water vapor, m2/s
    real static constexpr therco = 2.40e-02;      // Thermal conductivity of air, J/m/s/K
    real static constexpr muelq = 1.717e-05;      // Dynamic viscosity of air
    real static constexpr fac_cond = lcond/cp;      
    real static constexpr fac_fus = lfus/cp;        
    real static constexpr fac_sub = lsub/cp;        
    real static constexpr pi = 3.141592653589793;
    real static constexpr a_bg = 1./(tbgmax-tbgmin);
    real static constexpr a_pr = 1./(tprmax-tprmin);
    real static constexpr a_gr = 1./(tgrmax-tgrmin);



    // dt             : Time step (s)
    // zint(ncol,nz+1): constant grid spacing in z direction (when dz_constant=.true.)
    // rho (ncol,nz  ): air density at pressure levels,kg/m3 
    // rhow(ncol,nz+1): air density at vertical velocity levels,kg/m3
    // pres(ncol,nz  ): pressure,mb at scalar levels
    // tabs(ncol,nz  ): temperature
    // qv  (ncol,nz  ): water vapor
    // qn  (ncol,nz  ): cloud condensate (liquid + ice)
    // qp  (ncol,nz  ): total precipitating water
    void main(real dt, real2d const &zint, real2d const &rho, real2d const &rhow, real2d const &pres,
              real2d &tabs, real2d &qv, real2d &qn, real2d &qp) {

      int ncol = size(rho,1);
      int nz   = size(rho,2);

      // The following are computed by precip_init
      real2d accrsi ("accrsi ",ncol,nz);  // Undocumented
      real2d accrsc ("accrsc ",ncol,nz);  // Undocumented
      real2d coefice("coefice",ncol,nz);  // Undocumented
      real2d evaps1 ("evaps1 ",ncol,nz);  // Undocumented
      real2d evaps2 ("evaps2 ",ncol,nz);  // Undocumented
      real2d accrgi ("accrgi ",ncol,nz);  // Undocumented
      real2d accrgc ("accrgc ",ncol,nz);  // Undocumented
      real2d evapg1 ("evapg1 ",ncol,nz);  // Undocumented
      real2d evapg2 ("evapg2 ",ncol,nz);  // Undocumented
      real2d accrrc ("accrrc ",ncol,nz);  // Undocumented
      real2d evapr1 ("evapr1 ",ncol,nz);  // Undocumented
      real2d evapr2 ("evapr2 ",ncol,nz);  // Undocumented
      real   gam3, gamr1, gamr2, gamr3, gams1, gams2, gams3, gamg1, gamg2, gamg3;

      // Other internal variables
      real1d dz      ("dz      ",ncol)   ;  // Grid spacing at the lowest level
      real2d adz     ("adz     ",ncol,nz);  // Ratio of grid spacing to dz
      real2d gamaz   ("gamaz   ",ncol,nz);  // grav/cp*z
      real2d t       ("t       ",ncol,nz);  // liquid/ice water static energy 
      real2d q       ("q       ",ncol,nz);  // total nonprecipitating water
      real2d qcl     ("qcl     ",ncol,nz);  // liquid water  (condensate)
      real2d qci     ("qci     ",ncol,nz);  // ice water  (condensate)
      real2d qpl     ("qpl     ",ncol,nz);  // liquid water  (precipitation)
      real2d qpi     ("qpi     ",ncol,nz);  // ice water  (precipitation)
      real2d qpfall  ("qpfall  ",ncol,nz);  // for statistics
      real2d precflux("precflux",ncol,nz);  // for statistics
      real1d precsfc ("precsfc ",ncol   );  // surface precip. rate
      real1d precssfc("precssfc",ncol   );  // surface ice precip. rate
      real2d qpsrc   ("qpsrc   ",ncol,nz);  // source of precipitation microphysical processes
      real2d qpevp   ("qpevp   ",ncol,nz);  // sink of precipitating water due to evaporation

      /////////////////////////////////////////////////
      // Compute initial quantities
      /////////////////////////////////////////////////
      // do icol = 1 , ncol
      parallel_for( ncol , YAKL_LAMBDA (int icol) {
        dz(icol) = zint(icol,2) - zint(icol,1);
      });

      // do k = 1 , nz
      //   do icol = 1 , ncol
      parallel_for( Bounds<2>(nz,ncol) , YAKL_LAMBDA (int k, int icol) {
        // Compute adz
        adz(icol,k) = ( zint(icol,k+1) - zint(icol,k) ) / dz(icol);

        // Compute gamaz
        real zmid = 0.5_fp * ( zint(icol,k) + zint(icol,k+1) );
        gamaz(icol,k)=ggr/cp*zmid;

        // Compute qcl, qci, qpl, qpi, and t
        real omn = max( 0._fp , min( 1._fp , (tabs(icol,k)-tbgmin)*a_bg ) );
        real omp = max( 0._fp , min( 1._fp , (tabs(icol,k)-tprmin)*a_pr ) );
        qcl(icol,k) = qn(icol,k)*omn;
        qci(icol,k) = qn(icol,k)*(1.-omn);
        qpl(icol,k) = qp(icol,k)*omp;
        qpi(icol,k) = qp(icol,k)*(1.-omp);
        t  (icol,k) = tabs(icol,k)+gamaz(icol,k)-fac_cond*qcl(icol,k)-fac_sub*qci(icol,k) 
                                                -fac_cond*qpl(icol,k)-fac_sub*qpi(icol,k);

        // Compute q
        q(icol,k) = qv(icol,k) + qn(icol,k);

        // Initialize statistics variables to zero
        qpfall  (icol,k) = 0;
        precflux(icol,k) = 0;
        precsfc (icol  ) = 0;
        precssfc(icol  ) = 0;
        qpsrc   (icol,k) = 0;
        qpevp   (icol,k) = 0;
      });

      precip_init(tabs, pres, rho, accrsi, accrsc, coefice, evaps1, evaps2, accrgi, accrgc, evapg1, evapg2,  
                  accrrc, evapr1, evapr2, b_rain, b_snow, b_grau, a_grau, a_rain, a_snow, diffelq, egccoef,  
                  egicoef, erccoef, esccoef, esicoef, lcond, lsub, muelq, nzerog, nzeror, nzeros, rhog,      
                  rhor, rhos, rv, therco, gam3, gamr1, gamr2, gamr3, gams1, gams2, gams3, gamg1, gamg2,      
                  gamg3);
      // real(8), intent(in   ) :: tabs0  (ncol,nz) ! Undocumented
      // real(8), intent(in   ) :: pres   (ncol,nz) ! pressure,mb at scalar levels
      // real(8), intent(in   ) :: rho    (ncol,nz) ! air density at pressure levels,kg/m3 
      // real(8), intent(  out) :: accrsi (ncol,nz) ! Undocumented
      // real(8), intent(  out) :: accrsc (ncol,nz) ! Undocumented
      // real(8), intent(  out) :: coefice(ncol,nz) ! Undocumented
      // real(8), intent(  out) :: evaps1 (ncol,nz) ! Undocumented
      // real(8), intent(  out) :: evaps2 (ncol,nz) ! Undocumented
      // real(8), intent(  out) :: accrgi (ncol,nz) ! Undocumented
      // real(8), intent(  out) :: accrgc (ncol,nz) ! Undocumented
      // real(8), intent(  out) :: evapg1 (ncol,nz) ! Undocumented
      // real(8), intent(  out) :: evapg2 (ncol,nz) ! Undocumented
      // real(8), intent(  out) :: accrrc (ncol,nz) ! Undocumented
      // real(8), intent(  out) :: evapr1 (ncol,nz) ! Undocumented
      // real(8), intent(  out) :: evapr2 (ncol,nz) ! Undocumented


      cloud(q, tabs, t, gamaz, qp, pres, qn, tbgmax, tbgmin, tprmax, tprmin, fac_cond, fac_fus, fac_sub, 
            tgrmax, tgrmin);
      // ! real(8), intent(inout) :: q    (ncol,nz)  ! total nonprecipitating water
      // ! real(8), intent(  out) :: tabs (ncol,nz)  ! temperature
      // ! real(8), intent(in   ) :: t    (ncol,nz)  ! liquid/ice water static energy 
      // ! real(8), intent(in   ) :: gamaz(ncol,nz)  ! grav/cp*z
      // ! real(8), intent(inout) :: qp   (ncol,nz)  ! total precipitating water
      // ! real(8), intent(in   ) :: pres (ncol,nz)  ! pressure,mb at scalar levels
      // ! real(8), intent(  out) :: qn   (ncol,nz)  ! cloud condensate (liquid + ice)

      precip_proc(qpsrc, qpevp, qn, qp, tabs, coefice, accrrc, accrsc, accrsi, accrgc, accrgi, q, pres, 
                  evapr1, evapr2, evaps1, evaps2, evapg1, evapg2, a_bg, a_gr, a_pr, alphaelq, b_grau, 
                  b_rain, b_snow, betaelq, dt, qci0, qcw0, qp_threshold, tbgmin, tgrmin, tprmin);
      // ! real(8), intent(  out) :: qpsrc  (ncol,nz) ! source of precipitation microphysical processes
      // ! real(8), intent(  out) :: qpevp  (ncol,nz) ! sink of precipitating water due to evaporation
      // ! real(8), intent(inout) :: qn     (ncol,nz) ! cloud condensate (liquid + ice)
      // ! real(8), intent(inout) :: qp     (ncol,nz) ! total precipitating water
      // ! real(8), intent(in   ) :: tabs   (ncol,nz) ! temperature
      // ! real(8), intent(in   ) :: coefice(ncol,nz) ! Undocumented
      // ! real(8), intent(in   ) :: accrrc (ncol,nz) ! Undocumented
      // ! real(8), intent(in   ) :: accrsc (ncol,nz) ! Undocumented
      // ! real(8), intent(in   ) :: accrsi (ncol,nz) ! Undocumented
      // ! real(8), intent(in   ) :: accrgc (ncol,nz) ! Undocumented
      // ! real(8), intent(in   ) :: accrgi (ncol,nz) ! Undocumented
      // ! real(8), intent(inout) :: q      (ncol,nz) ! total nonprecipitating water
      // ! real(8), intent(in   ) :: pres   (ncol,nz) ! pressure,mb at scalar levels
      // ! real(8), intent(in   ) :: evapr1 (ncol,nz) ! Undocumented
      // ! real(8), intent(in   ) :: evapr2 (ncol,nz) ! Undocumented
      // ! real(8), intent(in   ) :: evaps1 (ncol,nz) ! Undocumented
      // ! real(8), intent(in   ) :: evaps2 (ncol,nz) ! Undocumented
      // ! real(8), intent(in   ) :: evapg1 (ncol,nz) ! Undocumented
      // ! real(8), intent(in   ) :: evapg2 (ncol,nz) ! Undocumented

      /////////////////////////////////////////////////////////////////////////////
      // Update t from tabs, which was changed in cloud()
      // Also updates qcl and qci, which are needed in ice_fall
      /////////////////////////////////////////////////////////////////////////////
      // do k = 1 , nz
      //   do icol = 1 , ncol
      parallel_for( Bounds<2>(nz,ncol) , YAKL_LAMBDA (int k, int icol) {
        // Compute qcl, qci, qpl, qpi, and t
        real omn = max( 0. , min( 1. , (tabs(icol,k)-tbgmin)*a_bg ) );
        real omp = max( 0. , min( 1. , (tabs(icol,k)-tprmin)*a_pr ) );
        qcl(icol,k) = qn(icol,k)*omn;
        qci(icol,k) = qn(icol,k)*(1.-omn);
        qpl(icol,k) = qp(icol,k)*omp;
        qpi(icol,k) = qp(icol,k)*(1.-omp);
        t  (icol,k) = tabs(icol,k)+gamaz(icol,k)-fac_cond*qcl(icol,k)-fac_sub*qci(icol,k) 
                                                -fac_cond*qpl(icol,k)-fac_sub*qpi(icol,k);
      });

      // ! NOTE: In SAM, ice_fall happens before precip_fall
      // call ice_fall( qcl, qci, tabs, adz, dz, rho, q, t, precsfc, precssfc, dt, fac_cond, fac_fus, ncol, nz )
      // ! real(8), intent(in   ) :: qcl     (ncol,nz) ! liquid water  (condensate)
      // ! real(8), intent(in   ) :: qci     (ncol,nz) ! ice water  (condensate)
      // ! real(8), intent(in   ) :: tabs    (ncol,nz) ! temperature
      // ! real(8), intent(in   ) :: adz     (ncol,nz) ! ratio of the thickness of scalar levels to dz 
      // ! real(8), intent(in   ) :: dz      (ncol   ) ! constant grid spacing in z direction (when dz_constant=.true.)
      // ! real(8), intent(in   ) :: rho     (ncol,nz) ! air density at pressure levels,kg/m3 
      // ! real(8), intent(inout) :: q       (ncol,nz) ! total nonprecipitating water
      // ! real(8), intent(inout) :: t       (ncol,nz) ! liquid/ice water static energy 
      // ! real(8), intent(inout) :: precsfc (ncol   ) ! surface precip. rate
      // ! real(8), intent(inout) :: precssfc(ncol   ) ! surface ice precip. rate

      // call micro_precip_fall(rho, adz, dz, rhow, qp, t, tabs, qpfall, precflux, precsfc, precssfc,       &
      //                        qp_threshold, tprmin, a_pr, tgrmin, a_gr, dt, fac_cond, fac_fus, &
      //                        b_rain, b_snow, b_grau, a_rain, a_snow, a_grau, gamr3, gams3, gamg3, rhor, rhos,  &
      //                        rhog, nzeror, nzeros, nzerog, ncol, nz)
      // ! real(8), intent(in   ) :: rho     (ncol,nz  ) ! air density at pressure levels,kg/m3 
      // ! real(8), intent(in   ) :: adz     (ncol,nz  ) ! ratio of the thickness of scalar levels to dz 
      // ! real(8), intent(in   ) :: dz      (ncol     ) ! constant grid spacing in z direction (when dz_constant=.true.)
      // ! real(8), intent(in   ) :: rhow    (ncol,nz+1) ! air density at vertical velocity levels,kg/m3
      // ! real(8), intent(inout) :: qp      (ncol,nz  ) ! total precipitating water
      // ! real(8), intent(inout) :: t       (ncol,nz  ) ! liquid/ice water static energy 
      // ! real(8), intent(in   ) :: tabs    (ncol,nz  ) ! temperature
      // ! real(8), intent(inout) :: qpfall  (ncol,nz  ) ! for statistics
      // ! real(8), intent(inout) :: precflux(ncol,nz  ) ! for statistics
      // ! real(8), intent(inout) :: precsfc (ncol     ) ! surface precip. rate
      // ! real(8), intent(inout) :: precssfc(ncol     ) ! surface ice precip. rate

      // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
      // !! Compute tabs from t. Compute qv from q and qn
      // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
      // do k = 1 , nz
      //   do icol = 1 , ncol
      //     ! Compute qv
      //     qv(icol,k) = q(icol,k) - qn(icol,k)

      //     !Compute tabs
      //     omn = max(real(0.,8),min(real(1.,8),(tabs(icol,k)-tbgmin)*a_bg))
      //     omp = max(real(0.,8),min(real(1.,8),(tabs(icol,k)-tprmin)*a_pr))
      //     qcl(icol,k) = qn(icol,k)*omn
      //     qci(icol,k) = qn(icol,k)*(1.-omn)
      //     qpl(icol,k) = qp(icol,k)*omp
      //     qpi(icol,k) = qp(icol,k)*(1.-omp)
      //     tabs(icol,k) = t(icol,k) - gamaz(icol,k) + fac_cond*qcl(icol,k) + fac_sub*qci(icol,k) + fac_cond*qpl(icol,k) + fac_sub*qpi(icol,k)
      //   enddo
      // enddo

    }



    void precip_init(real2d const &tabs0, real2d const &pres, real2d const &rho, real2d &accrsi, real2d &accrsc,
                     real2d &coefice, real2d &evaps1, real2d &evaps2, real2d &accrgi, real2d &accrgc, real2d &evapg1,
                     real2d &evapg2, real2d &accrrc, real2d &evapr1, real2d &evapr2,
                     real b_rain, real b_snow, real b_grau, real a_grau, real a_rain, real a_snow, real diffelq,
                     real egccoef, real egicoef, real erccoef, real esccoef, real esicoef, real lcond, real lsub,
                     real muelq, real nzerog, real nzeror, real nzeros, real rhog, real rhor, real rhos, real rv,
                     real therco,
                     real &gam3, real &gamr1, real &gamr2, real &gamr3, real &gams1, real &gams2, real &gams3,
                     real &gamg1, real &gamg2, real &gamg3) {

      int ncol = size(tabs0,1);
      int nz   = size(tabs0,2);

      gam3 = 3.;
      gamr1 = 3.+b_rain;
      gamr2 = (5.+b_rain)/2.;
      gamr3 = 4.+b_rain;
      gams1 = 3.+b_snow;
      gams2 = (5.+b_snow)/2.;
      gams3 = 4.+b_snow;
      gamg1 = 3.+b_grau;
      gamg2 = (5.+b_grau)/2.;
      gamg3 = 4.+b_grau;
      gam3  = tgamma(gam3 );
      gamr1 = tgamma(gamr1);
      gamr2 = tgamma(gamr2);
      gamr3 = tgamma(gamr3);
      gams1 = tgamma(gams1);
      gams2 = tgamma(gams2);
      gams3 = tgamma(gams3);
      gamg1 = tgamma(gamg1);
      gamg2 = tgamma(gamg2);
      gamg3 = tgamma(gamg3);

      if (round(gam3) != 2) {
        endrun( "cannot compute gamma-function in precip_init. Exiting..." );
      }

      // do k=1,nz
      //   do icol = 1 , ncol
      parallel_for( Bounds<2>(nz,ncol) , YAKL_LAMBDA (int k, int icol) {
        real pratio = sqrt(1.29 / rho(icol,k));
        real rrr1=393./(tabs0(icol,k)+120.)* pow( (tabs0(icol,k)/273.) , 1.5 );
        real rrr2=pow( (tabs0(icol,k)/273.) , 1.94 )*(1000./pres(icol,k));
        real estw = 100.*esatw_crm(tabs0(icol,k));
        real esti = 100.*esati_crm(tabs0(icol,k));

        // accretion by snow:
        real coef1 = 0.25 * pi * nzeros * a_snow * gams1 * pratio/pow( (pi * rhos * nzeros/rho(icol,k) ) , ((3.+b_snow)/4.) );
        real coef2 = exp(0.025*(tabs0(icol,k) - 273.15));
        accrsi(icol,k) =  coef1 * coef2 * esicoef;
        accrsc(icol,k) =  coef1 * esccoef;
        coefice(icol,k) =  coef2;

        // evaporation of snow:
        coef1  =(lsub/(tabs0(icol,k)*rv)-1.)*lsub/(therco*rrr1*tabs0(icol,k));
        coef2  = rv*tabs0(icol,k)/(diffelq*rrr2*esti);
        evaps1(icol,k)  =  0.65*4.*nzeros/sqrt(pi*rhos*nzeros)/(coef1+coef2)/sqrt(rho(icol,k));
        evaps2(icol,k)  =  0.49*4.*nzeros*gams2*sqrt(a_snow/(muelq*rrr1))/pow( (pi*rhos*nzeros) , ((5.+b_snow)/8.) ) / (coef1+coef2) * pow( rho(icol,k) , ((1.+b_snow)/8.) )*sqrt(pratio);

        // accretion by graupel:
        coef1 = 0.25*pi*nzerog*a_grau*gamg1*pratio/pow( (pi*rhog*nzerog/rho(icol,k)) , ((3.+b_grau)/4.) );
        coef2 = exp(0.025*(tabs0(icol,k) - 273.15));
        accrgi(icol,k) =  coef1 * coef2 * egicoef;
        accrgc(icol,k) =  coef1 * egccoef;

        // evaporation of graupel:
        coef1  =(lsub/(tabs0(icol,k)*rv)-1.)*lsub/(therco*rrr1*tabs0(icol,k));
        coef2  = rv*tabs0(icol,k)/(diffelq*rrr2*esti);
        evapg1(icol,k)  = 0.65*4.*nzerog/sqrt(pi*rhog*nzerog)/(coef1+coef2)/sqrt(rho(icol,k));
        evapg2(icol,k)  = 0.49*4.*nzerog*gamg2*sqrt(a_grau/(muelq*rrr1))/pow( (pi * rhog * nzerog) , ((5+b_grau)/8.) ) / (coef1+coef2) * pow( rho(icol,k) , ((1.+b_grau)/8.) )*sqrt(pratio);

        // accretion by rain:
        accrrc(icol,k)=  0.25 * pi * nzeror * a_rain * gamr1 * pratio/pow( (pi * rhor * nzeror / rho(icol,k)) , ((3.+b_rain)/4.) )* erccoef;

        // evaporation of rain:
        coef1  =(lcond/(tabs0(icol,k)*rv)-1.)*lcond/(therco*rrr1*tabs0(icol,k));
        coef2  = rv*tabs0(icol,k)/(diffelq * rrr2 * estw);
        evapr1(icol,k)  =  0.78 * 2. * pi * nzeror / sqrt(pi * rhor * nzeror) / (coef1+coef2) / sqrt(rho(icol,k));
        evapr2(icol,k)  =  0.31 * 2. * pi  * nzeror * gamr2 * 0.89 * sqrt(a_rain/(muelq*rrr1))/pow( (pi * rhor * nzeror) , ((5.+b_rain)/8.) ) / (coef1+coef2) * pow( rho(icol,k) , ((1.+b_rain)/8.) )*sqrt(pratio);
      });
    }



    // Condensation of cloud water/cloud ice.
    // q    (ncol,nz): total nonprecipitating water
    // tabs (ncol,nz): temperature
    // t    (ncol,nz): liquid/ice water static energy 
    // gamaz(ncol,nz): grav/cp*z
    // qp   (ncol,nz): total precipitating water
    // pres (ncol,nz): pressure,mb at scalar levels
    // qn   (ncol,nz): cloud condensate (liquid + ice)
    void cloud(real2d &q, real2d &tabs, real2d const &t, real2d const &gamaz, real2d &qp, real2d const &pres,
               real2d &qn, real tbgmax, real tbgmin, real tprmax, real tprmin, real fac_cond, real fac_fus,
               real fac_sub, real tgrmax, real tgrmin) {
      int ncol = size(q,1);
      int nz   = size(q,2);

      real an = 1./(tbgmax-tbgmin);
      real bn = tbgmin * an;
      real ap = 1./(tprmax-tprmin);
      real bp = tprmin * ap;
      real fac1 = fac_cond+(1+bp)*fac_fus;
      real fac2 = fac_fus*ap;
      real ag = 1./(tgrmax-tgrmin);

      // do k = 1, nz
      //   do icol = 1, ncol
      parallel_for( Bounds<2>(nz,ncol) , YAKL_LAMBDA (int k, int icol) {
        real qsatt;
        q(icol,k)=max( 0. , q(icol,k) );
        // Initial guess for temperature assuming no cloud water/ice:
        tabs(icol,k) = t(icol,k)-gamaz(icol,k);
        real tabs1 = (tabs(icol,k)+fac1*qp(icol,k))/(1.+fac2*qp(icol,k));
        // Warm cloud:
        if (tabs1 >= tbgmax) {
          tabs1=tabs(icol,k)+fac_cond*qp(icol,k);
          qsatt = qsatw_crm(tabs1,pres(icol,k));
          // Ice cloud:
        } else if (tabs1 <= tbgmin) {
          tabs1=tabs(icol,k)+fac_sub*qp(icol,k);
          qsatt = qsati_crm(tabs1,pres(icol,k));
          // Mixed-phase cloud:
        } else {
          real om = an*tabs1-bn;
          qsatt = om*qsatw_crm(tabs1,pres(icol,k))+(1.-om)*qsati_crm(tabs1,pres(icol,k));
        }
        // Test if condensation is possible:
        if ( q(icol,k) > qsatt ) {
          int niter = 0;
          real dtabs = 100.;
          real dqsat;
          while (abs(dtabs) > 0.01 && niter < 10) {
            real lstarn;
            real dlstarn;
            real lstarp;
            real dlstarp;
            if (tabs1 >= tbgmax) {
              real om=1.;
              lstarn=fac_cond;
              dlstarn=0.;
              qsatt=qsatw_crm(tabs1,pres(icol,k));
              dqsat=dtqsatw_crm(tabs1,pres(icol,k));
            } else if (tabs1 <= tbgmin) {
              real om=0.;
              lstarn=fac_sub;
              dlstarn=0.;
              qsatt=qsati_crm(tabs1,pres(icol,k));
              dqsat=dtqsati_crm(tabs1,pres(icol,k));
            } else {
              real om=an*tabs1-bn;
              lstarn=fac_cond+(1.-om)*fac_fus;
              dlstarn=an*fac_fus;
              qsatt=om*qsatw_crm(tabs1,pres(icol,k))+(1.-om)*qsati_crm(tabs1,pres(icol,k));
              dqsat=om*dtqsatw_crm(tabs1,pres(icol,k))+(1.-om)*dtqsati_crm(tabs1,pres(icol,k));
            }
            if (tabs1 >= tprmax) {
              real omp=1.;
              lstarp=fac_cond;
              dlstarp=0.;
            } else if(tabs1 <= tprmin) {
              real omp=0.;
              lstarp=fac_sub;
              dlstarp=0.;
            } else {
              real omp=ap*tabs1-bp;
              lstarp=fac_cond+(1.-omp)*fac_fus;
              dlstarp=ap*fac_fus;
            }
            real fff = tabs(icol,k)-tabs1+lstarn*(q(icol,k)-qsatt)+lstarp*qp(icol,k);
            real dfff=dlstarn*(q(icol,k)-qsatt)+dlstarp*qp(icol,k)-lstarn*dqsat-1.;
            dtabs=-fff/dfff;
            niter=niter+1;
            tabs1=tabs1+dtabs;
          }
          qsatt = qsatt + dqsat * dtabs;
          qn(icol,k) = max( 0. , q(icol,k)-qsatt );
        } else {     // if ( q(icol,k) > qsatt )
          qn(icol,k) = 0.;
        }     // if ( q(icol,k) > qsatt )
        tabs(icol,k) = tabs1;
        qp(icol,k) = max( 0. , qp(icol,k) ); // just in case
      });
    }



    // qpsrc  : source of precipitation microphysical processes
    // qpevp  : sink of precipitating water due to evaporation
    // qn     : cloud condensate (liquid + ice)
    // qp     : total precipitating water
    // tabs   : temperature
    // coefice: Undocumented
    // accrrc : Undocumented
    // accrsc : Undocumented
    // accrsi : Undocumented
    // accrgc : Undocumented
    // accrgi : Undocumented
    // q      : total nonprecipitating water
    // pres   : pressure,mb at scalar levels
    // evapr1 : Undocumented
    // evapr2 : Undocumented
    // evaps1 : Undocumented
    // evaps2 : Undocumented
    // evapg1 : Undocumented
    // evapg2 : Undocumented
    void precip_proc(real2d &qpsrc, real2d &qpevp, real2d &qn, real2d &qp, real2d const &tabs, real2d const &coefice,
                     real2d const &accrrc, real2d const &accrsc, real2d const &accrsi, real2d const &accrgc,
                     real2d const &accrgi, real2d &q, real2d const &pres, real2d const &evapr1, real2d const &evapr2,
                     real2d const &evaps1, real2d const &evaps2, real2d const &evapg1, real2d const &evapg2, real a_bg,
                     real a_gr, real a_pr, real alphaelq, real b_grau, real b_rain, real b_snow, real betaelq,
                     real dtn, real qci0, real qcw0, real qp_threshold, real tbgmin, real tgrmin, real tprmin) {
      int ncol = size(qpsrc,1);
      int nz   = size(qpsrc,2);

      real powr1 = (3 + b_rain) / 4.;
      real powr2 = (5 + b_rain) / 8.;
      real pows1 = (3 + b_snow) / 4.;
      real pows2 = (5 + b_snow) / 8.;
      real powg1 = (3 + b_grau) / 4.;
      real powg2 = (5 + b_grau) / 8.;

      // do k=1,nz
      //   do icol = 1 , ncol
      parallel_for( Bounds<2>(nz,ncol) , YAKL_LAMBDA (int k, int icol) {
        qpsrc(icol,k)=0.;
        qpevp(icol,k)=0.;
      });

      // do k=1,nz
      //   do icol = 1 , ncol
      parallel_for( Bounds<2>(nz,ncol) , YAKL_LAMBDA (int k, int icol) {
        //-------     Autoconversion/accretion
        if (qn(icol,k)+qp(icol,k) > 0. ) {
          real omn = max( 0. , min( 1. , (tabs(icol,k)-tbgmin)*a_bg ) );
          real omp = max( 0. , min( 1. , (tabs(icol,k)-tprmin)*a_pr ) );
          real omg = max( 0. , min( 1. , (tabs(icol,k)-tgrmin)*a_gr ) );

          if (qn(icol,k) > 0.) {

            real qcc = qn(icol,k) * omn;
            real qii = qn(icol,k) * (1.-omn);

            real autor;
            if(qcc > qcw0) {
              autor = alphaelq;
            } else {
              autor = 0.;
            }

            real autos;
            if(qii > qci0) {
              autos = betaelq*coefice(icol,k);
            } else {
              autos = 0.;
            }

            real accrr = 0.;
            if (omp > 0.001) {
              real qrr = qp(icol,k) * omp;
              accrr = accrrc(icol,k) * pow( qrr , powr1 );
            }
            real accrcs = 0.;
            real accris = 0.;
            if (omp < 0.999 && omg < 0.999) {
              real qss = qp(icol,k) * (1.-omp)*(1.-omg);
              real tmp = pow( qss , pows1 );
              accrcs = accrsc(icol,k) * tmp;
              accris = accrsi(icol,k) * tmp;
            }
            real accrcg = 0.;
            real accrig = 0.;
            if (omp < 0.999 && omg > 0.001) {
              real qgg = qp(icol,k) * (1.-omp)*omg;
              real tmp = pow( qgg , powg1 );
              accrcg = accrgc(icol,k) * tmp;
              accrig = accrgi(icol,k) * tmp;
            }
            qcc = (qcc+dtn*autor*qcw0)/(1.+dtn*(accrr+accrcs+accrcg+autor));
            qii = (qii+dtn*autos*qci0)/(1.+dtn*(accris+accrig+autos));
            real dq = dtn *(accrr*qcc + autor*(qcc-qcw0)+(accris+accrig)*qii + (accrcs+accrcg)*qcc + autos*(qii-qci0));
            dq = min(dq,qn(icol,k));
            qp   (icol,k) = qp   (icol,k) + dq;
            q    (icol,k) = q    (icol,k) - dq;
            qn   (icol,k) = qn   (icol,k) - dq;
            qpsrc(icol,k) = qpsrc(icol,k) + dq;

          } else if (qp(icol,k) > qp_threshold && qn(icol,k) == 0.) {

            real qsatt = 0.;
            if (omn > 0.001) qsatt = qsatt + omn*qsatw_crm(tabs(icol,k),pres(icol,k));
            if (omn < 0.999) qsatt = qsatt + (1.-omn)*qsati_crm(tabs(icol,k),pres(icol,k));
            real dq = 0.;
            if (omp > 0.001) {
              real qrr = qp(icol,k) * omp;
              dq = dq + evapr1(icol,k)*sqrt(qrr) + evapr2(icol,k)*pow( qrr , powr2 );
            }
            if (omp < 0.999 && omg < 0.999) {
              real qss = qp(icol,k) * (1.-omp)*(1.-omg);
              dq = dq + evaps1(icol,k)*sqrt(qss) + evaps2(icol,k)*pow( qss , pows2 );
            }
            if (omp < 0.999 && omg > 0.001) {
              real qgg = qp(icol,k) * (1.-omp)*omg;
              dq = dq + evapg1(icol,k)*sqrt(qgg) + evapg2(icol,k)*pow( qgg , powg2 );
            }
            dq = dq * dtn * (q(icol,k) /qsatt-1.);
            dq = max(-0.5*qp(icol,k),dq);
            qp   (icol,k) = qp   (icol,k) + dq;
            q    (icol,k) = q    (icol,k) - dq;
            qpevp(icol,k) = qpevp(icol,k) + dq;

          } else {

            q    (icol,k) = q    (icol,k) + qp(icol,k);
            qpevp(icol,k) = qpevp(icol,k) - qp(icol,k);
            qp   (icol,k) = 0.;

          }

        }   // if (qn(icol,k)+qp(icol,k) > 0. )

        real dq = qp(icol,k);
        qp(icol,k) = max(0. , qp(icol,k) );
        q (icol,k) = q(icol,k) + (dq-qp(icol,k));
      });
    }



    YAKL_INLINE real esatw_crm(real t) {
      real constexpr a0 = 6.105851e0    ;
      real constexpr a1 = 0.4440316e0   ;
      real constexpr a2 = 0.1430341e-1  ;
      real constexpr a3 = 0.2641412e-3  ;
      real constexpr a4 = 0.2995057e-5  ;
      real constexpr a5 = 0.2031998e-7  ;
      real constexpr a6 = 0.6936113e-10 ;
      real constexpr a7 = 0.2564861e-13 ;
      real constexpr a8 = -0.3704404e-15;
      real dt = t-273.16e0;
      if (dt > -80.e0) { return a0 + dt*(a1+dt*(a2+dt*(a3+dt*(a4+dt*(a5+dt*(a6+dt*(a7+a8*dt))))))); }
      else             { return 2.e0*0.01e0*exp(9.550426e0 - 5723.265e0/t + 3.53068e0*log(t) - 0.00728332e0*t); }
      
    }



    YAKL_INLINE real esati_crm(real t) {
      real a0 = 6.11147274e0   ;
      real a1 = 0.503160820e0  ;
      real a2 = 0.188439774e-1 ;
      real a3 = 0.420895665e-3 ;
      real a4 = 0.615021634e-5 ;
      real a5 = 0.602588177e-7 ;
      real a6 = 0.385852041e-9 ;
      real a7 = 0.146898966e-11;
      real a8 = 0.252751365e-14;
      real dt = t-273.16e0;
      if (dt > -80.e0) { return a0 + dt*(a1+dt*(a2+dt*(a3+dt*(a4+dt*(a5+dt*(a6+dt*(a7+a8*dt))))))); }
      else             { return 0.01e0*exp(9.550426e0 - 5723.265e0/t + 3.53068e0*log(t) - 0.00728332e0*t); }
    }



    YAKL_INLINE real qsatw_crm(real t, real p) {
      real esat_crm = esatw_crm(t);
      return 0.622 * esat_crm/max(esat_crm,p-esat_crm);
    }



    YAKL_INLINE real qsati_crm(real t, real p) {
      real esat_crm=esati_crm(t);
      return 0.622 * esat_crm/max(esat_crm,p-esat_crm);
    }


    YAKL_INLINE real dtqsatw_crm(real t, real p) {
      return 0.622*dtesatw_crm(t)/p;
    }



    YAKL_INLINE real dtqsati_crm(real t, real p) {
      return 0.622*dtesati_crm(t)/p;
    }



    YAKL_INLINE real dtesatw_crm(real t) {
      real a0 = 0.443956472e0;
      real a1 = 0.285976452e-1;
      real a2 = 0.794747212e-3;
      real a3 = 0.121167162e-4;
      real a4 = 0.103167413e-6;
      real a5 = 0.385208005e-9;
      real a6 = -0.604119582e-12;
      real a7 = -0.792933209e-14;
      real a8 = -0.599634321e-17;
      real dt = t-273.16;
      if (dt > -80.) { return a0 + dt* (a1+dt*(a2+dt*(a3+dt*(a4+dt*(a5+dt*(a6+dt*(a7+a8*dt))))))); }
      else           { return esatw_crm(t+1.)-esatw_crm(t); }
    }



    YAKL_INLINE real dtesati_crm(real t) {
      real a0 = 0.503223089e0  ;
      real a1 = 0.377174432e-1 ;
      real a2 = 0.126710138e-2 ;
      real a3 = 0.249065913e-4 ;
      real a4 = 0.312668753e-6 ;
      real a5 = 0.255653718e-8 ;
      real a6 = 0.132073448e-10;
      real a7 = 0.390204672e-13;
      real a8 = 0.497275778e-16;
      real dt = t-273.16;
      if (dt > -80.) { return a0 + dt*(a1+dt*(a2+dt*(a3+dt*(a4+dt*(a5+dt*(a6+dt*(a7+a8*dt))))))); }
      else           { return esati_crm(t+1.)-esati_crm(t); }
    }



  };


}


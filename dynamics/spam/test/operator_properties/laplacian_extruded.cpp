// clang-format off
#include "extruded_common.h"
#include "hodge_star.h"
#include "ext_deriv.h"
// clang-format on

struct fun {
  real YAKL_INLINE operator()(real x, real z) const {
    real sx = sin(2 * M_PI * x);
    real cz = cos(M_PI * z);
    return sx * cz;
  }
};

struct lap_fun {
  real YAKL_INLINE operator()(real x, real z) const {
    real sx = sin(2 * M_PI * x);
    real cz = cos(M_PI * z);
    return -5 * M_PI * M_PI * sx * cz;
  }
};

struct vecfun {
  vecext<2> YAKL_INLINE operator()(real x, real z) const {
    real sx = sin(2 * M_PI * x);
    real sz = sin(M_PI * z);
    real cx = cos(2 * M_PI * x);
    real cz = cos(M_PI * z);

    vecext<2> vvec;
    vvec.u = sx * cz;
    vvec.w = cx * sz;
    return vvec;
  }
};

struct lap_vecfun {
  vecext<2> YAKL_INLINE operator()(real x, real z) const {
    real sx = sin(2 * M_PI * x);
    real sz = sin(M_PI * z);
    real cx = cos(2 * M_PI * x);
    real cz = cos(M_PI * z);

    vecext<2> vvec;
    vvec.u = -5 * M_PI * M_PI * sx * cz;
    vvec.w = -5 * M_PI * M_PI * cx * sz;
    return vvec;
  }
};

template <int diff_ord, int vert_diff_ord>
real compute_straight_00form_laplacian_error(int np) {
  ExtrudedUnitSquare square(np, 2 * np);

  auto st00 = square.create_straight_form<0, 0>();
  square.primal_geometry.set_00form_values(fun{}, st00, 0);

  auto lap_st00 = square.create_straight_form<0, 0>();
  auto lap_st00_expected = square.create_straight_form<0, 0>();
  square.primal_geometry.set_00form_values(lap_fun{}, lap_st00_expected, 0);

  int pis = square.primal_topology.is;
  int pjs = square.primal_topology.js;
  int pks = square.primal_topology.ks;

  int dis = square.dual_topology.is;
  int djs = square.dual_topology.js;
  int dks = square.dual_topology.ks;

  {
    st00.exchange();

    auto tmp_st10 = square.create_straight_form<1, 0>();
    parallel_for(
        SimpleBounds<3>(square.primal_topology.ni,
                        square.primal_topology.n_cells_y,
                        square.primal_topology.n_cells_x),
        YAKL_LAMBDA(int k, int j, int i) {
          compute_D0<1>(tmp_st10.data, st00.data, pis, pjs, pks, i, j, k, 0);
        });

    tmp_st10.exchange();
    auto tmp_st01 = square.create_straight_form<0, 1>();
    parallel_for(
        SimpleBounds<3>(square.primal_topology.nl,
                        square.primal_topology.n_cells_y,
                        square.primal_topology.n_cells_x),
        YAKL_LAMBDA(int k, int j, int i) {
          compute_D0_vert<1>(tmp_st01.data, st00.data, pis, pjs, pks, i, j, k,
                             0);
        });
    tmp_st01.exchange();

    auto tmp_tw01 = square.create_twisted_form<0, 1>();
    parallel_for(
        SimpleBounds<3>(square.dual_topology.nl, square.dual_topology.n_cells_y,
                        square.dual_topology.n_cells_x),
        YAKL_LAMBDA(int k, int j, int i) {
          compute_H1_ext<1, diff_ord>(
              tmp_tw01.data, tmp_st10.data, square.primal_geometry,
              square.dual_geometry, dis, djs, dks, i, j, k, 0);
        });
    tmp_tw01.exchange();

    auto tmp_tw10 = square.create_twisted_form<1, 0>();
    parallel_for(
        SimpleBounds<3>(square.dual_topology.ni - 2,
                        square.dual_topology.n_cells_y,
                        square.dual_topology.n_cells_x),
        YAKL_LAMBDA(int k, int j, int i) {
          compute_H1_vert<1, diff_ord>(
              tmp_tw10.data, tmp_st01.data, square.primal_geometry,
              square.dual_geometry, dis, djs, dks, i, j, k + 1, 0);
        });
    tmp_tw10.exchange();
    // BC
    tmp_tw10.set_bnd(0);

    auto tmp_tw11 = square.create_twisted_form<1, 1>();
    parallel_for(
        SimpleBounds<3>(square.dual_topology.nl, square.dual_topology.n_cells_y,
                        square.dual_topology.n_cells_x),
        YAKL_LAMBDA(int k, int j, int i) {
          compute_D1bar<1>(tmp_tw11.data, tmp_tw01.data, dis, djs, dks, i, j, k,
                           0);
          compute_D1bar_vert<1, ADD_MODE::ADD>(tmp_tw11.data, tmp_tw10.data,
                                               dis, djs, dks, i, j, k, 0);
        });
    tmp_tw11.exchange();

    parallel_for(
        SimpleBounds<3>(square.primal_topology.ni,
                        square.primal_topology.n_cells_y,
                        square.primal_topology.n_cells_x),
        YAKL_LAMBDA(int k, int j, int i) {
          compute_H2bar_ext<1, diff_ord, vert_diff_ord>(
              lap_st00.data, tmp_tw11.data, square.primal_geometry,
              square.dual_geometry, pis, pjs, pks, i, j, k, 0);
        });
  }

  real errf = square.compute_Linf_error(lap_st00_expected, lap_st00);
  return errf;
}

template <int diff_ord, int vert_diff_ord>
real compute_vel_laplacian_error(int np) {
  ExtrudedUnitSquare square(np, 2 * np);

  auto st10 = square.create_straight_form<1, 0>();
  auto st01 = square.create_straight_form<0, 1>();
  square.primal_geometry.set_10form_values(vecfun{}, st10, 0,
                                           LINE_INTEGRAL_TYPE::TANGENT);
  square.primal_geometry.set_01form_values(vecfun{}, st01, 0,
                                           LINE_INTEGRAL_TYPE::TANGENT);

  auto lap_st10 = square.create_straight_form<1, 0>();
  auto lap_st01 = square.create_straight_form<0, 1>();
  auto lap_st10_expected = square.create_straight_form<1, 0>();
  auto lap_st01_expected = square.create_straight_form<0, 1>();
  square.primal_geometry.set_10form_values(lap_vecfun{}, lap_st10_expected, 0,
                                           LINE_INTEGRAL_TYPE::TANGENT);
  square.primal_geometry.set_01form_values(lap_vecfun{}, lap_st01_expected, 0,
                                           LINE_INTEGRAL_TYPE::TANGENT);

  int pis = square.primal_topology.is;
  int pjs = square.primal_topology.js;
  int pks = square.primal_topology.ks;

  int dis = square.dual_topology.is;
  int djs = square.dual_topology.js;
  int dks = square.dual_topology.ks;

  {
    st10.exchange();
    st01.exchange();

    // *d*d
    auto tmp_st11 = square.create_straight_form<1, 1>();
    parallel_for(
        SimpleBounds<3>(square.primal_topology.nl,
                        square.primal_topology.n_cells_y,
                        square.primal_topology.n_cells_x),
        YAKL_LAMBDA(int k, int j, int i) {
          compute_D1_ext<1>(tmp_st11.data, st10.data, st01.data, pis, pjs, pks,
                            i, j, k, 0);
        });
    tmp_st11.exchange();

    auto tmp_tw00 = square.create_twisted_form<0, 0>();
    parallel_for(
        SimpleBounds<3>(square.dual_topology.ni - 2,
                        square.dual_topology.n_cells_y,
                        square.dual_topology.n_cells_x),
        YAKL_LAMBDA(int k, int j, int i) {
          compute_H2_ext<1, diff_ord, vert_diff_ord>(
              tmp_tw00.data, tmp_st11.data, square.primal_geometry,
              square.dual_geometry, dis, djs, dks, i, j, k + 1, 0);
        });
    tmp_tw00.exchange();
    tmp_tw00.set_bnd(0);

    auto tmp_tw10 = square.create_twisted_form<1, 0>();
    parallel_for(
        SimpleBounds<3>(square.dual_topology.ni, square.dual_topology.n_cells_y,
                        square.dual_topology.n_cells_x),
        YAKL_LAMBDA(int k, int j, int i) {
          compute_D0bar_ext<1>(tmp_tw10.data, tmp_tw00.data, dis, djs, dks, i,
                               j, k, 0);
        });
    tmp_tw10.exchange();

    auto tmp_tw01 = square.create_twisted_form<0, 1>();
    parallel_for(
        SimpleBounds<3>(square.dual_topology.nl, square.dual_topology.n_cells_y,
                        square.dual_topology.n_cells_x),
        YAKL_LAMBDA(int k, int j, int i) {
          compute_D0bar_vert<1>(tmp_tw01.data, tmp_tw00.data, dis, djs, dks, i,
                                j, k, 0);
        });
    tmp_tw01.exchange();

    parallel_for(
        SimpleBounds<3>(square.primal_topology.ni,
                        square.primal_topology.n_cells_y,
                        square.primal_topology.n_cells_x),
        YAKL_LAMBDA(int k, int j, int i) {
          compute_H1bar_ext<1, diff_ord>(
              lap_st10.data, tmp_tw01.data, square.primal_geometry,
              square.dual_geometry, pis, pjs, pks, i, j, k, 0);
        });

    parallel_for(
        SimpleBounds<3>(square.primal_topology.nl,
                        square.primal_topology.n_cells_y,
                        square.primal_topology.n_cells_x),
        YAKL_LAMBDA(int k, int j, int i) {
          compute_H1bar_vert<1, vert_diff_ord>(
              lap_st01.data, tmp_tw10.data, square.primal_geometry,
              square.dual_geometry, pis, pjs, pks, i, j, k, 0);
        });

    // d*d*
    parallel_for(
        SimpleBounds<3>(square.dual_topology.nl, square.dual_topology.n_cells_y,
                        square.dual_topology.n_cells_x),
        YAKL_LAMBDA(int k, int j, int i) {
          compute_H1_ext<1, diff_ord>(
              tmp_tw01.data, st10.data, square.primal_geometry,
              square.dual_geometry, dis, djs, dks, i, j, k, 0);
        });
    tmp_tw01.exchange();

    parallel_for(
        SimpleBounds<3>(square.dual_topology.ni - 2,
                        square.dual_topology.n_cells_y,
                        square.dual_topology.n_cells_x),
        YAKL_LAMBDA(int k, int j, int i) {
          compute_H1_vert<1, vert_diff_ord>(
              tmp_tw10.data, st01.data, square.primal_geometry,
              square.dual_geometry, dis, djs, dks, i, j, k + 1, 0);
        });
    tmp_tw10.exchange();
    tmp_tw10.set_bnd(0);

    auto tmp_tw11 = square.create_twisted_form<1, 1>();
    parallel_for(
        SimpleBounds<3>(square.dual_topology.nl, square.dual_topology.n_cells_y,
                        square.dual_topology.n_cells_x),
        YAKL_LAMBDA(int k, int j, int i) {
          compute_D1bar<1>(tmp_tw11.data, tmp_tw01.data, dis, djs, dks, i, j, k,
                           0);
          compute_D1bar_vert<1, ADD_MODE::ADD>(tmp_tw11.data, tmp_tw10.data,
                                               dis, djs, dks, i, j, k, 0);
        });
    tmp_tw11.exchange();

    auto tmp_st00 = square.create_straight_form<0, 0>();
    parallel_for(
        SimpleBounds<3>(square.primal_topology.ni,
                        square.primal_topology.n_cells_y,
                        square.primal_topology.n_cells_x),
        YAKL_LAMBDA(int k, int j, int i) {
          compute_H2bar_ext<1, diff_ord, vert_diff_ord>(
              tmp_st00.data, tmp_tw11.data, square.primal_geometry,
              square.dual_geometry, pis, pjs, pks, i, j, k, 0);
        });
    tmp_st00.exchange();

    parallel_for(
        SimpleBounds<3>(square.primal_topology.ni,
                        square.primal_topology.n_cells_y,
                        square.primal_topology.n_cells_x),
        YAKL_LAMBDA(int k, int j, int i) {
          compute_D0<1, ADD_MODE::ADD>(lap_st10.data, tmp_st00.data, pis, pjs,
                                       pks, i, j, k, 0);
        });

    parallel_for(
        SimpleBounds<3>(square.primal_topology.nl,
                        square.primal_topology.n_cells_y,
                        square.primal_topology.n_cells_x),
        YAKL_LAMBDA(int k, int j, int i) {
          compute_D0_vert<1, ADD_MODE::ADD>(lap_st01.data, tmp_st00.data, pis,
                                            pjs, pks, i, j, k, 0);
        });
  }

  real errf_st10 = square.compute_Linf_error(lap_st10_expected, lap_st10);
  real errf_st01 = square.compute_Linf_error(lap_st01_expected, lap_st01);
  return errf_st10 + errf_st01;
}

template <int diff_ord, int vert_diff_ord>
real compute_twisted_11form_laplacian_error(int np) {
  ExtrudedUnitSquare square(np, 2 * np);

  auto tw11 = square.create_twisted_form<1, 1>();
  square.dual_geometry.set_11form_values(fun{}, tw11, 0);

  auto lap_tw11 = square.create_twisted_form<1, 1>();
  auto lap_tw11_expected = square.create_twisted_form<1, 1>();
  square.dual_geometry.set_11form_values(lap_fun{}, lap_tw11_expected, 0);

  int pis = square.primal_topology.is;
  int pjs = square.primal_topology.js;
  int pks = square.primal_topology.ks;

  int dis = square.dual_topology.is;
  int djs = square.dual_topology.js;
  int dks = square.dual_topology.ks;

  {
    tw11.exchange();

    auto tmp_st00 = square.create_straight_form<0, 0>();
    parallel_for(
        SimpleBounds<3>(square.primal_topology.ni,
                        square.primal_topology.n_cells_y,
                        square.primal_topology.n_cells_x),
        YAKL_LAMBDA(int k, int j, int i) {
          compute_H2bar_ext<1, diff_ord, vert_diff_ord>(
              tmp_st00.data, tw11.data, square.primal_geometry,
              square.dual_geometry, pis, pjs, pks, i, j, k, 0);
        });
    tmp_st00.exchange();

    auto tmp_st10 = square.create_straight_form<1, 0>();
    parallel_for(
        SimpleBounds<3>(square.primal_topology.ni,
                        square.primal_topology.n_cells_y,
                        square.primal_topology.n_cells_x),
        YAKL_LAMBDA(int k, int j, int i) {
          compute_D0<1>(tmp_st10.data, tmp_st00.data, pis, pjs, pks, i, j, k,
                        0);
        });
    tmp_st10.exchange();

    auto tmp_st01 = square.create_straight_form<0, 1>();
    parallel_for(
        SimpleBounds<3>(square.primal_topology.nl,
                        square.primal_topology.n_cells_y,
                        square.primal_topology.n_cells_x),
        YAKL_LAMBDA(int k, int j, int i) {
          compute_D0_vert<1>(tmp_st01.data, tmp_st00.data, pis, pjs, pks, i, j,
                             k, 0);
        });
    tmp_st01.exchange();

    auto tmp_tw01 = square.create_twisted_form<0, 1>();
    parallel_for(
        SimpleBounds<3>(square.dual_topology.nl, square.dual_topology.n_cells_y,
                        square.dual_topology.n_cells_x),
        YAKL_LAMBDA(int k, int j, int i) {
          compute_H1_ext<1, diff_ord>(
              tmp_tw01.data, tmp_st10.data, square.primal_geometry,
              square.dual_geometry, dis, djs, dks, i, j, k, 0);
        });
    tmp_tw01.exchange();

    auto tmp_tw10 = square.create_twisted_form<1, 0>();
    parallel_for(
        SimpleBounds<3>(square.dual_topology.ni - 2,
                        square.dual_topology.n_cells_y,
                        square.dual_topology.n_cells_x),
        YAKL_LAMBDA(int k, int j, int i) {
          compute_H1_vert<1, diff_ord>(
              tmp_tw10.data, tmp_st01.data, square.primal_geometry,
              square.dual_geometry, dis, djs, dks, i, j, k + 1, 0);
        });
    tmp_tw10.exchange();
    // BC
    tmp_tw10.set_bnd(0);

    parallel_for(
        SimpleBounds<3>(square.dual_topology.nl, square.dual_topology.n_cells_y,
                        square.dual_topology.n_cells_x),
        YAKL_LAMBDA(int k, int j, int i) {
          compute_D1bar<1>(lap_tw11.data, tmp_tw01.data, dis, djs, dks, i, j, k,
                           0);
          compute_D1bar_vert<1, ADD_MODE::ADD>(lap_tw11.data, tmp_tw10.data,
                                               dis, djs, dks, i, j, k, 0);
        });
  }

  real errf = square.compute_Linf_error(lap_tw11_expected, lap_tw11);
  return errf;
}

void test_laplacian_convergence() {
  const int nlevels = 5;
  const real atol = 0.1;

  {
    const int diff_ord = 2;
    const int vert_diff_ord = 2;
    auto conv_st00 = ConvergenceTest<nlevels>(
        "straight 00 form laplacian",
        compute_straight_00form_laplacian_error<diff_ord, vert_diff_ord>);
    conv_st00.check_rate(vert_diff_ord, atol);
  }

  {
    const int diff_ord = 2;
    const int vert_diff_ord = 2;
    auto conv_vel = ConvergenceTest<nlevels>(
        "velocity laplacian",
        compute_vel_laplacian_error<diff_ord, vert_diff_ord>);
    conv_vel.check_rate(vert_diff_ord, atol);
  }

  {
    const int diff_ord = 2;
    const int vert_diff_ord = 2;
    auto conv_tw11 = ConvergenceTest<nlevels>(
        "twisted 11 form laplacian",
        compute_twisted_11form_laplacian_error<diff_ord, vert_diff_ord>);
    conv_tw11.check_rate(vert_diff_ord, atol);
  }
}

int main() {
  yakl::init();
  test_laplacian_convergence();
  yakl::finalize();
}

// Copyright (C) 2008-2017 Ed Bueler, Constantine Khroulev, Ricarda Winkelmann,
// Gudfinna Adalgeirsdottir, Andy Aschwanden and Torsten Albrecht
//
// This file is part of PISM.
//
// PISM is free software; you can redistribute it and/or modify it under the
// terms of the GNU General Public License as published by the Free Software
// Foundation; either version 3 of the License, or (at your option) any later
// version.
//
// PISM is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
// details.
//
// You should have received a copy of the GNU General Public License
// along with PISM; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

#include <gsl/gsl_math.h>

#include "ConstantPIK.hh"
#include "pism/util/Vars.hh"
#include "pism/util/ConfigInterface.hh"
#include "pism/util/IceGrid.hh"
#include "pism/util/iceModelVec.hh"
#include "pism/util/pism_options.hh"
#include "pism/util/io/io_helpers.hh"
#include "pism/util/MaxTimestep.hh"
#include "pism/util/pism_utilities.hh"

namespace pism {
namespace ocean {

PIK::PIK(IceGrid::ConstPtr g)
  : OceanModel(g) {
  m_meltfactor = m_config->get_double("ocean.pik_melt_factor");
}

PIK::~PIK() {
  // empty
}

void PIK::init_impl() {

  m_t = m_dt = GSL_NAN;  // every re-init restarts the clock

  m_log->message(2,
             "* Initializing the constant (PIK) ocean model...\n");

  m_meltfactor = options::Real("-meltfactor_pik",
                             "Use as a melt factor as in sub-shelf-melting"
                             " parameterization of [@ref Martinetal2011]",
                             m_meltfactor);
}

MaxTimestep PIK::max_timestep_impl(double t) const {
  (void) t;
  return MaxTimestep("ocean PIK");
}

void PIK::update_impl(double my_t, double my_dt) {
  m_t = my_t;
  m_dt = my_dt;
}

void PIK::sea_level_elevation_impl(double &result) const {
  result = m_sea_level;
}

void PIK::shelf_base_temperature_impl(IceModelVec2S &result) const {
  const double
    T0          = m_config->get_double("constants.fresh_water.melting_point_temperature"), // K
    beta_CC     = m_config->get_double("constants.ice.beta_Clausius_Clapeyron"),
    g           = m_config->get_double("constants.standard_gravity"),
    ice_density = m_config->get_double("constants.ice.density");

  const IceModelVec2S &H = *m_grid->variables().get_2d_scalar("land_ice_thickness");

  IceModelVec::AccessList list{&H, &result};

  for (Points p(*m_grid); p; p.next()) {
    const int i = p.i(), j = p.j();
    const double pressure = ice_density * g * H(i,j); // FIXME task #7297
    // temp is set to melting point at depth
    result(i,j) = T0 - beta_CC * pressure;
  }
}

//! \brief Computes mass flux in [kg m-2 s-1].
/*!
 * Assumes that mass flux is proportional to the shelf-base heat flux.
 */
void PIK::shelf_base_mass_flux_impl(IceModelVec2S &result) const {
  const double
    L                 = m_config->get_double("constants.fresh_water.latent_heat_of_fusion"),
    sea_water_density = m_config->get_double("constants.sea_water.density"),
    ice_density       = m_config->get_double("constants.ice.density"),
    c_p_ocean         = 3974.0, // J/(K*kg), specific heat capacity of ocean mixed layer
    gamma_T           = 1e-4,   // m/s, thermal exchange velocity
    ocean_salinity    = 35.0,   // g/kg
    T_ocean           = units::convert(m_sys, -1.7, "Celsius", "Kelvin");   //Default in PISM-PIK

  //FIXME: gamma_T should be a function of the friction velocity, not a const

  const IceModelVec2S &H = *m_grid->variables().get_2d_scalar("land_ice_thickness");

  IceModelVec::AccessList list{&H, &result};

  for (Points p(*m_grid); p; p.next()) {
    const int i = p.i(), j = p.j();

    // compute T_f(i, j) according to beckmann_goosse03, which has the
    // meaning of the freezing temperature of the ocean water directly
    // under the shelf, (of salinity 35psu) [this is related to the
    // Pressure Melting Temperature, see beckmann_goosse03 eq. 2 for
    // details]
    double
      shelfbaseelev = - (ice_density / sea_water_density) * H(i,j),
      T_f           = 273.15 + (0.0939 -0.057 * ocean_salinity + 7.64e-4 * shelfbaseelev);
    // add 273.15 to convert from Celsius to Kelvin

    // compute ocean_heat_flux according to beckmann_goosse03
    // positive, if T_oc > T_ice ==> heat flux FROM ocean TO ice
    double ocean_heat_flux = m_meltfactor * sea_water_density * c_p_ocean * gamma_T * (T_ocean - T_f); // in W/m^2

    // TODO: T_ocean -> field!

    // shelfbmassflux is positive if ice is freezing on; here it is always negative:
    // same sign as ocean_heat_flux (positive if massflux FROM ice TO ocean)
    result(i,j) = ocean_heat_flux / (L * ice_density); // m s-1

    // convert from [m s-1] to [kg m-2 s-1]:
    result(i,j) *= ice_density;
  }
}

} // end of namespace ocean
} // end of namespace pism
